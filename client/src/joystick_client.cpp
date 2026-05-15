#include "joystick_client/joystick_client.h"

#include <zmq.hpp>

#include <chrono>
#include <iostream>
#include <sstream>
#include <thread>

#include "joystick_common/config_parser.h"

namespace joystick_client {

struct JoystickClient::Impl {
  joystick_common::JoystickConfig cfg;
  joystick_common::JoystickMapper mapper;
  zmq::context_t ctx;
  zmq::socket_t sock;
  bool connected = false;

  Impl(const std::string& yaml_path)
    : cfg(joystick_common::loadConfig(yaml_path))
    , mapper(cfg)
    , ctx(1)
    , sock(ctx, zmq::socket_type::sub)
  {}
};

JoystickClient::JoystickClient(const std::string& yaml_path) {
  impl_ = std::make_unique<Impl>(yaml_path);
  impl_->cfg.print();
}

JoystickClient::~JoystickClient() { disconnect(); }

bool JoystickClient::connect() {
  if (impl_->connected) return true;

  if (impl_->cfg.publisher_type != joystick_common::PublisherType::ZMQ) {
    std::cerr << "JoystickClient: only ZMQ publisher is supported" << std::endl;
    return false;
  }

  try {
    // Convert bind address (tcp://*:5555) to connect address (tcp://localhost:5555)
    std::string addr = impl_->cfg.zmq_address;
    auto star = addr.find("//*");
    if (star != std::string::npos) {
      addr.replace(star + 2, 1, "localhost");
    }

    impl_->sock.set(zmq::sockopt::subscribe, "");
    impl_->sock.set(zmq::sockopt::rcvtimeo, -1);
    impl_->sock.connect(addr);
    impl_->connected = true;
    std::cout << "JoystickClient: connected to " << addr << std::endl;
  } catch (const zmq::error_t& e) {
    std::cerr << "JoystickClient: connect failed: " << e.what() << std::endl;
    return false;
  }
  return true;
}

void JoystickClient::disconnect() {
  if (!impl_->connected) return;

  try {
    impl_->sock.set(zmq::sockopt::linger, 0);
    impl_->sock.close();
  } catch (...) {}
  impl_->connected = false;
}

bool JoystickClient::isConnected() const { return impl_->connected; }

bool JoystickClient::receive(joystick_common::MappedJoystickData& out, int timeout_ms) {
  joystick_common::JoystickData raw;
  if (!receiveRaw(raw, timeout_ms)) return false;

  out = impl_->mapper.map(raw);
  out.frame_id = impl_->cfg.frame_id;
  return true;
}

bool JoystickClient::receiveRaw(joystick_common::JoystickData& out, int timeout_ms) {
  if (!impl_->connected) return false;

  // Set timeout for this receive
  impl_->sock.set(zmq::sockopt::rcvtimeo, timeout_ms);

  zmq::message_t topic_msg, body_msg;
  try {
    auto ret = impl_->sock.recv(topic_msg);
    if (!ret) return false;
  } catch (const zmq::error_t&) {
    return false;
  }

  try {
    auto ret = impl_->sock.recv(body_msg);
    if (!ret) return false;
  } catch (const zmq::error_t&) {
    return false;
  }

  // Parse JSON body
  std::string body(static_cast<const char*>(body_msg.data()), body_msg.size());

  // Minimal JSON parser for the expected format:
  // {"frame_id":"...","ts_ns":...,"axes":[...],"buttons":[...]}
  auto findValue = [&](const std::string& key) -> std::string {
    auto pos = body.find("\"" + key + "\"");
    if (pos == std::string::npos) return {};
    pos = body.find(":", pos);
    if (pos == std::string::npos) return {};
    pos++;
    while (pos < body.size() && (body[pos] == ' ' || body[pos] == '\t')) pos++;
    if (body[pos] == '[') {
      auto end = body.find("]", pos);
      if (end == std::string::npos) return {};
      return body.substr(pos, end - pos + 1);
    } else if (body[pos] == '"') {
      auto end = body.find("\"", pos + 1);
      if (end == std::string::npos) return {};
      return body.substr(pos + 1, end - pos - 1);
    } else {
      auto end = body.find_first_of(",}", pos);
      if (end == std::string::npos) return {};
      return body.substr(pos, end - pos);
    }
  };

  out.timestamp_ns = std::stoull(findValue("ts_ns"));

  // Parse axes array
  std::string axes_str = findValue("axes");
  if (!axes_str.empty() && axes_str[0] == '[') {
    out.axes.clear();
    std::istringstream ss(axes_str.substr(1, axes_str.size() - 2));
    std::string token;
    while (std::getline(ss, token, ',')) {
      out.axes.push_back(std::stof(token));
    }
  }

  // Parse buttons array
  std::string buttons_str = findValue("buttons");
  if (!buttons_str.empty() && buttons_str[0] == '[') {
    out.buttons.clear();
    std::istringstream ss(buttons_str.substr(1, buttons_str.size() - 2));
    std::string token;
    while (std::getline(ss, token, ',')) {
      out.buttons.push_back(std::stoi(token));
    }
  }

  return true;
}

const joystick_common::JoystickConfig& JoystickClient::config() const {
  return impl_->cfg;
}

const joystick_common::JoystickMapper& JoystickClient::mapper() const {
  return impl_->mapper;
}

}  // namespace joystick_client
