cmake_minimum_required(VERSION 3.10.2)
project(ocs2_pinocchio_interface)

# Generate compile_commands.json for clang tools
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default to C++17
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(ament_index_cpp REQUIRED)
find_package(ocs2_core REQUIRED)
find_package(ocs2_robotic_tools REQUIRED)
find_package(ocs2_oc REQUIRED)
find_package(ocs2_thirdparty REQUIRED)
find_package(OpenMP REQUIRED)
find_package(urdf REQUIRED)
find_package(urdfdom REQUIRED)

find_package(Boost REQUIRED COMPONENTS
  system
  filesystem
  log
  log_setup
)

find_package(pinocchio REQUIRED)

find_package(Eigen3 3.3 REQUIRED NO_MODULE)

###########
## Build ##
###########

include_directories(
  include
  ${EIGEN3_INCLUDE_DIRS}
  ${pinocchio_INCLUDE_DIRS}
)

set(OCS2_PINOCCHIO_FLAGS
  ${OCS2_CXX_FLAGS}
  ${pinocchio_CFLAGS_OTHER}
  -Wno-ignored-attributes
  -DPINOCCHIO_URDFDOM_TYPEDEF_SHARED_PTR
  -DPINOCCHIO_URDFDOM_USE_STD_SHARED_PTR
)

# ocs2 pinocchio interface library
add_library(${PROJECT_NAME}
  src/PinocchioInterface.cpp
  src/PinocchioInterfaceCppAd.cpp
  src/PinocchioEndEffectorKinematics.cpp
  src/PinocchioEndEffectorKinematicsCppAd.cpp
  src/urdf.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

ament_target_dependencies(${PROJECT_NAME}
  pinocchio
  ocs2_core
  ocs2_oc
  ocs2_robotic_tools
  urdf
  urdfdom
)

target_link_libraries(${PROJECT_NAME}
  pinocchio::pinocchio
)

target_compile_options(${PROJECT_NAME} PUBLIC ${OCS2_PINOCCHIO_FLAGS})

####################
## Clang tooling ###
####################

find_package(cmake_clang_tools QUIET)
if (cmake_clang_tools_FOUND)
  message(STATUS "Run clang tooling")
  add_clang_tooling(
    TARGETS ${PROJECT_NAME}
    SOURCE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/include
    CT_HEADER_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include
    CF_WERROR
  )
endif (cmake_clang_tools_FOUND)

#############
## Install ##
#############

install(TARGETS ${PROJECT_NAME}
        EXPORT export_${PROJECT_NAME}
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin
)

install(DIRECTORY include/
        DESTINATION include
)

ament_export_include_directories(include)
ament_export_libraries(${PROJECT_NAME})
ament_export_targets(
  export_${PROJECT_NAME}
)

ament_export_dependencies(ocs2_core)
ament_export_dependencies(ocs2_robotic_tools)
ament_export_dependencies(urdf)
ament_export_dependencies(urdfdom)

############
# Testing ##
############

#catkin_add_gtest(testPinocchioInterface
#  test/testPinocchioInterface.cpp
#  test/testPinocchioEndEffectorKinematics.cpp
#)
#target_link_libraries(testPinocchioInterface
#  gtest_main
#  ${PROJECT_NAME}
#)

ament_package()
