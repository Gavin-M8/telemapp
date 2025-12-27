from collections import deque
import threading

class TelemetryBuffer:
    def __init__(self, maxlen=200):
        """
        Initialize an in-memory buffer for telemetry data.
        :param maxlen: Maximum number of recent samples to keep
        """
        self.buffer = deque(maxlen=maxlen)
        self.lock = threading.Lock()  # Thread-safe access

    def add(self, timestamp, ax, ay, az):
        """
        Add a new telemetry sample.
        :param timestamp: Timestamp in ms
        :param ax: Acceleration X
        :param ay: Acceleration Y
        :param az: Acceleration Z
        """
        with self.lock:
            self.buffer.append({
                "timestamp": timestamp,
                "ax": ax,
                "ay": ay,
                "az": az
            })

    def get_all(self):
        """
        Return a copy of all samples in the buffer.
        :return: List of dicts
        """
        with self.lock:
            return list(self.buffer)

    def clear(self):
        """
        Optional: Clear the buffer.
        """
        with self.lock:
            self.buffer.clear()
