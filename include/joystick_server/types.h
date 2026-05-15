#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace joystick_server {

// ── Joystick data published to consumers ───────────────────────────
struct JoystickData {
  std::vector<float> axes;       // Normalized axis values [-1.0, 1.0]
  std::vector<int32_t> buttons;  // Button states (0 or 1)
  uint64_t timestamp_ns;         // Monotonic timestamp in nanoseconds
};

// ── Axis / button index configuration ──────────────────────────────
// Physical indices on different controllers vary; these structs let
// users map logical names to physical indices per device.

struct ButtonMapping {
  int a = 0;
  int b = 1;
  int x = 2;
  int y = 3;
  int lb = 4;
  int rb = 5;
  int back = 6;
  int start = 7;
  int left_stick = 9;
  int right_stick = 10;
  int guide = 8;
  int touchpad = -1;  // PS4 only, disabled by default
};

struct AxisMapping {
  int left_stick_x = 0;
  int left_stick_y = 1;
  int left_trigger = 2;
  int right_stick_x = 3;
  int right_stick_y = 4;
  int right_trigger = 5;
  int dpad_x = 6;
  int dpad_y = 7;
};

// ── Full server configuration ──────────────────────────────────────
enum class PublisherType { ROS2, ZMQ };

struct JoystickConfig {
  // Device
  std::string device = "/dev/input/js0";
  std::string device_name;  // If non-empty, auto-discover by name

  // Mapping
  ButtonMapping button_map;
  AxisMapping axis_map;

  // Processing
  double deadzone = 0.05;
  bool sticky_buttons = false;
  double publish_rate = 50.0;     // Hz
  double coalesce_interval = 0.001;  // seconds, 0=disabled

  // Publisher
  PublisherType publisher_type = PublisherType::ROS2;
  std::string ros2_topic = "joy";
  std::string zmq_address = "tcp://*:5555";
  std::string frame_id = "joy";

  // Print loaded config to stdout
  void print() const;
};

}  // namespace joystick_server
