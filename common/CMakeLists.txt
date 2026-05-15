# Shared types, config parser, and mapper used by both server and client.

add_library(joystick_common STATIC
  src/config_parser.cpp
  src/joystick_mapper.cpp
)

target_include_directories(joystick_common PUBLIC
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
)

target_link_libraries(joystick_common PUBLIC yaml-cpp::yaml-cpp)
