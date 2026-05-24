# ── Client library (ZMQ) ─────────────────────────────────────────────
if(HAS_ZMQ)
  add_library(joylink_client STATIC
    src/joylink_client.cpp
  )

  target_include_directories(joylink_client PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  )

  target_link_libraries(joylink_client PUBLIC
    joystick_common
    cppzmq
    PkgConfig::libzmq
  )

  target_compile_definitions(joylink_client PUBLIC ZMQ_BUILD_DRAFT_API=OFF)

  # ── Example executable ─────────────────────────────────────────────
  add_executable(joylink_client_example examples/zmq_subscriber.cpp)
  target_link_libraries(joylink_client_example PRIVATE joylink_client)
endif()
