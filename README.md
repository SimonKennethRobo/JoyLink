# JoystickServer

Linux游戏手柄C++服务端，从 `/dev/input/js*` 读取手柄数据，通过 ROS2 或 ZMQ 发布。

## 快速开始

```bash
# 构建
source /opt/ros/jazzy/setup.sh
cmake -S . -B build
cmake --build build -j$(nproc)

# 运行 ZMQ 模式
./build/joystick_server config/zmq_pub.yaml

# 运行 ROS2 模式
cmake -S . -B build -DBUILD_ROS2=ON
./build/joystick_server config/default.yaml
```

## 配置

复制 `config/default.yaml`，按手柄型号修改键位映射：

```yaml
joystick_server:
  device: "/dev/input/js0"
  deadzone: 0.05
  publish_rate: 50.0        # Hz
  publisher_type: "ros2"    # "ros2" 或 "zmq"

  button_mapping:           # 逻辑名 → 物理索引
    a: 0
    b: 1
    # ...

  axes_mapping:
    left_stick_x: 0
    left_stick_y: 1
    # ...
```

手柄物理索引可通过 `jstest /dev/input/js0` 查看。

## ZMQ 消费者示例

```python
import zmq, json
ctx = zmq.Context()
sock = ctx.socket(zmq.SUB)
sock.connect("tcp://localhost:5555")
sock.setsockopt_string(zmq.SUBSCRIBE, "")
while True:
    topic, body = sock.recv_string(), sock.recv_string()
    data = json.loads(body)
    print(f"axes: {data['axes']}, buttons: {data['buttons']}")
```

## 依赖

- yaml-cpp (系统包)
- libzmq (系统包, ZMQ模式)
- cmake >= 3.16
- cppzmq (FetchContent自动下载)
- ROS2 Jazzy (ROS2模式)
