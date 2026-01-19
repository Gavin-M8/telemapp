import os
from flask import Flask, jsonify, send_from_directory
from telemetry_buffer import TelemetryBuffer
from csv_logger import CSVLogger
from serial_reader import SerialReader
import logging

log = logging.getLogger('werkzeug')
log.setLevel(logging.ERROR)


# ------------------------------
# Configuration from environment
# ------------------------------
USE_DUMMY = os.getenv("USE_DUMMY", "false").lower() == "true"
SERIAL_PORT = os.getenv("SERIAL_PORT", "/dev/ttyUSB0")
BAUD_RATE = int(os.getenv("BAUD_RATE", 19200))
CSV_PATH = os.getenv("CSV_PATH", "logs/telemetry.csv")
BUFFER_SIZE = int(os.getenv("BUFFER_SIZE", 200))

WEB_FOLDER = os.path.join(os.path.dirname(os.path.abspath(__file__)), "../web")

# ------------------------------
# Initialize modules
# ------------------------------
buffer = TelemetryBuffer(maxlen=BUFFER_SIZE)
csv_logger = CSVLogger(CSV_PATH)
if USE_DUMMY:
    from dummy_reader import DummyReader
    reader = DummyReader(buffer, csv_logger)
else:
    from serial_reader import SerialReader
    reader = SerialReader(SERIAL_PORT, BAUD_RATE, buffer, csv_logger)

# ------------------------------
# Flask app
# ------------------------------
app = Flask(__name__, static_folder=WEB_FOLDER, template_folder=WEB_FOLDER)

@app.route('/')
def index():
    return send_from_directory(WEB_FOLDER, 'index.html')

@app.route('/data')
def get_data():
    """
    Return current telemetry buffer as JSON for live plotting.
    """
    return jsonify(buffer.get_all())

@app.route("/api/start", methods=["POST"])
def start_reader():
    print("API: start_reader called")
    reader.start()
    return {"status": "started"}

@app.route("/api/stop", methods=["POST"])
def stop_reader():
    reader.stop()
    return {"status": "stopped"}

@app.route("/api/status")
def reader_status():
    return {
        "running": reader.running
    }

# ------------------------------
# App start / shutdown hooks
# ------------------------------
if __name__ == '__main__':
    try:
        print("Starting telemetry source...")
        reader.start()
        app.run(
            host="0.0.0.0",
            port=5000,
            debug=False,
            use_reloader=False
        )
    finally:
        reader.stop()
        csv_logger.close()

