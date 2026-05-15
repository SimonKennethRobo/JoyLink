#pragma once

#include "joystick_common/types.h"

namespace joystick_common {

// Load configuration from a YAML file. Throws std::runtime_error on failure.
JoystickConfig loadConfig(const std::string& yaml_path);

}  // namespace joystick_common
