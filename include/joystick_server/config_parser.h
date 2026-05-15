#pragma once

#include "joystick_server/types.h"

namespace joystick_server {

// Load configuration from a YAML file. Throws std::runtime_error on failure.
JoystickConfig loadConfig(const std::string& yaml_path);

}  // namespace joystick_server
