cmake_minimum_required(VERSION 3.16)
project(JoystickServer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# ── Options ──────────────────────────────────────────────────────────
option(BUILD_ROS2 "Build with ROS2 publisher support" OFF)
option(BUILD_ZMQ "Build with ZMQ publisher support" ON)

# ── Find packages ────────────────────────────────────────────────────
find_package(yaml-cpp REQUIRED)

set(HAS_ROS2 OFF)
set(HAS_ZMQ OFF)

if(BUILD_ROS2)
  find_package(rclcpp QUIET)
  find_package(sensor_msgs QUIET)
  if(rclcpp_FOUND AND sensor_msgs_FOUND)
    message(STATUS "ROS2 found - enabling ROS2 publisher")
    set(HAS_ROS2 ON)
  else()
    message(STATUS "ROS2 not found - building without ROS2 support")
  endif()
endif()

if(BUILD_ZMQ)
  # libzmq (C library) from system
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(libzmq IMPORTED_TARGET libzmq)
  endif()

  if(TARGET PkgConfig::libzmq)
    # Fetch cppzmq header-only library from GitHub
    FetchContent_Declare(
      cppzmq
      GIT_REPOSITORY https://github.com/zeromq/cppzmq.git
      GIT_TAG v4.10.0
      GIT_SHALLOW TRUE
    )
    set(CPPZMQ_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(cppzmq)
    message(STATUS "cppzmq fetched from GitHub - enabling ZMQ publisher")
    set(HAS_ZMQ ON)
  else()
    message(STATUS "libzmq not found - building without ZMQ support")
  endif()
endif()

if(NOT HAS_ROS2 AND NOT HAS_ZMQ)
  message(FATAL_ERROR "Neither ROS2 nor ZMQ found. At least one publisher backend is required.")
endif()

# ── Library: joystick_server_core ────────────────────────────────────
set(CORE_SOURCES
  src/config_parser.cpp
  src/joystick_device.cpp
)

add_library(joystick_server_core STATIC ${CORE_SOURCES})
target_include_directories(joystick_server_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(joystick_server_core PUBLIC yaml-cpp::yaml-cpp)

# ── Main executable ──────────────────────────────────────────────────
set(MAIN_SOURCES
  src/main.cpp
  src/ipublisher.cpp
)

if(HAS_ROS2)
  list(APPEND MAIN_SOURCES src/ros2_publisher.cpp)
endif()

if(HAS_ZMQ)
  list(APPEND MAIN_SOURCES src/zmq_publisher.cpp)
endif()

add_executable(joystick_server ${MAIN_SOURCES})
target_link_libraries(joystick_server PRIVATE joystick_server_core)

if(HAS_ROS2)
  target_compile_definitions(joystick_server PRIVATE HAS_ROS2)
  target_link_libraries(joystick_server PRIVATE
    rclcpp::rclcpp
    sensor_msgs::sensor_msgs_library
  )
endif()

if(HAS_ZMQ)
  target_compile_definitions(joystick_server PRIVATE HAS_ZMQ)
  target_link_libraries(joystick_server PRIVATE cppzmq PkgConfig::libzmq)
endif()

# ── Install ──────────────────────────────────────────────────────────
install(TARGETS joystick_server RUNTIME DESTINATION bin)
install(DIRECTORY config/ DESTINATION share/joystick_server/config)
