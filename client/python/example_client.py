#!/usr/bin/env python3
"""Example: Joystick client — displays mapped axes/buttons in a fixed layout.

Works with ZMQ, ROS2, or DDS backend depending on YAML config.

Usage:
    python3 client/python/example_client.py config/xbox.yaml       # ZMQ/ROS2/DDS
"""

import sys
import signal
from joylink_client import JoylinkClient

running = True


def signal_handler(sig, frame):
    global running
    running = False


def axis_line(n1, v1, n2, v2):
    """Format two axis name/value pairs with fixed column widths."""
    return f"{n1:>4s} {v1:+7.3f}  {n2:>4s} {v2:+7.3f}"


def btn_str(buttons):
    """Format all buttons, highlighting pressed ones with brackets."""
    parts = []
    for name, val in buttons.items():
        if val:
            parts.append(f"[{name}]")
        else:
            parts.append(f" {name} ")
    return "".join(parts)


def print_frame(data):
    """Print one complete frame, overwriting the terminal."""
    d = data
    print("\033[H")  # cursor home, overwrite previous frame

    print("┌─────────────── Axes ───────────────┬── Buttons ───────────────────┐")
    print(f"│ {axis_line('LX', d['axes']['left_stick_x'],  'RX', d['axes']['right_stick_x'])}  │")
    print(f"│ {axis_line('LY', d['axes']['left_stick_y'],  'RY', d['axes']['right_stick_y'])}  │")
    print(f"│ {axis_line('LT', d['axes']['left_trigger'],  'RT', d['axes']['right_trigger'])}  │")
    print(f"│ {axis_line('DX', d['axes']['dpad_x'],        'DY', d['axes']['dpad_y'])}       │")
    print("├──────────────────────────────────────┼─────────────────────────────┤")
    # Buttons: two rows for PS4 (6+6), one row for Xbox (5+5)
    names = list(d["buttons"].keys())
    mid = (len(names) + 1) // 2
    row1 = btn_str({k: d["buttons"][k] for k in names[:mid]})
    row2 = btn_str({k: d["buttons"][k] for k in names[mid:]})
    print(f"│ {row1} │")
    print(f"│ {row2} │")
    print("└──────────────────────────────────────┴─────────────────────────────┘")


def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <config.yaml>")
        sys.exit(1)

    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

    client = JoylinkClient(sys.argv[1])
    client.connect()

    print("\033[2J\033[H")  # clear screen
    print("Waiting for joystick data... Press Ctrl+C to stop.\n")

    while running:
        data = client.receive(timeout_ms=1000)
        if data:
            print_frame(data)

    client.close()
    print("\nDone.")


if __name__ == "__main__":
    main()
