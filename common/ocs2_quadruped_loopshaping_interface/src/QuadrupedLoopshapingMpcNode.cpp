//
// Created by rgrandia on 17.02.20.
//

#include "ocs2_quadruped_loopshaping_interface/QuadrupedLoopshapingMpcNode.h"

#include <ocs2_comm_interfaces/ocs2_ros_interfaces/mpc/MPC_ROS_Interface.h>
#include <ocs2_switched_model_interface/logic/GaitReceiver.h>

#include <ocs2_quadruped_loopshaping_interface/QuadrupedLoopshapingSlqMpc.h>

namespace switched_model_loopshaping {

void quadrupedLoopshapingMpcNode(ros::NodeHandle& nodeHandle, const QuadrupedLoopshapingInterface& quadrupedInterface,
                                 const ocs2::MPC_Settings& mpcSettings, const ocs2::SLQ_Settings& slqSettings) {
  const std::string robotName = "anymal";
  using mpc_ros_t = ocs2::MPC_ROS_Interface<STATE_DIM, INPUT_DIM>;

  auto gaitReceiver = std::make_shared<switched_model::GaitReceiver>(
      nodeHandle, quadrupedInterface.getModeScheduleManagerPtr()->modeScheduleManager_->getGaitSchedule(), robotName);
  auto loopshapingSolverModule = quadrupedInterface.getLoopshapingSynchronizedModule();
  loopshapingSolverModule->synchronizedModules_.push_back(gaitReceiver);

  // launch MPC nodes
  auto mpcPtr = getMpc(quadrupedInterface, mpcSettings, slqSettings);
  mpcPtr->getSolverPtr()->setSynchronizedModules({loopshapingSolverModule});
  mpc_ros_t mpcNode(*mpcPtr, robotName);
  mpcNode.launchNodes(nodeHandle);
}

}  // namespace switched_model_loopshaping
