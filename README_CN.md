# 🎮 JoyLink 中文说明

面向 Linux 的 C++17 游戏手柄桥接程序。它从 `/dev/input/js*` 读取手柄事件，
归一化轴/按键数据，并通过 **ROS 2**、**ZMQ** 或 **DDS** 发布。

📘 English: [README.md](README.md)
🏗️ 架构说明: [DESIGN.md](DESIGN.md)

## ✨ 特性

- 🕹️ 直接使用 Linux joystick API，运行时依赖少
- 🚀 可选发布后端：ROS 2、ZMQ、CycloneDDS
- 🧭 通过 `joystick_common::JoystickMapper` 复用语义化键位映射
- 📦 启用 DDS 时可自动拉取并编译 CycloneDDS 依赖

## 🗂️ 目录

```text
common/                  共享配置解析和手柄映射实现
include/joystick_common/ 共享公共头文件
server/                  手柄读取和发布后端
client/                  ZMQ C++/Python 客户端
config/                  YAML 预设和 CycloneDDS XML
idl/                     DDS 消息定义
```

## ⚙️ 编译

默认启用 ZMQ：

```bash
sudo apt install libzmq3-dev

cmake -S . -B build -DBUILD_ZMQ=ON -DBUILD_ROS2=OFF -DBUILD_DDS=OFF
cmake --build build -j$(nproc)
```

运行服务端：

```bash
./build/server/joylink config/zmq_pub.yaml
```

运行 ZMQ 示例客户端：

```bash
./build/client/joystick_client_example config/zmq_pub.yaml
```

## 🔌 发布后端

### ROS 2

```bash
source /opt/ros/jazzy/setup.sh
cmake -S . -B build-ros2 -DBUILD_ZMQ=OFF -DBUILD_ROS2=ON -DBUILD_DDS=OFF
cmake --build build-ros2 -j$(nproc)
./build-ros2/server/joylink config/default.yaml
```

发布 `sensor_msgs::msg::Joy`。

### ZMQ

```bash
cmake -S . -B build-zmq -DBUILD_ZMQ=ON -DBUILD_ROS2=OFF -DBUILD_DDS=OFF
cmake --build build-zmq -j$(nproc)
./build-zmq/server/joylink config/zmq_pub.yaml
```

发布 multipart PUB/SUB 消息：topic 帧 + JSON 负载。

### DDS

```bash
cmake -S . -B build-dds -DBUILD_ZMQ=OFF -DBUILD_ROS2=OFF -DBUILD_DDS=ON
cmake --build build-dds -j$(nproc)
```

启用 `BUILD_DDS=ON` 时，CMake 可以自动把 CycloneDDS/CycloneDDS-CXX 拉取并编译到
`third_party/`。如果想优先使用系统已有安装，加上 `-DFORCE_FETCH_DDS_DEPS=OFF`。

局域网 DDS 通信：

```bash
# 先修改 config/cyclonedds.xml，把网卡名改成实际接口。
export CYCLONEDDS_URI=file://$PWD/config/cyclonedds.xml
./build-dds/server/joylink config/dds_pub.yaml
```

## 🛠️ 配置

从 `config/` 里的预设开始改：

```yaml
joylink:
  device: "/dev/input/js0"
  deadzone: 0.05
  publish_rate: 50.0
  publisher_type: "zmq" # "ros2", "zmq" 或 "dds"

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

用 `jstest /dev/input/js0` 或 `debug_raw: true` 查看物理轴/按键索引。
映射值为 `-1` 表示禁用该逻辑按键或轴。

## 🧑‍💻 客户端 API

```cpp
#include "joystick_client/joystick_client.h"

joystick_client::JoystickClient client("config/zmq_pub.yaml");
client.connect();

joystick_common::MappedJoystickData data;
if (client.receive(data)) {
  float lx = data.axes["left_stick_x"];
  bool a = data.buttons["a"];
}
```

Python 示例位于 `client/python/`。

## 🐍 Python 安装（可编辑）

在仓库根目录执行：

```bash
pip install -e .
```

可选依赖：

```bash
pip install -e ".[dds]"   # DDS 后端
```
