#include <algorithm>
#include <chrono>
#include <cmath>
#include <csignal>
#include <iomanip>
#include <iostream>
#include <string>
#include <unordered_set>

#include <fcntl.h>
#include <linux/joystick.h>
#include <unistd.h>

#include "joystick_common/config_parser.h"
#include "joystick_server/ipublisher.h"
#include "joystick_server/joystick_device.h"

#ifdef HAS_ROS2
#include <rclcpp/rclcpp.hpp>
#endif

using namespace joystick_server;
using namespace joystick_common;

namespace {

std::atomic<bool> g_running{true};

void signalHandler(int) { g_running = false; }

void printUsage(const char* prog) {
  std::cerr << "Usage: " << prog << " <config.yaml>\n"
            << "  config.yaml  Path to YAML configuration file\n"
            << "\nExample:\n"
            << "  " << prog << " config/xbox.yaml\n";
}

float applyDeadzone(float raw, float deadzone) {
  if (std::fabs(raw) < deadzone) return 0.0f;
  float sign = (raw > 0) ? 1.0f : -1.0f;
  return sign * (std::fabs(raw) - deadzone) / (1.0f - deadzone);
}

}  // namespace

int main(int argc, char* argv[]) {
  if (argc < 2) {
    printUsage(argv[0]);
    return 1;
  }

  std::string config_path = argv[1];

  // ── Load configuration ──────────────────────────────────────────────
  JoystickConfig config;
  try {
    config = loadConfig(config_path);
  } catch (const std::exception& e) {
    std::cerr << "Error loading config: " << e.what() << std::endl;
    return 1;
  }
  config.print();

  // ── Init ROS2 if needed ──────────────────────────────────────────────
#ifdef HAS_ROS2
  if (config.publisher_type == PublisherType::ROS2) {
    rclcpp::init(argc, argv);
  }
#endif

  // ── Create publisher ─────────────────────────────────────────────────
  auto publisher = createPublisher(config);
  if (!publisher->init(config)) {
    std::cerr << "Failed to initialize publisher" << std::endl;
    return 1;
  }

  // ── Open joystick device ─────────────────────────────────────────────
  JoystickDevice device;
  if (!device.open(config)) {
    std::cerr << "Failed to open joystick device: " << config.device << std::endl;
    return 1;
  }

  // ── Signal handling ──────────────────────────────────────────────────
  std::signal(SIGINT, signalHandler);
  std::signal(SIGTERM, signalHandler);

  // ── Initialise state ─────────────────────────────────────────────────
  JoystickData data;
  data.axes.resize(device.numAxes(), 0.0f);
  data.buttons.resize(device.numButtons(), 0);

  // Build lookup sets for reverse/invert
  std::unordered_set<int> axis_rev_set(config.axis_reverse.begin(), config.axis_reverse.end());
  std::unordered_set<int> btn_inv_set(config.button_invert.begin(), config.button_invert.end());

  const int64_t publish_interval_us = static_cast<int64_t>(1'000'000.0 / config.publish_rate);
  const int64_t coalesce_interval_us = static_cast<int64_t>(config.coalesce_interval * 1'000'000.0);

  auto last_publish_time = std::chrono::steady_clock::now();
  bool dirty = false;
  auto dirty_since = std::chrono::steady_clock::now();

  // Stats
  int event_count = 0, pub_count = 0;
  auto stats_time = std::chrono::steady_clock::now();

  // Debug raw: track previous state to detect changes
  std::vector<float>    prev_axes;
  std::vector<int32_t>  prev_buttons;
  auto debug_last_print = std::chrono::steady_clock::now();
  if (config.debug_raw) {
    prev_axes = data.axes;
    prev_buttons = data.buttons;
  }

  std::cout << "\nJoystick server running. Press Ctrl+C to stop.\n" << std::endl;

  // ── Main loop ────────────────────────────────────────────────────────
  while (g_running) {
    publisher->spinOnce();

    // Determine select timeout
    auto now = std::chrono::steady_clock::now();
    int64_t since_publish_us =
        std::chrono::duration_cast<std::chrono::microseconds>(now - last_publish_time).count();

    int64_t delay_us = publish_interval_us - since_publish_us;
    if (dirty && coalesce_interval_us > 0) {
      int64_t since_dirty_us =
          std::chrono::duration_cast<std::chrono::microseconds>(now - dirty_since).count();
      int64_t coalesce_remain = coalesce_interval_us - since_dirty_us;
      if (coalesce_remain < delay_us) delay_us = coalesce_remain;
    }
    if (delay_us < 0) delay_us = 0;
    int select_timeout_ms = static_cast<int>(delay_us / 1000);

    // Read events
    js_event ev;
    if (device.readEvent(&ev, select_timeout_ms)) {
      event_count++;

      switch (ev.type & ~JS_EVENT_INIT) {
        case JS_EVENT_BUTTON:
          if (ev.number < static_cast<int>(data.buttons.size())) {
            int val = ev.value;
            if (btn_inv_set.count(ev.number)) val = 1 - val;
            if (config.sticky_buttons) {
              if (val == 1)
                data.buttons[ev.number] = 1 - data.buttons[ev.number];
            } else {
              data.buttons[ev.number] = (val ? 1 : 0);
            }
          }
          if (!(ev.type & JS_EVENT_INIT)) {
            dirty = true;
            dirty_since = now;
          }
          break;

        case JS_EVENT_AXIS:
          if (ev.number < static_cast<int>(data.axes.size())) {
            float raw = static_cast<float>(ev.value) / 32767.0f;
            raw = applyDeadzone(raw, static_cast<float>(config.deadzone));
            if (axis_rev_set.count(ev.number)) raw = -raw;
            data.axes[ev.number] = std::clamp(raw, -1.0f, 1.0f);
          }
          dirty = true;
          dirty_since = now;
          break;
      }
    }

    // Publish if it's time (coalesce or autorepeat)
    {
      now = std::chrono::steady_clock::now();
      since_publish_us =
          std::chrono::duration_cast<std::chrono::microseconds>(now - last_publish_time).count();

      bool publish_now = false;
      if (dirty && coalesce_interval_us > 0) {
        // Waiting for batch window to close after first change
        int64_t since_dirty_us =
            std::chrono::duration_cast<std::chrono::microseconds>(now - dirty_since).count();
        if (since_dirty_us >= coalesce_interval_us) publish_now = true;
      } else if (since_publish_us >= publish_interval_us) {
        publish_now = true;
      }

      if (publish_now) {
        data.timestamp_ns =
            std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
        publisher->publish(data);
        pub_count++;
        dirty = false;
        last_publish_time = now;
      }
    }

    // Periodic stats
    now = std::chrono::steady_clock::now();
    auto elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - stats_time).count();
    if (elapsed_ms >= 5000) {
      std::cout << "[stats] events=" << event_count << " ("
                << event_count * 1000.0 / elapsed_ms << " Hz)"
                << "  publishes=" << pub_count << " ("
                << pub_count * 1000.0 / elapsed_ms << " Hz)"
                << "  axes=" << data.axes.size() << " buttons=" << data.buttons.size()
                << std::endl;
      event_count = 0;
      pub_count = 0;
      stats_time = now;
    }

    // Debug: print raw channels on change (cooldown 300ms)
    if (config.debug_raw) {
      bool changed = false;
      for (size_t i = 0; i < data.axes.size() && !changed; ++i)
        if (std::abs(data.axes[i] - prev_axes[i]) > 0.001f) changed = true;
      for (size_t i = 0; i < data.buttons.size() && !changed; ++i)
        if (data.buttons[i] != prev_buttons[i]) changed = true;

      auto since_debug = std::chrono::duration_cast<std::chrono::milliseconds>(
                             now - debug_last_print).count();
      if (changed && since_debug >= 300) {
        // Print axes row:  AX: [ idx: value ... ]
        std::cout << "\033[2K\r";  // clear line
        for (size_t i = 0; i < data.axes.size(); ++i) {
          bool active = std::abs(data.axes[i]) > 0.01f;
          std::cout << (active ? "\033[1;33m" : "\033[2m")  // bold-yellow / dim
                    << "A" << i << ":" << std::fixed << std::setprecision(2)
                    << std::setw(6) << data.axes[i] << "\033[0m ";
        }
        std::cout << std::endl;

        // Print buttons row: BTN: [ idx:value ... ]
        std::cout << "\033[2K\r";  // clear line
        for (size_t i = 0; i < data.buttons.size(); ++i) {
          bool down = data.buttons[i] != 0;
          std::cout << (down ? "\033[1;32m" : "\033[2m")  // bold-green / dim
                    << "B" << i << ":" << data.buttons[i] << "\033[0m ";
        }
        std::cout << std::endl;

        prev_axes = data.axes;
        prev_buttons = data.buttons;
        debug_last_print = now;
      }
    }
  }

  // ── Cleanup ──────────────────────────────────────────────────────────
  std::cout << "\nShutting down..." << std::endl;
  device.close();

#ifdef HAS_ROS2
  if (config.publisher_type == PublisherType::ROS2) {
    rclcpp::shutdown();
  }
#endif

  return 0;
}
