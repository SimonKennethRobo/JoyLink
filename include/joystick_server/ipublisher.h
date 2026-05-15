#pragma once

#include <memory>

#include "joystick_server/types.h"

namespace joystick_server {

// Abstract interface for publishing joystick data.
// Implementations: Ros2Publisher, ZmqPublisher.
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
