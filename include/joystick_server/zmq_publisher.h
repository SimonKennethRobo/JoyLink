#pragma once

#include <memory>

#include "joystick_server/ipublisher.h"

namespace joystick_server {

class ZmqPublisher : public IPublisher {
public:
  ZmqPublisher();
  ~ZmqPublisher() override;

  bool init(const JoystickConfig& config) override;
  void publish(const JoystickData& data) override;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace joystick_server
