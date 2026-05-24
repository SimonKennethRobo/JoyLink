# 🎮 JoyLink

Fast C++17 joystick bridge for Linux. It reads `/dev/input/js*`, normalizes
axes/buttons, and publishes the stream through **ROS 2**, **ZMQ**, or **DDS**.

📘 中文说明: [README_CN.md](README_CN.md)
🏗️ Design notes: [DESIGN.md](DESIGN.md)

## ✨ Features

- 🕹️ Direct Linux joystick input with low runtime overhead
- 🚀 Optional publisher backends: ROS 2, ZMQ, CycloneDDS
- 🧭 Shared semantic mapping via `joystick_common::JoystickMapper`
- 📦 Automatic CycloneDDS dependency fetch/build when DDS is enabled

## 🗂️ Layout

```text
common/                  shared config parser and mapper
include/joystick_common/ public shared headers
server/                  joystick reader and publisher backends
client/                  ZMQ C++/Python clients
config/                  YAML presets and CycloneDDS XML
idl/                     DDS message definition
```

## ⚙️ Build

ZMQ is enabled by default:

```bash
sudo apt install libzmq3-dev

cmake -S . -B build -DBUILD_ZMQ=ON -DBUILD_ROS2=OFF -DBUILD_DDS=OFF
cmake --build build -j$(nproc)
```

Run the server:

```bash
./build/server/joylink config/zmq_pub.yaml
```

Run the ZMQ example client:

```bash
./build/client/joylink_client_example config/zmq_pub.yaml
```

## 🔌 Backends

### ROS 2

```bash
source /opt/ros/jazzy/setup.sh
cmake -S . -B build-ros2 -DBUILD_ZMQ=OFF -DBUILD_ROS2=ON -DBUILD_DDS=OFF
cmake --build build-ros2 -j$(nproc)
./build-ros2/server/joylink config/default.yaml
```

Publishes `sensor_msgs::msg::Joy`.

### ZMQ

```bash
cmake -S . -B build-zmq -DBUILD_ZMQ=ON -DBUILD_ROS2=OFF -DBUILD_DDS=OFF
cmake --build build-zmq -j$(nproc)
./build-zmq/server/joylink config/zmq_pub.yaml
```

Publishes multipart PUB/SUB messages: topic frame + JSON payload.

### DDS

```bash
cmake -S . -B build-dds -DBUILD_ZMQ=OFF -DBUILD_ROS2=OFF -DBUILD_DDS=ON
cmake --build build-dds -j$(nproc)
```

With `BUILD_DDS=ON`, CMake can fetch and build CycloneDDS/CycloneDDS-CXX under
`third_party/`. Use `-DFORCE_FETCH_DDS_DEPS=OFF` to prefer an existing install.

For LAN DDS:

```bash
# Edit config/cyclonedds.xml and set the real network interface first.
export CYCLONEDDS_URI=file://$PWD/config/cyclonedds.xml
./build-dds/server/joylink config/dds_pub.yaml
```

## 🛠️ Config

Start from a preset in `config/`:

```yaml
joylink:
  device: "/dev/input/js0"
  deadzone: 0.05
  publish_rate: 50.0
  publisher_type: "zmq" # "ros2", "zmq", or "dds"

  zmq:
    address: "tcp://*:5555"

  dds:
    topic: "JoyData"

  button_mapping:
    a: 0
    b: 1
    touchpad: -1

  axes_mapping:
    left_stick_x: 0
    left_stick_y: 1
```

Use `jstest /dev/input/js0` or set `debug_raw: true` to inspect physical
button/axis indices. A mapping value of `-1` disables that logical control.

## 🧑‍💻 Client API

```cpp
#include "joylink_client/joylink_client.h"

joylink_client::JoylinkClient client("config/zmq_pub.yaml");
client.connect();

joystick_common::MappedJoystickData data;
if (client.receive(data)) {
  float lx = data.axes["left_stick_x"];
  bool a = data.buttons["a"];
}
```

Python examples live in `client/python/`.

## 🐍 Python install (editable)

From the repo root:

```bash
pip install -e .
```

Optional extras:

```bash
pip install -e ".[dds]"   # DDS backend
```
