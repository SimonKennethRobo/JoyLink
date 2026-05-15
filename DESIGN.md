# JoystickServer 架构与设计说明

## 概览

```
┌──────────────────────────────────────────────────────────────┐
│                         main.cpp                             │
│              (主循环: select + 定时发布 + 统计)               │
├──────────────────┬────────────────────┬──────────────────────┤
│  JoystickDevice  │   IPublisher       │   JoystickConfig     │
│  /dev/input/js*  │   (抽象接口)        │   (YAML配置)         │
│  Linux joystick  ├────────┬───────────┤                      │
│  API             │ ROS2   │ ZMQ       │   yaml-cpp          │
│                  │ Pub    │ Pub       │                      │
└──────────────────┴────────┴───────────┴──────────────────────┘
```

## 模块设计

### 1. JoystickDevice — 设备读取层

**文件**: `joystick_device.h/cpp`

封装 Linux joystick API (`linux/joystick.h`)，直接读取 `/dev/input/js*` 字符设备：

- `open(config)`: 打开设备，读取名称和capabilities（轴数/按钮数）。采用 ROS `joy_linux_node` 的双次open技巧规避内核驱动bug（初始事件为上次关闭时的状态而非当前状态）
- `readEvent(event, timeout_ms)`: 基于 `select()+read()` 的阻塞/超时读取，返回 `js_event` 原始结构体
- `findDeviceByName(name)`: 遍历 `/dev/input/js*`，通过 `JSIOCGNAME` ioctl 匹配设备名，支持热插拔前的设备发现

**设计要点**：
- 所有错误通过 `std::atomic<bool> error_` 标记，主循环可检测设备断开
- timeout_ms = 0 为非阻塞，-1 为永久阻塞
- 相比SDL方案，零额外依赖，延迟更低

### 2. IPublisher — 发布抽象层

**文件**: `ipublisher.h`, `ros2_publisher.h/cpp`, `zmq_publisher.h/cpp`

策略模式，`createPublisher(config)` 根据 `publisher_type` 返回对应实现：

#### ROS2 Publisher
- 内部创建独立 `rclcpp::Node` 和 `SingleThreadedExecutor`
- 发布 `sensor_msgs::msg::Joy`（兼容 ROS 生态）
- `spinOnce()`: 每帧调用 `executor->spin_some()`，处理订阅回调和反馈
- 使用 `RCL_STEADY_TIME` 作为时间戳时钟源

#### ZMQ Publisher
- PUB-SUB 模式，bind 到配置的地址（如 `tcp://*:5555`）
- 多帧发送：topic 帧 + JSON 负载帧，支持订阅者按 topic 过滤
- JSON格式：`{"frame_id":"joy","ts_ns":...,"axes":[...],"buttons":[...]}`
- 设置 `linger=0`，关闭时立即释放端口

### 3. JoystickConfig — 配置层

**文件**: `config_parser.h/cpp`, `types.h`

YAML配置结构：

| 配置项 | 说明 |
|--------|------|
| `device` | 设备路径，默认 `/dev/input/js0` |
| `device_name` | 设备名，非空时自动搜索匹配 |
| `deadzone` | 死区 [0, 1)，默认 0.05 |
| `publish_rate` | 发布频率 Hz，默认 50 |
| `coalesce_interval` | 事件合并窗口秒，默认 0.001 |
| `sticky_buttons` | toggle模式按钮 |
| `publisher_type` | `"ros2"` 或 `"zmq"` |
| `button_mapping` | 12个逻辑按钮 → 物理索引映射 |
| `axes_mapping` | 8个逻辑轴 → 物理索引映射 |

支持的逻辑按钮: `a, b, x, y, lb, rb, back, start, guide, left_stick, right_stick, touchpad`
支持的逻辑轴: `left_stick_x, left_stick_y, left_trigger, right_stick_x, right_stick_y, right_trigger, dpad_x, dpad_y`

### 4. main.cpp — 主事件循环

```
while (running):
    publisher->spinOnce()          // ROS2 executor
    timeout = calcSelectTimeout()  // 基于 publish_rate 和 coalesce
    if (device.readEvent(timeout)):
        ├─ JS_EVENT_BUTTON → 更新 buttons[]
        └─ JS_EVENT_AXIS  → 死区 + 归一化 → 更新 axes[]
    if (publish_due):
        publisher->publish(data)
```

**死区处理** (`applyDeadzone`):
```
if |raw| < deadzone → 0
else → sign(raw) * (|raw| - deadzone) / (1 - deadzone)
```
线性缩放,保证输出连续覆盖 [-1, 1]。

**Coalesce 机制**：收到第一个事件后，等待 `coalesce_interval` 窗口收集后续事件，窗口关闭后一次发布。减少高速轴变化时的消息数。

**Autorepeat**：即使无新事件，到达 `1/publish_rate` 间隔后仍发布当前状态，保证下游有固定的数据心跳。

**统计**：每5秒输出事件速率和发布速率。

## 数据流

```
/dev/input/js0
     │ js_event {time, value, type, number}
     ▼
JoystickDevice::readEvent()
     │ raw (int16_t)
     ▼
main loop
     │ applyDeadzone + clamp
     ▼
JoystickData {axes<float>, buttons<int32>, timestamp_ns}
     │
     ├─→ Ros2Publisher → sensor_msgs::msg::Joy → ROS2 topic
     └─→ ZmqPublisher  → JSON (multi-part)     → ZMQ PUB socket (TCP)
```

## 编译时配置

通过 CMake `target_compile_definitions` 控制：
- `HAS_ROS2` — 编译 ROS2 publisher 及 `rclcpp::init/shutdown`
- `HAS_ZMQ` — 编译 ZMQ publisher

工厂函数 `createPublisher()` 在请求未编译的后端时抛出异常。

## 对比参考实现

| 特性 | ROS joy_linux | SDL joy | 本项目 |
|------|--------------|---------|--------|
| 依赖 | 仅Linux头文件 | SDL2 | yaml-cpp + (可选)ROS2/ZMQ |
| 配置 | ROS参数 | ROS参数 | YAML文件 |
| 发布 | 仅ROS2 | 仅ROS2 | ROS2 + ZMQ |
| 手柄映射 | 无 | 无 | 品牌级键位映射 |
| 设备发现 | 按名称 | 按名称/ID | 按名称 |
