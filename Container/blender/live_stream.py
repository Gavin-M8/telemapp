import bpy
import urllib.request
import json
import time

# ---------------- CONFIG ----------------
TELEMETRY_URL = "http://localhost:5000/data"
POLL_HZ = 20
SCALE = 0.1
# ----------------------------------------

obj_name = "TelemetryObject"

# Create object if needed
if obj_name not in bpy.data.objects:
    mesh = bpy.data.meshes.new("TelemetryMesh")
    obj = bpy.data.objects.new(obj_name, mesh)
    bpy.context.collection.objects.link(obj)
else:
    obj = bpy.data.objects[obj_name]

last_timestamp = None

def fetch_latest_sample():
    try:
        with urllib.request.urlopen(TELEMETRY_URL, timeout=1) as response:
            data = json.loads(response.read().decode())
            if not data:
                return None
            return data[-1]
    except Exception as e:
        print("Telemetry fetch error:", e)
        return None

def update_object():
    global last_timestamp

    sample = fetch_latest_sample()
    if not sample:
        return POLL_INTERVAL

    ts = sample["timestamp"]
    if ts == last_timestamp:
        return POLL_INTERVAL

    last_timestamp = ts

    ax = sample["ax"]
    ay = sample["ay"]
    az = sample["az"]

    # Map acceleration directly to motion (visualization)
    obj.location = (
        ax * SCALE,
        ay * SCALE,
        az * SCALE
    )

    return POLL_INTERVAL

POLL_INTERVAL = 1.0 / POLL_HZ

# Register Blender timer
if not bpy.app.timers.is_registered(update_object):
    bpy.app.timers.register(update_object)

print("Live telemetry streaming started.")
