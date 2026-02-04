import serial
import threading
from telemetry_buffer import TelemetryBuffer
from csv_logger import CSVLogger
from datetime import datetime


class SerialReader:
    def __init__(self, port, baud_rate, buffer: TelemetryBuffer, csv_logger: CSVLogger):
        """
        Initialize the serial reader.
        :param port: Serial port (e.g., /dev/ttyUSB0)
        :param baud_rate: Baud rate for Arduino serial
        :param buffer: TelemetryBuffer instance
        :param csv_logger: CSVLogger instance
        """
        self.port = port
        self.baud_rate = baud_rate
        self.buffer = buffer
        self.csv_logger = csv_logger
        self.thread = None
        self.running = False

    def start(self):
        """Start the serial reading thread."""
        if self.thread is None:
            self.running = True
            self.thread = threading.Thread(target=self._read_loop, daemon=True)
            self.thread.start()

    def stop(self):
        """Stop the serial reading thread."""
        self.running = False
        if self.thread is not None:
            self.thread.join()
            self.thread = None

    def _read_loop(self):
        """Internal loop that continuously reads serial data."""
        try:
            with serial.Serial(self.port, self.baud_rate, timeout=1) as ser:
                while self.running:
                    line = ser.readline().decode('utf-8', errors='ignore').strip()
                    if not line:
                        continue
                    # Skip CSV header if present
                    if "timestamp" in line.lower():
                        continue
                    parts = line.split(",")
                    if len(parts) != 4:
                        continue  # Malformed line
                    try:
                        timestamp = int(parts[0])  # epoch ms or seconds, from Arduino
                        
                        ax = float(parts[1])
                        ay = float(parts[2])
                        az = float(parts[3])
                        
                        human_ts = datetime.fromtimestamp(timestamp / 1000).strftime(
                            '%Y-%m-%d %H:%M:%S.%f'
                        )[:-3]

                    except ValueError:
                        continue  # Skip lines with invalid numbers

                    self.buffer.add(timestamp, human_ts, ax, ay, az)
                    self.csv_logger.write(timestamp, human_ts, ax, ay, az)

        except serial.SerialException as e:
            print(f"Serial error on {self.port}: {e}")
