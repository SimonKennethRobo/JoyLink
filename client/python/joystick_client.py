"""JoystickClient — Python client for JoyLink.

Supports ZMQ (SUB), ROS2 (rclpy), and CycloneDDS backends.
Receives raw channel arrays and maps them to semantic key-value pairs
using the same YAML configuration file.

Usage:
    from joystick_client import JoystickClient

    client = JoystickClient("config/xbox.yaml")
    while True:
        data = client.receive(timeout_ms=1000)
        if data:
            print(data["axes"]["left_stick_x"], data["buttons"]["a"])
"""

import json
import threading
import time
from collections import deque

import yaml


class JoystickClient:
    def __init__(self, yaml_path: str):
        """Load YAML config and build button/axes inverse mapping."""
        with open(yaml_path) as f:
            cfg = yaml.safe_load(f)

        js = cfg.get("joylink", cfg.get("joystick_server", cfg))
        self._publisher_type = js.get("publisher_type", "ros2")
        self._frame_id = js.get("frame_id", "joy")

        # Build inverse mappings: physical_index → logical_name
        btn_cfg = js.get("button_mapping", {})
        axs_cfg = js.get("axes_mapping", {})
        self._btn_map = {v: k for k, v in btn_cfg.items() if v >= 0}
        self._axis_map = {v: k for k, v in axs_cfg.items() if v >= 0}

        # Config for each backend
        self._ros2_topic = js.get("ros2", {}).get("topic", "joy")
        self._zmq_address = js.get("zmq", {}).get("address", "tcp://localhost:5555")
        self._dds_topic = js.get("dds", {}).get("topic", "JoyData")

        self._connected = False
        self._backend = None  # "zmq", "ros2", or "dds"
        self._impl = None
        self._queue = deque()
        self._cv = threading.Condition()
        self._rx_thread = None
        self._running = False
        self._max_queue = 100

    # ── Connection ────────────────────────────────────────────────────

    def connect(self) -> bool:
        """Connect to the publisher. Auto-detects backend from config."""
        if self._connected:
            return True

        if self._publisher_type == "zmq":
            self._impl = _ZmqBackend(self._zmq_address)
        elif self._publisher_type == "ros2":
            self._impl = _Ros2Backend(self._ros2_topic)
        elif self._publisher_type == "dds":
            self._impl = _DdsBackend(self._dds_topic)
        else:
            raise RuntimeError(f"Unknown publisher_type: {self._publisher_type}")

        self._impl.connect()
        self._backend = self._publisher_type
        self._connected = True
        self._start_receiver()
        return True

    def disconnect(self):
        if self._connected and self._impl:
            self._stop_receiver()
            self._impl.disconnect()
            self._connected = False

    # ── Data access ───────────────────────────────────────────────────

    def receive(self, timeout_ms: int = 1000) -> dict | None:
        """Blocking read from background queue with timeout.

        Return format:
            {
                "axes":    {"left_stick_x": 0.5, "left_stick_y": 0.0, ...},
                "buttons": {"a": 1, "b": 0, ...},
                "ts_ns":   1649662620242754,
                "frame_id": "joy"
            }
        """
        raw = self.receive_raw(timeout_ms)
        if raw is None:
            return None
        return self._map(raw)

    def receive_raw(self, timeout_ms: int = 1000) -> dict | None:
        """Receive raw data without mapping. Returns dict or None on timeout."""
        if not self._connected:
            return None
        return self._pop_queue(timeout_ms)

    # ── Helpers ────────────────────────────────────────────────────────

    def _map(self, raw: dict) -> dict:
        """Map raw channel arrays to semantic key-value pairs."""
        axes_list = raw.get("axes", [])
        buttons_list = raw.get("buttons", [])

        axes = {
            name: (axes_list[idx] if idx < len(axes_list) else 0.0)
            for idx, name in self._axis_map.items()
        }
        buttons = {
            name: (buttons_list[idx] if idx < len(buttons_list) else 0)
            for idx, name in self._btn_map.items()
        }

        return {
            "axes": axes,
            "buttons": buttons,
            "ts_ns": raw.get("ts_ns", 0),
            "frame_id": raw.get("frame_id", ""),
        }

    @property
    def axis_names(self):
        return list(self._axis_map.values())

    @property
    def button_names(self):
        return list(self._btn_map.values())

    def close(self):
        self.disconnect()

    def __enter__(self):
        self.connect()
        return self

    def __exit__(self, *args):
        self.close()

    # ── Background receiver ─────────────────────────────────────────

    def _start_receiver(self):
        if self._running:
            return
        self._running = True
        self._rx_thread = threading.Thread(target=self._recv_loop, daemon=True)
        self._rx_thread.start()

    def _stop_receiver(self):
        if not self._running:
            return
        self._running = False
        with self._cv:
            self._cv.notify_all()
        if self._rx_thread:
            self._rx_thread.join(timeout=1.0)
        self._rx_thread = None

    def _recv_loop(self):
        while self._running:
            if not self._impl:
                break
            data = self._impl.receive(100)
            if data is None:
                continue
            with self._cv:
                if len(self._queue) >= self._max_queue:
                    self._queue.popleft()
                self._queue.append(data)
                self._cv.notify()

    def _pop_queue(self, timeout_ms: int) -> dict | None:
        deadline = None
        if timeout_ms >= 0:
            deadline = time.monotonic() + timeout_ms / 1000.0

        with self._cv:
            while True:
                if self._queue:
                    return self._queue.popleft()
                if timeout_ms == 0:
                    return None
                if deadline is None:
                    self._cv.wait()
                else:
                    remaining = deadline - time.monotonic()
                    if remaining <= 0:
                        return None
                    self._cv.wait(timeout=remaining)


# ═══════════════════════════════════════════════════════════════════════
# Backend implementations
# ═══════════════════════════════════════════════════════════════════════

class _ZmqBackend:
    def __init__(self, address: str):
        import zmq
        self._zmq = zmq
        addr = address.replace("//*", "//localhost")
        self._address = addr
        self._ctx = zmq.Context()
        self._sock = self._ctx.socket(zmq.SUB)

    def connect(self):
        self._sock.setsockopt_string(self._zmq.SUBSCRIBE, "")
        self._sock.connect(self._address)
        print(f"JoystickClient[ZMQ]: connected to {self._address}")

    def disconnect(self):
        self._sock.close()
        self._ctx.term()

    def receive(self, timeout_ms: int) -> dict | None:
        self._sock.setsockopt(self._zmq.RCVTIMEO, timeout_ms)
        try:
            self._sock.recv_string()         # topic frame (discard)
            body = self._sock.recv_string()  # JSON payload
        except self._zmq.Again:
            return None
        return json.loads(body)


class _Ros2Backend:
    def __init__(self, topic: str):
        import rclpy
        from sensor_msgs.msg import Joy

        if not rclpy.ok():
            rclpy.init()

        self._node = rclpy.create_node("joystick_client")
        self._queue = deque()
        self._lock = threading.Lock()
        self._topic = topic

        self._sub = self._node.create_subscription(
            Joy, topic,
            lambda msg: self._on_msg(msg),
            10
        )
        self._executor = None
        self._spin_thread = None
        self._running = False

    def _on_msg(self, msg):
        data = {
            "axes": list(msg.axes),
            "buttons": list(msg.buttons),
            "ts_ns": msg.header.stamp.nanosec + msg.header.stamp.sec * 1_000_000_000,
            "frame_id": msg.header.frame_id,
        }
        with self._lock:
            self._queue.append(data)

    def connect(self):
        import rclpy
        from rclpy.executors import SingleThreadedExecutor

        self._running = True
        self._executor = SingleThreadedExecutor()
        self._executor.add_node(self._node)

        def spin():
            while self._running:
                self._executor.spin_once(timeout_sec=0.05)

        self._spin_thread = threading.Thread(target=spin, daemon=True)
        self._spin_thread.start()
        print(f"JoystickClient[ROS2]: subscribed to {self._topic}")

    def disconnect(self):
        self._running = False
        if self._spin_thread:
            self._spin_thread.join(timeout=0.5)
        if self._executor:
            self._executor.shutdown()
        self._node.destroy_node()

    def receive(self, timeout_ms: int) -> dict | None:
        deadline = time.monotonic() + timeout_ms / 1000.0
        while time.monotonic() < deadline:
            with self._lock:
                if self._queue:
                    return self._queue.popleft()
            time.sleep(0.001)
        return None


class _DdsBackend:
    """DDS subscriber using cyclonedds Python bindings.

    Requires cyclonedds pip package matching the C library version.
    Install with: pip install cyclonedds==0.10.5
    Build against the same CycloneDDS install as the server:
      CMAKE_PREFIX_PATH=$CYCLONEDDS_HOME pip install cyclonedds==0.10.5
    """
    def __init__(self, topic: str):
        self._topic = topic
        self._reader = None
        self._participant = None
        self._JoyData = None

    def connect(self):
        try:
            from cyclonedds.domain import DomainParticipant
            from cyclonedds.topic import Topic
            from cyclonedds.sub import Subscriber, DataReader
            from cyclonedds.idl import make_idl_struct
            from cyclonedds.idl.types import bounded_str, sequence, float32, int32, uint64

            self._JoyData = make_idl_struct("JoyData", "joy_data::JoyData", {
                "axes": sequence[float32],
                "buttons": sequence[int32],
                "timestamp_ns": uint64,
                "frame_id": bounded_str[32],
            })

            self._participant = DomainParticipant()
            self._topic_obj = Topic(self._participant, self._topic, self._JoyData)
            self._subscriber = Subscriber(self._participant)
            self._reader = DataReader(self._subscriber, self._topic_obj)
            print(f"JoystickClient[DDS]: subscribed to {self._topic}")

        except ImportError:
            raise RuntimeError(
                "DDS backend requires 'cyclonedds' pip package. "
                "Install with: CMAKE_PREFIX_PATH=<cyclonedds_install> pip install cyclonedds==0.10.5"
            )

    def disconnect(self):
        if self._reader:
            del self._reader
        if self._participant:
            del self._participant

    def receive(self, timeout_ms: int) -> dict | None:
        import time

        deadline = time.monotonic() + timeout_ms / 1000.0
        while time.monotonic() < deadline:
            samples = self._reader.take(N=1)
            if samples:
                s = samples[0]
                return {
                    "axes": list(s.axes),
                    "buttons": list(s.buttons),
                    "ts_ns": s.timestamp_ns,
                    "frame_id": s.frame_id,
                }
            time.sleep(0.001)
        return None
