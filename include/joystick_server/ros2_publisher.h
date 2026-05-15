#pragma once

#include <memory>

#include "joystick_server/ipublisher.h"

namespace joystick_server {

class Ros2Publisher : public IPublisher {
public:
  Ros2Publisher();
  ~Ros2Publisher() override;

  bool init(const JoystickConfig& config) override;
  void publish(const JoystickData& data) override;
  void spinOnce() override;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace joystick_server
