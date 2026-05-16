# JoyLink Design

This document describes the current architecture of `JoyLink` after the
include/source split into `common`, `server`, and `client`.

## Goals

- Keep Linux joystick input handling small, direct, and low-latency.
- Let the same normalized joystick stream feed ROS 2, ZMQ, or DDS consumers.
- Keep shared configuration and semantic mapping reusable by both server and
  client code.
- Keep backend dependencies optional at compile time.

## High-Level Architecture

```text
/dev/input/js*
      |
      v
server/src/joystick_device.cpp
      |
      v
server/src/main.cpp
  - event loop
  - deadzone processing
  - button/axis state
  - coalescing and autorepeat
      |
      v
JoyLink publisher interface
      |
      +--> ROS 2 publisher -> sensor_msgs::msg::Joy
      +--> ZMQ publisher   -> multipart JSON PUB/SUB
      +--> DDS publisher   -> joy_data::JoyData

common/
  - YAML config parser
  - shared data types
  - semantic joystick mapper

client/
  - ZMQ C++ receive/mapping library
  - ZMQ example subscriber
  - Python client examples
```

## Module Boundaries

### `joystick_common`

Files:

- `include/joystick_common/types.h`
- `include/joystick_common/config_parser.h`
- `include/joystick_common/joystick_mapper.h`
- `common/src/config_parser.cpp`
- `common/src/joystick_mapper.cpp`

`joystick_common` owns data structures and logic that are valid on both sides of
the transport boundary:

- `JoystickData`: raw channel-indexed axes/buttons plus monotonic timestamp.
- `JoystickConfig`: device, processing, backend, and mapping configuration.
- `JoystickMapper`: converts raw physical channel arrays into named logical
  maps such as `left_stick_x`, `right_trigger`, and `a`.
- YAML parsing through `loadConfig()`.

The server publishes raw channel arrays. Consumers can apply the same mapping
configuration locally, which keeps the transport payload compact and preserves
access to raw data when needed.

### `joylink_core`

Files:

- `server/include/joystick_server/joystick_device.h`
- `server/src/joystick_device.cpp`

`joylink_core` is server-only. It wraps the Linux joystick API and owns:

- opening `/dev/input/js*`
- optional device lookup by name
- querying device name, button count, and axis count
- reading `js_event` with `select()` based timeout handling
- detecting device errors/disconnects

The implementation intentionally uses the Linux joystick API directly instead
of SDL, keeping runtime dependencies and latency low.

### `joylink`

Files:

- `server/src/main.cpp`
- `server/include/joystick_server/ipublisher.h`
- `server/include/joystick_server/*_publisher.h`
- `server/src/ipublisher.cpp`
- `server/src/*_publisher.cpp`

The executable owns the runtime event loop:

```text
while running:
    publisher->spinOnce()
    timeout = next publish/coalesce deadline
    if device.readEvent(timeout):
        update raw button/axis state
    if publish is due:
        publisher->publish(data)
```

Processing behavior:

- Axis values are normalized to `[-1.0, 1.0]`.
- Deadzone is applied with linear rescaling outside the deadzone.
- `axis_reverse` multiplies selected physical axes by `-1` in the client mapper.
- `button_invert` flips selected physical button states in the client mapper.
- `sticky_buttons` can turn button presses into toggle-style state changes.
- `coalesce_interval` batches bursts of input events before publishing.
- `publish_rate` provides autorepeat heartbeats even when no new event arrives.

### Publisher Backends

The publisher interface is selected from `JoystickConfig::publisher_type`:

- `ROS2`: publishes `sensor_msgs::msg::Joy` to `ros2.topic`.
- `ZMQ`: binds `zmq.address` and sends a two-frame PUB message:
  topic frame plus JSON payload.
- `DDS`: publishes `joy_data::JoyData` on `dds.topic` using CycloneDDS-CXX.

Each backend is guarded by compile definitions:

- `HAS_ROS2`
- `HAS_ZMQ`
- `HAS_DDS`

If the YAML requests a backend that was not compiled in, `createPublisher()`
throws a runtime error with the matching rebuild option.

### `joystick_client`

Files:

- `client/include/joystick_client/joystick_client.h`
- `client/src/joystick_client.cpp`
- `client/examples/zmq_subscriber.cpp`
- `client/python/`

The C++ client currently targets the ZMQ transport. It loads the same YAML
configuration, connects to the configured ZMQ endpoint, receives raw messages,
and can return either:

- `joystick_common::JoystickData` through `receiveRaw()`
- `joystick_common::MappedJoystickData` through `receive()`

Python clients are intentionally lightweight examples for scripting and quick
integration tests.

## Build Architecture

Top-level CMake options:

| Option | Default | Purpose |
| ------ | ------- | ------- |
| `BUILD_ZMQ` | `ON` | Build the ZMQ publisher and C++ client. |
| `BUILD_ROS2` | `OFF` | Build the ROS 2 publisher when ROS 2 packages are available. |
| `BUILD_DDS` | `OFF` | Build the DDS publisher and IDL-generated types. |
| `FETCH_DDS_DEPS` | `ON` | Fetch/build CycloneDDS dependencies when DDS is requested. |
| `FORCE_FETCH_DDS_DEPS` | `ON` | Use the `third_party` DDS build path instead of probing the system first. |
| `CYCLONEDDS_VERSION` | `0.10.5` | Git tag used for both CycloneDDS repositories. |

Target graph:

```text
yaml-cpp
   |
   v
joystick_common
   |
   +--> joylink_core
   |         |
   |         v
   |    joylink
   |
   +--> joystick_client  (when ZMQ is enabled)
             |
             v
        joystick_client_example
```

DDS dependencies are fetched into:

```text
third_party/cyclonedds
third_party/cyclonedds-cxx
```

and installed into each repository's local `install/` directory. Generated DDS
sources are placed in the active build tree under `server/`.

## Data Formats

### Internal Raw State

```cpp
struct JoystickData {
  std::vector<float> axes;
  std::vector<int32_t> buttons;
  uint64_t timestamp_ns;
};
```

### Semantic Mapped State

`JoystickMapper` converts channel-indexed arrays into maps keyed by logical
names:

```cpp
mapped.axes["left_stick_x"]
mapped.axes["right_trigger"]
mapped.buttons["a"]
mapped.buttons["start"]
```

A mapping value of `-1` disables that logical entry.

### ZMQ Payload

ZMQ uses a multipart PUB message:

```text
frame 0: topic
frame 1: JSON payload
```

Payload shape:

```json
{
  "frame_id": "joy",
  "ts_ns": 123456789,
  "axes": [0.0, -0.5],
  "buttons": [0, 1]
}
```

### DDS Payload

DDS uses `idl/JoystickData.idl` and CycloneDDS-CXX generated types. The topic
name comes from `joylink.dds.topic`.

## LAN DDS Notes

For cross-machine DDS communication, all participating machines should use a
compatible CycloneDDS configuration. `config/cyclonedds.xml` pins the network
interface and enables multicast:

```bash
export CYCLONEDDS_URI=file://$PWD/config/cyclonedds.xml
```

Change the `<NetworkInterface name="..."/>` value to the real LAN interface on
each machine, for example `eth0`, `enp3s0`, or `wlan0`.

## Design Tradeoffs

- Raw channel arrays are the transport contract; semantic names stay in shared
  mapping code. This avoids expanding payloads and lets consumers choose raw or
  mapped access.
- Server-only headers live under `server/include/joystick_server`; shared API
  headers live under `include/joystick_common`.
- Backend implementations are optional and isolated behind `IPublisher`, so a
  minimal ZMQ-only build does not require ROS 2 or DDS.
- DDS dependencies are automated in CMake for reproducible local builds, while
  `FORCE_FETCH_DDS_DEPS=OFF` keeps system installations usable.
