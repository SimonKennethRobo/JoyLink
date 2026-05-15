#pragma once

#include <atomic>
#include <chrono>
#include <string>
#include <vector>

#include "joystick_server/types.h"

struct js_event;  // forward: defined in linux/joystick.h

namespace joystick_server {

// Reads joystick events from /dev/input/js* using the Linux joystick API.
//
// Usage:
//   JoystickDevice dev;
//   dev.open(config);
//   while (...) {
//     JoystickEvent ev;
//     if (dev.readEvent(&ev)) { ... }
//   }
//   dev.close();
class JoystickDevice {
public:
  JoystickDevice();
  ~JoystickDevice();

  JoystickDevice(const JoystickDevice&) = delete;
  JoystickDevice& operator=(const JoystickDevice&) = delete;

  // Open the device. Returns true on success.
  bool open(const JoystickConfig& config);

  // Close the device.
  void close();

  // True if the device is open and healthy.
  bool isOpen() const { return fd_ >= 0 && !error_; }

  // Blocking read with timeout. Returns true if an event was read.
  // timeout_ms=0 means non-blocking, -1 means block indefinitely.
  bool readEvent(js_event* event, int timeout_ms = -1);

  // Read joystick capabilities (after open).
  int numButtons() const { return num_buttons_; }
  int numAxes() const { return num_axes_; }
  const std::string& name() const { return name_; }
  int fd() const { return fd_; }

  // Discover a joystick device path by human-readable name.
  // Scans /dev/input/js* and returns the path matching `name`.
  // Returns empty string if not found.
  static std::string findDeviceByName(const std::string& name);

private:
  int fd_ = -1;
  int num_buttons_ = 0;
  int num_axes_ = 0;
  std::string name_;
  std::atomic<bool> error_{false};
};

}  // namespace joystick_server
