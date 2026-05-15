# ── Client library (ZMQ) ─────────────────────────────────────────────
if(HAS_ZMQ)
  add_library(joystick_client STATIC
    src/joystick_client.cpp
  )

  target_include_directories(joystick_client PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  )

  target_link_libraries(joystick_client PUBLIC
    joystick_common
    cppzmq
    PkgConfig::libzmq
  )

  target_compile_definitions(joystick_client PUBLIC ZMQ_BUILD_DRAFT_API=OFF)

  # ── Example executable ─────────────────────────────────────────────
  add_executable(joystick_client_example examples/zmq_subscriber.cpp)
  target_link_libraries(joystick_client_example PRIVATE joystick_client)
endif()
