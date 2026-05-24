// Example: ZMQ joystick client — receives raw data and prints mapped values.
//
// Build:
//   cmake -S . -B build -DBUILD_ZMQ=ON
//   cmake --build build
// Run:
//   ./build/client/joylink_client_example config/xbox.yaml

#include <atomic>
#include <csignal>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>

#include "joylink_client/joylink_client.h"

std::atomic<bool> running{true};
void signalHandler(int) { running = false; }

// ── Layout constants ────────────────────────────────────────────────
constexpr int W_NAME = 4;     // axis label (2 chars + padding)
constexpr int W_VAL  = 7;     // value (+0.000)
constexpr int W_GAP  = 8;     // spacing between left/right axis pairs

namespace {

std::string axisLine(const std::string& n1, float v1,
                     const std::string& n2, float v2) {
  std::ostringstream ss;
  ss << " " << std::left  << std::setw(W_NAME) << n1
     << " " << std::right << std::setw(W_VAL)  << std::fixed << std::setprecision(3) << v1
     << std::string(W_GAP, ' ')
     << std::left  << std::setw(W_NAME) << n2
     << " " << std::right << std::setw(W_VAL)  << std::fixed << std::setprecision(3) << v2;
  return ss.str();
}

std::string btnPair(const std::string& label, bool pressed) {
  std::ostringstream ss;
  ss << (pressed ? "[" : " ") << std::setw(4) << std::left << label << (pressed ? "]" : " ");
  return ss.str();
}

std::string btnRow(const std::map<std::string, int>& buttons,
                   const std::vector<std::pair<std::string, std::string>>& order) {
  std::ostringstream ss;
  for (const auto& [display, key] : order) {
    ss << btnPair(display, buttons.at(key));
  }
  return ss.str();
}

void printFrame(const joystick_common::MappedJoystickData& d) {
  // Abbreviated button names in display order
  using BP = std::pair<std::string, std::string>;  // {display, map_key}
  static const std::vector<BP> row1 = {{"a","a"},{"b","b"},{"x","x"},{"y","y"},{"lb","lb"},{"rb","rb"}};
  static const std::vector<BP> row2 = {{"BK","back"},{"ST","start"},{"GD","guide"},{"LS","left_stick"},{"RS","right_stick"}};

  std::cout << "\033[H";  // cursor home, overwrite previous frame
  std::cout << "┌─────────────── Axes ───────────────┬────── Buttons ──────┐\n";
  std::cout << "│" << axisLine("LX", d.axes.at("left_stick_x"),  "RX", d.axes.at("right_stick_x"))  << " │\n";
  std::cout << "│" << axisLine("LY", d.axes.at("left_stick_y"),  "RY", d.axes.at("right_stick_y"))  << " │\n";
  std::cout << "│" << axisLine("LT", d.axes.at("left_trigger"),  "RT", d.axes.at("right_trigger"))  << " │\n";
  std::cout << "│" << axisLine("DX", d.axes.at("dpad_x"),        "DY", d.axes.at("dpad_y"))          << " │\n";
  std::cout << "├──────────────────────────────────────┼──────────────────────┤\n";
  std::cout << "│ " << btnRow(d.buttons, row1) << " │\n";
  std::cout << "│ " << btnRow(d.buttons, row2) << " │\n";
  std::cout << "└──────────────────────────────────────┴──────────────────────┘" << std::endl;
}

void printHeader() {
  std::cout << "\033[2J\033[H";  // clear screen, cursor home
  std::cout << "┌─────────────── Axes ───────────────┬────── Buttons ──────┐\n"
               "│                                    │                     │\n"
               "│    waiting for joystick data...    │                     │\n"
               "│                                    │                     │\n"
               "└────────────────────────────────────┴─────────────────────┘\n";
}

}  // namespace

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <config.yaml>" << std::endl;
    return 1;
  }

  std::signal(SIGINT, signalHandler);
  std::signal(SIGTERM, signalHandler);

  joylink_client::JoylinkClient client(argv[1]);
  if (!client.connect()) {
    std::cerr << "Failed to connect" << std::endl;
    return 1;
  }

  printHeader();

  while (running) {
    joystick_common::MappedJoystickData data;
    if (client.receive(data, 1000)) {
      printFrame(data);
    }
  }

  std::cout << "\nDone." << std::endl;
  return 0;
}
