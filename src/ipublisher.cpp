#include "joystick_server/ipublisher.h"

#include <memory>
#include <stdexcept>

#ifdef HAS_ROS2
#include "joystick_server/ros2_publisher.h"
#endif
#ifdef HAS_ZMQ
#include "joystick_server/zmq_publisher.h"
#endif

namespace joystick_server {

std::unique_ptr<IPublisher> createPublisher(const JoystickConfig& config) {
  switch (config.publisher_type) {
    case PublisherType::ROS2: {
#ifdef HAS_ROS2
      return std::make_unique<Ros2Publisher>();
#else
      throw std::runtime_error("ROS2 support not compiled in. Rebuild with BUILD_ROS2=ON.");
#endif
    }
    case PublisherType::ZMQ: {
#ifdef HAS_ZMQ
      return std::make_unique<ZmqPublisher>();
#else
      throw std::runtime_error("ZMQ support not compiled in. Rebuild with BUILD_ZMQ=ON.");
#endif
    }
  }
  throw std::runtime_error("Unknown publisher type");
}

}  // namespace joystick_server
