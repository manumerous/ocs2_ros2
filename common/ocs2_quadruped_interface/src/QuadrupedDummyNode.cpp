//
// Created by rgrandia on 17.02.20.
//

#include "ocs2_quadruped_interface/QuadrupedDummyNode.h"

#include <ocs2_comm_interfaces/ocs2_ros_interfaces/mrt/MRT_ROS_Dummy_Loop.h>
#include <ocs2_comm_interfaces/ocs2_ros_interfaces/mrt/MRT_ROS_Interface.h>

#include <ocs2_quadruped_interface/QuadrupedInterface.h>
#include <ocs2_quadruped_interface/QuadrupedXppVisualizer.h>

namespace switched_model {

void quadrupedDummyNode(ros::NodeHandle& nodeHandle, const QuadrupedInterface& quadrupedInterface) {
  static constexpr size_t STATE_DIM = 24;
  static constexpr size_t INPUT_DIM = 24;
  static constexpr size_t JOINT_DIM = 12;
  const std::string robotName = "anymal";
  using vis_t = switched_model::QuadrupedXppVisualizer;
  using mrt_t = ocs2::MRT_ROS_Interface<STATE_DIM, INPUT_DIM>;
  using dummy_t = ocs2::MRT_ROS_Dummy_Loop<STATE_DIM, INPUT_DIM>;

  // MRT
  mrt_t mrt(robotName);
  mrt.initRollout(&quadrupedInterface.getRollout());
  mrt.launchNodes(nodeHandle);

  // Visualization
  std::shared_ptr<vis_t> visualizer(new vis_t(quadrupedInterface.getKinematicModel(), quadrupedInterface.getComModel(), robotName, nodeHandle));

  // Dummy MRT
  dummy_t dummySimulator(mrt, quadrupedInterface.mpcSettings().mrtDesiredFrequency_, quadrupedInterface.mpcSettings().mpcDesiredFrequency_);
  dummySimulator.subscribeObservers({visualizer});

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