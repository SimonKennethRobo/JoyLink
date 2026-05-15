#pragma once

#include <memory>

#include "joystick_common/types.h"

namespace joystick_server {

using joystick_common::JoystickConfig;
using joystick_common::JoystickData;
using joystick_common::PublisherType;

// Abstract interface for publishing joystick data.
// Implementations: Ros2Publisher, ZmqPublisher, DdsPublisher.
class IPublisher {
public:
  virtual ~IPublisher() = default;

  // Initialize the publisher. Returns true on success.
  virtual bool init(const JoystickConfig& config) = 0;

  // Publish a data frame. Thread-safe with respect to other publish() calls.
  virtual void publish(const JoystickData& data) = 0;

  // Called periodically by the main loop. ROS2 publisher uses this to spin.
  virtual void spinOnce() {}
};

// Factory function - creates the appropriate publisher based on config.
std::unique_ptr<IPublisher> createPublisher(const JoystickConfig& config);

}  // namespace joystick_server
