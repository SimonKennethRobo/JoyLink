import zmq
import json

ctx = zmq.Context()
sock = ctx.socket(zmq.SUB)
sock.connect("udp://localhost:5555")
sock.setsockopt_string(zmq.SUBSCRIBE, "joy")
while True:
    topic = sock.recv_string()
    data = json.loads(sock.recv_string())
    print(f"axes: {data['axes']}, buttons: {data['buttons']}")
