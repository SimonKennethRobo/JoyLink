#pragma once

#include <memory>
#include <string>

#include "joystick_common/joystick_mapper.h"

namespace joystick_client {

// High-level client for receiving mapped joystick data.
//
// Usage:
//   JoystickClient client("config/xbox.yaml");
//   while (true) {
//       joystick_common::MappedJoystickData data;
//       if (client.receive(data)) {
//           float lx = data.axes["left_stick_x"];
//           bool  a  = data.buttons["a"];
//       }
//   }
class JoystickClient {
public:
  // Load config from YAML. Throws std::runtime_error on failure.
  explicit JoystickClient(const std::string& yaml_path);

  ~JoystickClient();

  JoystickClient(const JoystickClient&) = delete;
  JoystickClient& operator=(const JoystickClient&) = delete;

  // ── Connection ──────────────────────────────────────────
  // Connect to the server (address from YAML). Call once after construction.
  // Returns true on success.
  bool connect();

  // Disconnect.
  void disconnect();

  // True if connected.
  bool isConnected() const;

  // ── Data access ─────────────────────────────────────────
  // Blocking receive with timeout (ms). -1 = block forever, 0 = non-blocking.
  // Returns true if data was received.
  bool receive(joystick_common::MappedJoystickData& out, int timeout_ms = 1000);

  // Receive raw channel-indexed data (before mapping).
  bool receiveRaw(joystick_common::JoystickData& out, int timeout_ms = 1000);

  // ── Accessors ───────────────────────────────────────────
  const joystick_common::JoystickConfig& config() const;
  const joystick_common::JoystickMapper& mapper() const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace joystick_client
