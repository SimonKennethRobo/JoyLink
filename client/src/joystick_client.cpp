#include "joystick_client/joystick_client.h"

#include <zmq.hpp>

#include <chrono>
#include <condition_variable>
#include <deque>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>

#include "joystick_common/config_parser.h"

namespace joystick_client {

namespace {

bool parseRawJson(const std::string& body, joystick_common::JoystickData& out) {
  // Minimal JSON parser for the expected format:
  // {"frame_id":"...","ts_ns":...,"axes":[...],"buttons":[...]}
  auto findValue = [&](const std::string& key) -> std::string {
    auto pos = body.find("\"" + key + "\"");
    if (pos == std::string::npos) return {};
    pos = body.find(":", pos);
    if (pos == std::string::npos) return {};
    pos++;
    while (pos < body.size() && (body[pos] == ' ' || body[pos] == '\t')) pos++;
    if (pos >= body.size()) return {};
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

  const std::string ts = findValue("ts_ns");
  if (ts.empty()) return false;
  out.timestamp_ns = std::stoull(ts);

  // Parse axes array
  std::string axes_str = findValue("axes");
  if (!axes_str.empty() && axes_str[0] == '[') {
    out.axes.clear();
    std::istringstream ss(axes_str.substr(1, axes_str.size() - 2));
    std::string token;
    while (std::getline(ss, token, ',')) {
      if (!token.empty()) out.axes.push_back(std::stof(token));
    }
  }

  // Parse buttons array
  std::string buttons_str = findValue("buttons");
  if (!buttons_str.empty() && buttons_str[0] == '[') {
    out.buttons.clear();
    std::istringstream ss(buttons_str.substr(1, buttons_str.size() - 2));
    std::string token;
    while (std::getline(ss, token, ',')) {
      if (!token.empty()) out.buttons.push_back(std::stoi(token));
    }
  }

  return true;
}

}  // namespace

struct JoystickClient::Impl {
  joystick_common::JoystickConfig cfg;
  joystick_common::JoystickMapper mapper;
  zmq::context_t ctx;
  zmq::socket_t sock;
  bool connected = false;
  bool running = false;
  std::thread rx_thread;
  std::mutex queue_mutex;
  std::condition_variable queue_cv;
  std::deque<joystick_common::JoystickData> queue;
  size_t max_queue = 100;

  Impl(const std::string& yaml_path)
    : cfg(joystick_common::loadConfig(yaml_path))
    , mapper(cfg)
    , ctx(1)
    , sock(ctx, zmq::socket_type::sub)
  {}

  void startReceiver() {
    if (running) return;
    running = true;
    rx_thread = std::thread([this]() { recvLoop(); });
  }

  void stopReceiver() {
    if (!running) return;
    running = false;
    if (rx_thread.joinable()) rx_thread.join();
  }

  void recvLoop() {
    sock.set(zmq::sockopt::rcvtimeo, 100);
    while (running) {
      zmq::message_t topic_msg, body_msg;
      try {
        auto ret = sock.recv(topic_msg);
        if (!ret) continue;
      } catch (const zmq::error_t&) {
        if (!running) break;
        continue;
      }

      try {
        auto ret = sock.recv(body_msg);
        if (!ret) continue;
      } catch (const zmq::error_t&) {
        if (!running) break;
        continue;
      }

      std::string body(static_cast<const char*>(body_msg.data()), body_msg.size());
      joystick_common::JoystickData raw;
      if (!parseRawJson(body, raw)) continue;

      {
        std::lock_guard<std::mutex> lock(queue_mutex);
        if (queue.size() >= max_queue) {
          queue.pop_front();
        }
        queue.push_back(std::move(raw));
      }
      queue_cv.notify_one();
    }
  }
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
    impl_->sock.connect(addr);
    impl_->connected = true;
    impl_->startReceiver();
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
    impl_->stopReceiver();
    impl_->sock.set(zmq::sockopt::linger, 0);
    impl_->sock.close();
  } catch (...) {}
  {
    std::lock_guard<std::mutex> lock(impl_->queue_mutex);
    impl_->queue.clear();
  }
  impl_->connected = false;
  impl_->queue_cv.notify_all();
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

  std::unique_lock<std::mutex> lock(impl_->queue_mutex);
  if (timeout_ms < 0) {
    impl_->queue_cv.wait(lock, [&]() { return !impl_->queue.empty() || !impl_->connected; });
  } else if (timeout_ms == 0) {
    if (impl_->queue.empty()) return false;
  } else {
    impl_->queue_cv.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                             [&]() { return !impl_->queue.empty() || !impl_->connected; });
  }

  if (impl_->queue.empty()) return false;
  out = std::move(impl_->queue.front());
  impl_->queue.pop_front();
  return true;
}

const joystick_common::JoystickConfig& JoystickClient::config() const {
  return impl_->cfg;
}

const joystick_common::JoystickMapper& JoystickClient::mapper() const {
  return impl_->mapper;
}

}  // namespace joystick_client
