#include "joystick_common/joystick_mapper.h"

namespace joystick_common {

namespace {

template <typename M>
std::unordered_map<int, std::string> buildButtonInverse(const M& bm) {
  std::unordered_map<int, std::string> inv;
  auto add = [&](int idx, const char* name) {
    if (idx >= 0) {
      inv[idx] = name;
    }
  };
  add(bm.a, "a");
  add(bm.b, "b");
  add(bm.x, "x");
  add(bm.y, "y");
  add(bm.lb, "lb");
  add(bm.rb, "rb");
  add(bm.back, "back");
  add(bm.start, "start");
  add(bm.left_stick, "left_stick");
  add(bm.right_stick, "right_stick");
  add(bm.guide, "guide");
  add(bm.touchpad, "touchpad");
  return inv;
}

template <typename M>
std::unordered_map<int, std::string> buildAxisInverse(const M& am) {
  std::unordered_map<int, std::string> inv;
  auto add = [&](int idx, const char* name) {
    if (idx >= 0) {
      inv[idx] = name;
    }
  };
  add(am.left_stick_x, "left_stick_x");
  add(am.left_stick_y, "left_stick_y");
  add(am.left_trigger, "left_trigger");
  add(am.right_stick_x, "right_stick_x");
  add(am.right_stick_y, "right_stick_y");
  add(am.right_trigger, "right_trigger");
  add(am.dpad_x, "dpad_x");
  add(am.dpad_y, "dpad_y");
  return inv;
}

}  // namespace

JoystickMapper::JoystickMapper(const JoystickConfig& config)
  : button_inverse_(buildButtonInverse(config.button_map))
  , axis_inverse_(buildAxisInverse(config.axis_map))
  , button_invert_(config.button_invert.begin(), config.button_invert.end())
  , axis_reverse_(config.axis_reverse.begin(), config.axis_reverse.end())
{
  // Collect sorted names for iteration
  for (const auto& [idx, name] : button_inverse_) {
    button_names_.push_back(name);
  }
  for (const auto& [idx, name] : axis_inverse_) {
    axis_names_.push_back(name);
  }
}

MappedJoystickData JoystickMapper::map(const JoystickData& raw) const {
  MappedJoystickData out;
  out.timestamp_ns = raw.timestamp_ns;

  for (const auto& [idx, name] : button_inverse_) {
    int val = (idx < static_cast<int>(raw.buttons.size()))
                  ? raw.buttons[idx]
                  : 0;
    if (button_invert_.count(idx)) val = 1 - val;
    out.buttons[name] = val;
  }

  for (const auto& [idx, name] : axis_inverse_) {
    float val = (idx < static_cast<int>(raw.axes.size()))
                    ? raw.axes[idx]
                    : 0.0f;
    if (axis_reverse_.count(idx)) val = -val;
    out.axes[name] = val;
  }

  return out;
}

// static
std::unordered_map<int, std::string>
JoystickMapper::buildInverse(const ButtonMapping& bm) {
  return buildButtonInverse(bm);
}

std::unordered_map<int, std::string>
JoystickMapper::buildInverse(const AxisMapping& am) {
  return buildAxisInverse(am);
}

}  // namespace joystick_common
