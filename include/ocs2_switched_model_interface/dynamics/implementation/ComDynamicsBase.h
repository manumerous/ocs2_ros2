/*
 * ComDynamicsBase.h
 *
 *  Created on: Nov 7, 2017
 *      Author: farbod
 */

namespace switched_model {

/******************************************************************************************************/
/******************************************************************************************************/
/******************************************************************************************************/
template <size_t JOINT_COORD_SIZE>
ComDynamicsBase<JOINT_COORD_SIZE>* ComDynamicsBase<JOINT_COORD_SIZE>::clone() const {

	return new ComDynamicsBase<JOINT_COORD_SIZE>(*this);
}


/******************************************************************************************************/
/******************************************************************************************************/
/******************************************************************************************************/
template <size_t JOINT_COORD_SIZE>
void ComDynamicsBase<JOINT_COORD_SIZE>::initializeModel(logic_rules_machine_t& logicRulesMachine,
		const size_t& partitionIndex, const char* algorithmName/*=NULL*/) {

	Base::initializeModel(logicRulesMachine, partitionIndex, algorithmName);

	if (algorithmName!=NULL)
		algorithmName_.assign(algorithmName);
	else
		algorithmName_.clear();
}


/******************************************************************************************************/
/******************************************************************************************************/
/******************************************************************************************************/
template <size_t JOINT_COORD_SIZE>
void ComDynamicsBase<JOINT_COORD_SIZE>::setData(const std::array<bool,4>& stanceLegs,
		const joint_coordinate_t& qJoints,
		const joint_coordinate_t& dqJoints)  {

	stanceLegs_ = stanceLegs;
	qJoints_  = qJoints;	  // Joints' coordinate
	dqJoints_ = dqJoints;     // Joints' velocity
}


/******************************************************************************************************/
/******************************************************************************************************/
/******************************************************************************************************/
template <size_t JOINT_COORD_SIZE>
void ComDynamicsBase<JOINT_COORD_SIZE>::computeFlowMap(const scalar_t& t,
		const state_vector_t& x,
		const input_vector_t& u,
		state_vector_t& dxdt)   {
	// Rotation matrix from Base frame (or the coincided frame world frame) to Origin frame (global world).
	Eigen::Matrix3d o_R_b = RotationMatrixBasetoOrigin(x.head<3>());

	// base to CoM displacement in the CoM frame
	com_base2CoM_ = comModelPtr_->comPositionBaseFrame(qJoints_);

	// base coordinate
	q_base_.template head<3>() = x.segment<3>(0);
	q_base_.template tail<3>() = x.segment<3>(3) - o_R_b * com_base2CoM_;

	// update kinematic model
	kinematicModelPtr_->update(q_base_, qJoints_);

	// Fiction cone constraint
	input_vector_t u_adapted;
	if (enforceFrictionConeConstraint_){
		u_adapted = u;
		for (size_t i=0; i<4; i++) {
			if (stanceLegs_[i]) {
				scalar_t Fx = u(3 * i + 0);
				scalar_t Fy = u(3 * i + 1);
				scalar_t Fz = std::max(u(3 * i + 2), scalar_t(0.0));
				scalar_t FT = std::sqrt(Fx * Fx + Fy * Fy);
				if (FT > (frictionCoefficient_ * Fz)) {
					Fx *= frictionCoefficient_ * Fz / FT;
					Fy *= frictionCoefficient_ * Fz / FT;
					std::cout << "Friction clamping active" << std::endl;
				}
				u_adapted(3 * i + 0) = Fx;
				u_adapted(3 * i + 1) = Fy;
				u_adapted(3 * i + 2) = Fz;
			}
		}
	} else {
		u_adapted = u;
	}

	// Torque limits
	// assuming tau = -JjT * lambda
	// -tau_lim < - B_Jj^T * b_R_o * u_lambda < tau_lim
	// o_R_b * B_Jj.inverse() * tau_lim < 	u_lambda   < -o_R_b * B_Jj.inverse() * tau_lim
	if (enforceTorqueConstraint_){
		base_jacobian_matrix_t base_jacobian_matrix;
		for (size_t i=0; i<4; i++) {
			if (stanceLegs_[i]) {
				kinematicModelPtr_->footJacobainBaseFrame(i, base_jacobian_matrix);
				matrix3d_t Jj = base_jacobian_matrix.block(3, 3 * i, 3, 3) + 1e-5 * matrix3d_t::Identity();
				vector3d_t torques = -Jj.transpose() * (o_R_b.transpose() * u_adapted.segment(3 * i, 3));
				torques(0) = std::min(std::max(-torqueLimit_, torques(0)), torqueLimit_);
				torques(1) = std::min(std::max(-torqueLimit_, torques(1)), torqueLimit_);
				torques(2) = std::min(std::max(-torqueLimit_, torques(2)), torqueLimit_);
				u_adapted.segment(3 * i, 3) = -o_R_b * (Jj.inverse().transpose() * torques);
			}
		}
	}

	// local angular velocity (com_W_com) and local linear velocity (com_V_com) of CoM
	Eigen::VectorBlock<const state_vector_t,3> com_W_com = x.segment<3>(6);
	Eigen::VectorBlock<const state_vector_t,3> com_V_com = x.segment<3>(9);

	// base to stance feet displacement in the CoM frame
	for (size_t i=0; i<4; i++)
		if (stanceLegs_[i]==true || constrainedIntegration_==false)
			kinematicModelPtr_->footPositionBaseFrame(i, com_base2StanceFeet_[i]);
		else
			com_base2StanceFeet_[i].setZero();

	// Inertia matrix in the CoM frame and its derivatives
	M_    = comModelPtr_->comInertia(qJoints_);
	dMdt_ = comModelPtr_->comInertiaDerivative(qJoints_, dqJoints_);
	Eigen::Matrix3d rotationMInverse = M_.topLeftCorner<3,3>().inverse();
	MInverse_ << rotationMInverse, Eigen::Matrix3d::Zero(),
			Eigen::Matrix3d::Zero(), Eigen::Matrix3d::Identity()/M_(5,5);
//	MInverse_ << M_.topLeftCorner<3,3>().inverse(), Eigen::Matrix3d::Zero(),
//			     Eigen::Matrix3d::Zero(),           (1/M_(5,5))*Eigen::Matrix3d::Identity();

	// Coriolis and centrifugal forces
	C_.head<3>() = com_W_com.cross(M_.topLeftCorner<3,3>()*com_W_com) + dMdt_.topLeftCorner<3,3>()*com_W_com;
	C_.tail<3>().setZero();

	// gravity effect on CoM in CoM coordinate
	MInverseG_ << Eigen::Vector3d::Zero(), -o_R_b.transpose() * o_gravityVector_;

	// CoM Jacobian in the Base frame
	b_comJacobainOmega_ = ( MInverse_*comModelPtr_->comMomentumJacobian(qJoints_) ).template topRows<3>();

	// contact JacobianTransposeLambda
	Eigen::Vector3d com_comToFoot;
	Eigen::Matrix<double, 6, 1> JcTransposeLambda(Eigen::VectorXd::Zero(6));
	for (size_t i=0; i<4; i++)  {

		// for a swing leg skip the followings
		if (stanceLegs_[i]==false && constrainedIntegration_==true)  continue;

		com_comToFoot  = com_base2StanceFeet_[i]-com_base2CoM_;

		JcTransposeLambda.head<3>() += com_comToFoot.cross(u_adapted.segment<3>(3*i));
		JcTransposeLambda.tail<3>() += u_adapted.segment<3>(3*i);
	}

	// angular velocities to Euler angle derivatives transformation
	Eigen::Matrix3d transformAngVel2EulerAngDev = AngularVelocitiesToEulerAngleDerivativesMatrix(x.head<3>());

	// CoM dynamics
//	dxdt.segment<3>(0) = o_R_b * transformAngVel2EulerAngDev * (com_W_com - b_comJacobainOmega_*dqJoints_);
	dxdt.segment<3>(0) = transformAngVel2EulerAngDev * (com_W_com - b_comJacobainOmega_*dqJoints_);
	dxdt.segment<3>(3) = o_R_b * com_V_com;
	dxdt.tail<6>() = MInverse_ * (-C_ + JcTransposeLambda) - MInverseG_;

}

/******************************************************************************************************/
/******************************************************************************************************/
/******************************************************************************************************/
/*
 * @brief Computes the matrix which transforms derivatives of angular velocities in the body frame to euler angles derivatives
 * WARNING: matrix is singular when rotation around y axis is +/- 90 degrees
 * @param[in] eulerAngles: euler angles in xyz convention
 * @return M: matrix that does the transformation
 */
template <size_t JOINT_COORD_SIZE>
Eigen::Matrix3d ComDynamicsBase<JOINT_COORD_SIZE>::AngularVelocitiesToEulerAngleDerivativesMatrix (const Eigen::Vector3d& eulerAngles){

	Eigen::Matrix<double,3,3> M;
	double sinPsi = sin(eulerAngles(2));
	double cosPsi = cos(eulerAngles(2));
	double sinTheta = sin(eulerAngles(1));
	double cosTheta = cos(eulerAngles(1));

	M << 	cosPsi/cosTheta,          -sinPsi/cosTheta,          0,
			sinPsi, 				   cosPsi,                   0,
		   -cosPsi*sinTheta/cosTheta,  sinTheta*sinPsi/cosTheta, 1;

	return M;
}

/******************************************************************************************************/
/******************************************************************************************************/
/******************************************************************************************************/
template <size_t JOINT_COORD_SIZE>
void ComDynamicsBase<JOINT_COORD_SIZE>::getStanceLegs(std::array<bool,4>& stanceLegs) const {
	stanceLegs = stanceLegs_;
}

/******************************************************************************************************/
/******************************************************************************************************/
/******************************************************************************************************/
template <size_t JOINT_COORD_SIZE>
void ComDynamicsBase<JOINT_COORD_SIZE>::getFeetPositions(std::array<Eigen::Vector3d,4>& b_base2StanceFeet) const {
	b_base2StanceFeet = com_base2StanceFeet_;
}

/******************************************************************************************************/
/******************************************************************************************************/
/******************************************************************************************************/
template <size_t JOINT_COORD_SIZE>
void ComDynamicsBase<JOINT_COORD_SIZE>::getBasePose(base_coordinate_t& o_basePose) const {
	o_basePose = q_base_;
}

/******************************************************************************************************/
/******************************************************************************************************/
/******************************************************************************************************/
template <size_t JOINT_COORD_SIZE>
Eigen::Matrix<double, 6, 6>  ComDynamicsBase<JOINT_COORD_SIZE>::getM() const {
	return M_;
}

/******************************************************************************************************/
/******************************************************************************************************/
/******************************************************************************************************/
template <size_t JOINT_COORD_SIZE>
Eigen::Matrix<double, 6, 1>  ComDynamicsBase<JOINT_COORD_SIZE>::getC() const {
	return C_;
}

/******************************************************************************************************/
/******************************************************************************************************/
/******************************************************************************************************/
template <size_t JOINT_COORD_SIZE>
Eigen::Matrix<double, 6, 1>  ComDynamicsBase<JOINT_COORD_SIZE>::getG() const {
	return M_*MInverseG_;
}

/******************************************************************************************************/
/******************************************************************************************************/
/******************************************************************************************************/
template <size_t JOINT_COORD_SIZE>
Eigen::Matrix<double, 6, 6>  ComDynamicsBase<JOINT_COORD_SIZE>::getMInverse() const {
	return MInverse_;
}

} //end of namespace switched_model
