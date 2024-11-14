/******************************************************************************
Copyright (c) 2021, Farbod Farshidian. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

 * Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

 * Neither the name of the copyright holder nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
******************************************************************************/

#include "ament_index_cpp/get_package_share_directory.hpp"
#include "rclcpp/rclcpp.hpp"

#include <ocs2_centroidal_model/CentroidalModelPinocchioMapping.h>
#include <ocs2_legged_robot/LeggedRobotInterface.h>
#include <ocs2_pinocchio_interface/PinocchioEndEffectorKinematics.h>
#include <ocs2_ros2_interfaces/mrt/MRT_ROS_Dummy_Loop.h>
#include <ocs2_ros2_interfaces/mrt/MRT_ROS_Interface.h>

#include "ocs2_legged_robot_ros/visualization/LeggedRobotVisualizer.h"

using namespace ocs2;
using namespace legged_robot;

int main(int argc, char** argv) {
  const std::string robotName = "legged_robot";

  std::vector<std::string> programArgs = rclcpp::remove_ros_arguments(argc, argv);
  if (programArgs.size() <= 1) {
    throw std::runtime_error("No task file specified. Aborting.");
  }
  std::string taskFileFolderName = std::string(programArgs[1]);
  std::string referenceFileFolderName = std::string(programArgs[2]);

  // Initialize ros node
  rclcpp::init(argc, argv);
  rclcpp::Node::SharedPtr nodeHandle = rclcpp::Node::make_shared(robotName + "_mrt");

  // Get node parameters
  const std::string taskFile =
      ament_index_cpp::get_package_share_directory("ocs2_legged_robot") + "/config/" + taskFileFolderName + "/task.info";
  const std::string referenceFile =
      ament_index_cpp::get_package_share_directory("ocs2_legged_robot") + "/config/" + referenceFileFolderName + "/reference.info";
  const std::string urdfFile = ament_index_cpp::get_package_share_directory("ocs2_legged_robot_ros") + "/urdf/urdf/anymal.urdf";

  // Robot interface
  LeggedRobotInterface interface(taskFile, urdfFile, referenceFile);

  // MRT
  MRT_ROS_Interface mrt(robotName);
  mrt.initRollout(&interface.getRollout());
  mrt.launchNodes(nodeHandle, rclcpp::QoS(1));

  // Visualization
  CentroidalModelPinocchioMapping pinocchioMapping(interface.getCentroidalModelInfo());
  PinocchioEndEffectorKinematics endEffectorKinematics(interface.getPinocchioInterface(), pinocchioMapping,
                                                       interface.modelSettings().contactNames3DoF);
  auto leggedRobotVisualizer = std::make_shared<LeggedRobotVisualizer>(
      interface.getPinocchioInterface(), interface.getCentroidalModelInfo(), endEffectorKinematics, nodeHandle);

  // Dummy legged robot
  MRT_ROS_Dummy_Loop leggedRobotDummySimulator(mrt, interface.mpcSettings().mrtDesiredFrequency_,
                                               interface.mpcSettings().mpcDesiredFrequency_);
  leggedRobotDummySimulator.subscribeObservers({leggedRobotVisualizer});

  // Initial state
  SystemObservation initObservation;
  initObservation.state = interface.getInitialState();
  initObservation.input = vector_t::Zero(interface.getCentroidalModelInfo().inputDim);
  initObservation.mode = ModeNumber::STANCE;

  // Initial command
  TargetTrajectories initTargetTrajectories({0.0}, {initObservation.state}, {initObservation.input});

  // run dummy
  leggedRobotDummySimulator.run(initObservation, initTargetTrajectories);

  // Successful exit
  return 0;
}
