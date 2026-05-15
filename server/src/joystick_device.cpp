#include "joystick_server/joystick_device.h"

#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/joystick.h>
#include <string.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <unistd.h>

#include <iostream>
#include <sstream>

namespace joystick_server {

JoystickDevice::JoystickDevice() = default;

JoystickDevice::~JoystickDevice() { close(); }

bool JoystickDevice::open(const JoystickConfig& config) {
  std::string path = config.device;

  // If a device name was given, try to discover the path
  if (!config.device_name.empty()) {
    std::string found = findDeviceByName(config.device_name);
    if (found.empty()) {
      std::cerr << "JoystickDevice: no device matching \"" << config.device_name
                << "\" found. Falling back to " << path << std::endl;
    } else {
      path = found;
    }
  }

  fd_ = ::open(path.c_str(), O_RDONLY);
  if (fd_ < 0) {
    std::cerr << "JoystickDevice: cannot open " << path << ": " << strerror(errno) << std::endl;
    return false;
  }

  // Read device name
  char cname[128] = "Unknown";
  if (ioctl(fd_, JSIOCGNAME(sizeof(cname)), cname) < 0) {
    std::cerr << "JoystickDevice: ioctl JSIOCGNAME failed: " << strerror(errno) << std::endl;
  }
  name_ = cname;

  // Read capabilities
  __u8 n_axes = 0, n_buttons = 0;
  ioctl(fd_, JSIOCGAXES, &n_axes);
  ioctl(fd_, JSIOCGBUTTONS, &n_buttons);
  num_axes_ = n_axes;
  num_buttons_ = n_buttons;

  // Workaround for a driver bug: the initial events are the last-known state,
  // not the current state. Re-open to get fresh initial state.
  ::close(fd_);
  fd_ = ::open(path.c_str(), O_RDONLY);
  if (fd_ < 0) {
    std::cerr << "JoystickDevice: re-open failed: " << strerror(errno) << std::endl;
    return false;
  }

  error_ = false;
  std::cout << "JoystickDevice: opened " << name_ << " at " << path << " ("
            << num_axes_ << " axes, " << num_buttons_ << " buttons)" << std::endl;
  return true;
}

void JoystickDevice::close() {
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
  num_buttons_ = 0;
  num_axes_ = 0;
}

bool JoystickDevice::readEvent(js_event* event, int timeout_ms) {
  if (fd_ < 0) return false;

  fd_set set;
  FD_ZERO(&set);
  FD_SET(fd_, &set);

  struct timeval tv;
  struct timeval* ptv = nullptr;
  if (timeout_ms >= 0) {
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    ptv = &tv;
  }

  int ret = select(fd_ + 1, &set, nullptr, nullptr, ptv);
  if (ret < 0) {
    if (errno == EINTR) return false;
    std::cerr << "JoystickDevice: select error: " << strerror(errno) << std::endl;
    error_ = true;
    return false;
  }
  if (ret == 0) return false;  // timeout

  if (!FD_ISSET(fd_, &set)) return false;

  ssize_t n = read(fd_, event, sizeof(*event));
  if (n < 0) {
    if (errno == EAGAIN) return false;
    std::cerr << "JoystickDevice: read error: " << strerror(errno) << std::endl;
    error_ = true;
    return false;
  }
  if (n != sizeof(*event)) {
    std::cerr << "JoystickDevice: short read (" << n << " != " << sizeof(*event) << ")"
              << std::endl;
    error_ = true;
    return false;
  }

  return true;
}

std::string JoystickDevice::findDeviceByName(const std::string& name) {
  const char* path = "/dev/input";
  DIR* dev_dir = opendir(path);
  if (!dev_dir) {
    std::cerr << "Cannot open " << path << ": " << strerror(errno) << std::endl;
    return {};
  }

  std::string result;
  struct dirent* entry;
  while ((entry = readdir(dev_dir)) != nullptr) {
    if (strncmp(entry->d_name, "js", 2) != 0) continue;

    std::string full = std::string(path) + "/" + entry->d_name;
    int fd = ::open(full.c_str(), O_RDONLY);
    if (fd < 0) continue;

    char cname[128] = "Unknown";
    if (ioctl(fd, JSIOCGNAME(sizeof(cname)), cname) >= 0) {
      if (name == cname) {
        result = full;
      }
    }
    ::close(fd);

    if (!result.empty()) {
      std::cout << "Found joystick \"" << name << "\" at " << result << std::endl;
      break;
    }
  }
  closedir(dev_dir);
  return result;
}

}  // namespace joystick_server
