//
// Created by rgrandia on 26.02.20.
//

#include <gtest/gtest.h>

#include "ocs2_qp_solver/Ocs2QpSolver.h"

#include <ocs2_core/cost/QuadraticCostFunction.h>
#include <ocs2_core/dynamics/LinearSystemDynamics.h>

#include "testProblemsGeneration.h"

class Ocs2QpSolverTest : public testing::Test {
 protected:
  static constexpr size_t N = 10;  // Trajectory length
  static constexpr size_t STATE_DIM = 3;
  static constexpr size_t INPUT_DIM = 2;
  using SystemDynamics_t = ocs2::SystemDynamicsBase<STATE_DIM, INPUT_DIM>;
  using costFunction_t = ocs2::CostFunctionBase<STATE_DIM, INPUT_DIM>;
  using input_vector_t = costFunction_t::input_vector_t;
  using state_vector_t = costFunction_t::state_vector_t;

  Ocs2QpSolverTest() {
    srand(0);
    cost = ocs2::qp_solver::getOcs2Cost<STATE_DIM, INPUT_DIM>(ocs2::qp_solver::getRandomCost(STATE_DIM, INPUT_DIM),
                                                              ocs2::qp_solver::getRandomCost(STATE_DIM, INPUT_DIM),
                                                              state_vector_t::Random(), input_vector_t::Random(), state_vector_t::Random());
    system = ocs2::qp_solver::getOcs2Dynamics<STATE_DIM, INPUT_DIM>(ocs2::qp_solver::getRandomDynamics(STATE_DIM, INPUT_DIM));
    linearization = ocs2::qp_solver::getRandomTrajectory(N, STATE_DIM, INPUT_DIM);
    x0 = state_vector_t::Random();
    solution = solveLinearQuadraticOptimalControlProblem(*cost, *system, linearization, x0);
  }

  std::unique_ptr<costFunction_t> cost;
  std::unique_ptr<SystemDynamics_t> system;
  ocs2::qp_solver::ContinuousTrajectory linearization;
  state_vector_t x0;
  ocs2::qp_solver::ContinuousTrajectory solution;
};

TEST_F(Ocs2QpSolverTest, initialCondition) {
  ASSERT_TRUE(x0.isApprox(solution.stateTrajectory.front()));
}

TEST_F(Ocs2QpSolverTest, satisfiesDynamics) {
  // Forward integrate with solution u(t) and check x(t)
  state_vector_t x = x0;
  ocs2::qp_solver::SystemWrapper systemWrapper(*system);
  for (int k = 0; k < N; ++k) {
    double dt = solution.timeTrajectory[k + 1] - solution.timeTrajectory[k];
    x += dt * systemWrapper.getFlowMap(solution.timeTrajectory[k], x, solution.inputTrajectory[k]);
    ASSERT_TRUE(x.isApprox(solution.stateTrajectory[k + 1]));
  }
}

TEST_F(Ocs2QpSolverTest, invariantUnderLinearization) {
  // Different linearization, with same time discretization
  auto linearization2 = ocs2::qp_solver::getRandomTrajectory(N, STATE_DIM, INPUT_DIM);
  linearization2.timeTrajectory = linearization.timeTrajectory;

  // Compare solutions
  auto solution2 = solveLinearQuadraticOptimalControlProblem(*cost, *system, linearization2, x0);
  ASSERT_TRUE(ocs2::qp_solver::isEqual(solution.stateTrajectory, solution2.stateTrajectory));
  ASSERT_TRUE(ocs2::qp_solver::isEqual(solution.inputTrajectory, solution2.inputTrajectory));
}

TEST_F(Ocs2QpSolverTest, knownSolutionAtOrigin) {
  // If the cost's nominal trajectory is set to zero, and the initial state is zero, then the solution has only zeros.
  const auto zeroCost = ocs2::qp_solver::getOcs2Cost<STATE_DIM, INPUT_DIM>(
      ocs2::qp_solver::getRandomCost(STATE_DIM, INPUT_DIM), ocs2::qp_solver::getRandomCost(STATE_DIM, INPUT_DIM), state_vector_t::Zero(),
      input_vector_t::Zero(), state_vector_t::Zero());
  const auto zeroX0 = state_vector_t::Zero();

  // Obtain solution, with non-zero linearization
  auto zeroSolution = solveLinearQuadraticOptimalControlProblem(*zeroCost, *system, linearization, zeroX0);

  std::vector<Eigen::VectorXd> allStatesZero(N + 1, Eigen::VectorXd::Zero(STATE_DIM));
  std::vector<Eigen::VectorXd> allInputsZero(N, Eigen::VectorXd::Zero(INPUT_DIM));
  ASSERT_TRUE(ocs2::qp_solver::isEqual(zeroSolution.stateTrajectory, allStatesZero));
  ASSERT_TRUE(ocs2::qp_solver::isEqual(zeroSolution.inputTrajectory, allInputsZero));
}