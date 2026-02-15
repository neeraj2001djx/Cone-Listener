from flask import Flask, request, jsonify
from datetime import datetime

app = Flask(__name__)

# Store online devices
online_devices = {}

@app.route("/register")
def register():
    device_id = request.args.get("id")
    ip = request.remote_addr

    online_devices[device_id] = {
        "ip": ip,
        "last_seen": datetime.now().strftime("%H:%M:%S")
    }

    print(f"[REGISTER] {device_id} from {ip}")
    return "OK"

@app.route("/list")
def list_devices():
    return jsonify(online_devices)

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000)