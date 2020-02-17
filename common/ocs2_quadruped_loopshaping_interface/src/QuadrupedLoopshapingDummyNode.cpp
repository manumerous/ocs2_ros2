//
// Created by rgrandia on 17.02.20.
//

#include "ocs2_quadruped_loopshaping_interface/QuadrupedLoopshapingDummyNode.h"

#include <ocs2_comm_interfaces/ocs2_ros_interfaces/mrt/MRT_ROS_Dummy_Loop.h>
#include <ocs2_comm_interfaces/ocs2_ros_interfaces/mrt/MRT_ROS_Interface.h>

#include <ocs2_quadruped_loopshaping_interface/QuadrupedLoopshapingInterface.h>
#include <ocs2_quadruped_loopshaping_interface/QuadrupedLoopshapingXppVisualizer.h>

namespace switched_model {

void quadrupedLoopshapingDummyNode(ros::NodeHandle& nodeHandle, const QuadrupedLoopshapingInterface& quadrupedInterface) {
  static constexpr size_t STATE_DIM = 48;
  static constexpr size_t INPUT_DIM = 24;
  static constexpr size_t JOINT_DIM = 12;
  const std::string robotName = "anymal";
  using vis_t = switched_model::QuadrupedXppVisualizer;
  using vis_wrapper_t = switched_model::QuadrupedLoopshapingXppVisualizer;
  using mrt_t = ocs2::MRT_ROS_Interface<STATE_DIM, INPUT_DIM>;
  using dummy_t = ocs2::MRT_ROS_Dummy_Loop<STATE_DIM, INPUT_DIM>;

  // MRT
  mrt_t mrt(robotName);
  mrt.initRollout(&quadrupedInterface.getRollout());
  mrt.launchNodes(nodeHandle);

  // Visualization
  std::unique_ptr<vis_t> visualizer(
      new vis_t(quadrupedInterface.getKinematicModel(), quadrupedInterface.getComModel(), robotName, nodeHandle));
  std::shared_ptr<vis_wrapper_t> loopshapingVisualizer(
      new vis_wrapper_t(quadrupedInterface.getLoopshapingDefinition(), std::move(visualizer)));

  // Dummy MRT
  dummy_t dummySimulator(mrt, quadrupedInterface.mpcSettings().mrtDesiredFrequency_, quadrupedInterface.mpcSettings().mpcDesiredFrequency_);
  dummySimulator.subscribeObservers({loopshapingVisualizer});

  // initial state
  mrt_t::system_observation_t initObservation;
  initObservation.state() = quadrupedInterface.getInitialState();
  initObservation.subsystem() = switched_model::ModeNumber::STANCE;

  // initial command
  ocs2::CostDesiredTrajectories initCostDesiredTrajectories({0.0}, {initObservation.state()}, {initObservation.input()});

  // run dummy
  dummySimulator.run(initObservation, initCostDesiredTrajectories);
}

}