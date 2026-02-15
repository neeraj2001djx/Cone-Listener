import requests
import json

SERVER = "http://192.168.214.95:5000"   # CHANGE to your PC IP

# Load device-file mapping
with open("devices.json") as f:
    config = json.load(f)

# Get online devicess
response = requests.get(SERVER + "/list")
online_devices = response.json()

print("Online devices:")
print(online_devices)
print("")

# Upload to each connected ESP
for device in config["devices"]:
    device_id = device["id"]

    if device_id in online_devices:
        ip = online_devices[device_id]["ip"]
        file_path = device["file"]

        print(f"Uploading {file_path} to {device_id} ({ip})")

        try:
            with open(file_path, "rb") as f:
                files = {"file": f}
                r = requests.post(f"http://{ip}/upload", files=files)
                print("Response:", r.text)
        except Exception as e:
            print("Upload failed:", e)

    else:
        print(f"{device_id} not online")

print("\nDone.")
