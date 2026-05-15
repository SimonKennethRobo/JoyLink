#pragma once

#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "joystick_common/types.h"

namespace joystick_common {

// Semantic joystick data with named axes/buttons instead of raw indices.
// Suitable for JSON serialization in ZMQ/DDS publishers.
struct MappedJoystickData {
  std::map<std::string, float> axes;       // "left_stick_x" → 0.5, etc.
  std::map<std::string, int> buttons;      // "A" → 1, "B" → 0, etc.
  uint64_t timestamp_ns = 0;
  std::string frame_id;
};

// Converts raw channel-indexed JoystickData to named MappedJoystickData
// using the button/axis mapping from JoystickConfig.
//
// Usage (client side):
//   auto config = loadConfig("xbox.yaml");
//   JoystickMapper mapper(config);
//   auto mapped = mapper.map(rawData);
//   if (mapped.buttons["A"]) { ... }
//   float lx = mapped.axes["left_stick_x"];
class JoystickMapper {
public:
  JoystickMapper() = default;
  explicit JoystickMapper(const JoystickConfig& config);

  // Convert raw data to mapped data.
  MappedJoystickData map(const JoystickData& raw) const;

  // All logical names defined in the mapping (excluding those set to -1).
  const std::vector<std::string>& buttonNames() const { return button_names_; }
  const std::vector<std::string>& axisNames()   const { return axis_names_; }

private:
  // Build reverse lookup: physical index → logical name
  static std::unordered_map<int, std::string>
    buildInverse(const ButtonMapping& bm);
  static std::unordered_map<int, std::string>
    buildInverse(const AxisMapping& am);

  std::unordered_map<int, std::string> button_inverse_;
  std::unordered_map<int, std::string> axis_inverse_;
  std::vector<std::string> button_names_;
  std::vector<std::string> axis_names_;
};

}  // namespace joystick_common
