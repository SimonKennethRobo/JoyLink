#include "joystick_server/zmq_publisher.h"

#include <zmq.hpp>

#include <iomanip>
#include <iostream>
#include <sstream>

namespace joystick_server {

struct ZmqPublisher::Impl {
  zmq::context_t ctx;
  zmq::socket_t sock;
  std::string frame_id;

  Impl() : ctx(1), sock(ctx, zmq::socket_type::pub) {}
};

ZmqPublisher::ZmqPublisher() = default;
ZmqPublisher::~ZmqPublisher() = default;

bool ZmqPublisher::init(const JoystickConfig& config) {
  impl_ = std::make_unique<Impl>();
  impl_->frame_id = config.frame_id;
  impl_->sock.set(zmq::sockopt::linger, 0);

  try {
    impl_->sock.bind(config.zmq_address);
    std::cout << "ZmqPublisher: bound to " << config.zmq_address << std::endl;
  } catch (const zmq::error_t& e) {
    std::cerr << "ZmqPublisher: bind failed: " << e.what() << std::endl;
    return false;
  }
  return true;
}

void ZmqPublisher::publish(const JoystickData& data) {
  if (!impl_) return;

  std::ostringstream json;
  json << std::fixed << std::setprecision(4);
  json << "{\"frame_id\":\"" << impl_->frame_id << "\""
       << ",\"ts_ns\":" << data.timestamp_ns
       << ",\"axes\":[";

  for (size_t i = 0; i < data.axes.size(); ++i) {
    if (i > 0) json << ",";
    json << data.axes[i];
  }

  json << "],\"buttons\":[";
  for (size_t i = 0; i < data.buttons.size(); ++i) {
    if (i > 0) json << ",";
    json << data.buttons[i];
  }
  json << "]}";

  std::string body = json.str();

  // Multi-part PUB-SUB: topic frame, then data frame
  impl_->sock.send(zmq::const_buffer(impl_->frame_id.data(), impl_->frame_id.size()),
                   zmq::send_flags::sndmore);
  impl_->sock.send(zmq::const_buffer(body.data(), body.size()), zmq::send_flags::none);
}

}  // namespace joystick_server
