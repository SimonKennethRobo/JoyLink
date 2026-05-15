#include "joystick_server/config_parser.h"

#include <yaml-cpp/yaml.h>

#include <iostream>
#include <stdexcept>

namespace joystick_server {

namespace {

int getInt(const YAML::Node& node, const std::string& key, int default_val) {
  if (node && node[key]) {
    return node[key].as<int>();
  }
  return default_val;
}

double getDouble(const YAML::Node& node, const std::string& key, double default_val) {
  if (node && node[key]) {
    return node[key].as<double>();
  }
  return default_val;
}

std::string getStr(const YAML::Node& node, const std::string& key, const std::string& default_val) {
  if (node && node[key]) {
    return node[key].as<std::string>();
  }
  return default_val;
}

void parseButtonMapping(const YAML::Node& node, ButtonMapping& map) {
  if (!node) return;
  map.a           = getInt(node, "a", map.a);
  map.b           = getInt(node, "b", map.b);
  map.x           = getInt(node, "x", map.x);
  map.y           = getInt(node, "y", map.y);
  map.lb          = getInt(node, "lb", map.lb);
  map.rb          = getInt(node, "rb", map.rb);
  map.back        = getInt(node, "back", map.back);
  map.start       = getInt(node, "start", map.start);
  map.left_stick  = getInt(node, "left_stick", map.left_stick);
  map.right_stick = getInt(node, "right_stick", map.right_stick);
  map.guide       = getInt(node, "guide", map.guide);
  map.touchpad    = getInt(node, "touchpad", map.touchpad);
}

void parseAxisMapping(const YAML::Node& node, AxisMapping& map) {
  if (!node) return;
  map.left_stick_x  = getInt(node, "left_stick_x", map.left_stick_x);
  map.left_stick_y  = getInt(node, "left_stick_y", map.left_stick_y);
  map.left_trigger  = getInt(node, "left_trigger", map.left_trigger);
  map.right_stick_x = getInt(node, "right_stick_x", map.right_stick_x);
  map.right_stick_y = getInt(node, "right_stick_y", map.right_stick_y);
  map.right_trigger = getInt(node, "right_trigger", map.right_trigger);
  map.dpad_x        = getInt(node, "dpad_x", map.dpad_x);
  map.dpad_y        = getInt(node, "dpad_y", map.dpad_y);
}

}  // namespace

JoystickConfig loadConfig(const std::string& yaml_path) {
  JoystickConfig config;
  YAML::Node root;

  try {
    root = YAML::LoadFile(yaml_path);
  } catch (const YAML::Exception& e) {
    throw std::runtime_error("Failed to parse YAML config: " + std::string(e.what()));
  }

  // Top-level key: "joystick_server" is optional
  YAML::Node js;
  if (root["joystick_server"]) {
    js = root["joystick_server"];
  } else {
    js = root;  // Allow flat config without enclosing key
  }

  if (js.IsNull()) {
    throw std::runtime_error("Empty or missing configuration node");
  }

  // ── Device ──────────────────────────────────────────
  config.device      = getStr(js, "device", config.device);
  config.device_name = getStr(js, "device_name", config.device_name);

  // ── Processing ──────────────────────────────────────
  config.deadzone           = getDouble(js, "deadzone", config.deadzone);
  config.sticky_buttons     = js["sticky_buttons"] ? js["sticky_buttons"].as<bool>() : false;
  config.publish_rate       = getDouble(js, "publish_rate", config.publish_rate);
  config.coalesce_interval  = getDouble(js, "coalesce_interval", config.coalesce_interval);
  config.frame_id           = getStr(js, "frame_id", config.frame_id);

  // ── Publisher ───────────────────────────────────────
  std::string pub_type = getStr(js, "publisher_type", "ros2");
  if (pub_type == "zmq") {
    config.publisher_type = PublisherType::ZMQ;
  } else {
    config.publisher_type = PublisherType::ROS2;
  }

  // ROS2 settings
  if (js["ros2"]) {
    config.ros2_topic = getStr(js["ros2"], "topic", config.ros2_topic);
  } else {
    config.ros2_topic = getStr(js, "topic", config.ros2_topic);  // shorthand
  }

  // ZMQ settings
  if (js["zmq"]) {
    config.zmq_address = getStr(js["zmq"], "address", config.zmq_address);
  }

  // ── Button mapping ──────────────────────────────────
  parseButtonMapping(js["button_mapping"], config.button_map);

  // ── Axis mapping ────────────────────────────────────
  parseAxisMapping(js["axes_mapping"], config.axis_map);

  return config;
}

void JoystickConfig::print() const {
  const char* pub_name = (publisher_type == PublisherType::ROS2) ? "ROS2" : "ZMQ";
  std::cout << "╔══════════════════════════════════════════════╗\n"
            << "║         Joystick Server Configuration        ║\n"
            << "╠══════════════════════════════════════════════╣\n"
            << "║ Device         : " << device << "\n";
  if (!device_name.empty())
    std::cout << "║ Device name    : " << device_name << "\n";
  std::cout << "║ Publisher      : " << pub_name << "\n"
            << "║ ROS2 topic     : " << ros2_topic << "\n"
            << "║ ZMQ address    : " << zmq_address << "\n"
            << "║ Publish rate   : " << publish_rate << " Hz\n"
            << "║ Deadzone       : " << deadzone << "\n"
            << "║ Coalesce(ms)   : " << coalesce_interval << "\n"
            << "║ Sticky buttons : " << (sticky_buttons ? "on" : "off") << "\n"
            << "║ Frame ID       : " << frame_id << "\n"
            << "╠══════════════════════════════════════════════╣\n"
            << "║ Button Mapping:                             ║\n"
            << "║   A=" << button_map.a
            << " B=" << button_map.b
            << " X=" << button_map.x
            << " Y=" << button_map.y << "\n"
            << "║   LB=" << button_map.lb
            << " RB=" << button_map.rb
            << " BACK=" << button_map.back
            << " START=" << button_map.start << "\n"
            << "║   LS=" << button_map.left_stick
            << " RS=" << button_map.right_stick
            << " GUIDE=" << button_map.guide << "\n"
            << "╠══════════════════════════════════════════════╣\n"
            << "║ Axis Mapping:                               ║\n"
            << "║   LX=" << axis_map.left_stick_x
            << " LY=" << axis_map.left_stick_y
            << " LT=" << axis_map.left_trigger << "\n"
            << "║   RX=" << axis_map.right_stick_x
            << " RY=" << axis_map.right_stick_y
            << " RT=" << axis_map.right_trigger << "\n"
            << "║   DPAD_X=" << axis_map.dpad_x
            << " DPAD_Y=" << axis_map.dpad_y << "\n"
            << "╚══════════════════════════════════════════════╝\n"
            << std::endl;
}

}  // namespace joystick_server
