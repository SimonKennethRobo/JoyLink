# ── Server library: joylink_core ────────────────────────────────────
# Server-only device reader and private server headers.

add_library(joylink_core STATIC
  ${CMAKE_SOURCE_DIR}/server/src/joystick_device.cpp
)

target_include_directories(joylink_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/server/include>
)

target_link_libraries(joylink_core PUBLIC joystick_common)

# ── Server executable: joylink ───────────────────────────────────────

set(SRV "${CMAKE_SOURCE_DIR}/server/src")

set(SERVER_SOURCES
  ${SRV}/main.cpp
  ${SRV}/ipublisher.cpp
)

if(HAS_ROS2)
  list(APPEND SERVER_SOURCES ${SRV}/ros2_publisher.cpp)
endif()

if(HAS_ZMQ)
  list(APPEND SERVER_SOURCES ${SRV}/zmq_publisher.cpp)
endif()

if(HAS_DDS)
  list(APPEND SERVER_SOURCES ${SRV}/dds_publisher.cpp)
endif()

add_executable(joylink ${SERVER_SOURCES})

target_link_libraries(joylink PRIVATE joylink_core)

if(HAS_ROS2)
  target_compile_definitions(joylink PRIVATE HAS_ROS2)
  target_link_libraries(joylink PRIVATE
    rclcpp::rclcpp
    sensor_msgs::sensor_msgs_library
  )
endif()

if(HAS_ZMQ)
  target_compile_definitions(joylink PRIVATE HAS_ZMQ)
  target_link_libraries(joylink PRIVATE cppzmq PkgConfig::libzmq)
endif()

if(HAS_DDS)
  # Generate C++ types from IDL
  idlcxx_generate(
    TARGET _joydata
    FILES ${CMAKE_SOURCE_DIR}/idl/JoystickData.idl
    DEFAULT_EXTENSIBILITY final
  )

  target_compile_definitions(joylink PRIVATE HAS_DDS)
  target_link_libraries(joylink PRIVATE _joydata CycloneDDS-CXX::ddscxx)
endif()

install(TARGETS joylink RUNTIME DESTINATION bin)
