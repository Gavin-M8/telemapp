import threading
import time
import math
from datetime import datetime

class DummyReader:
    def __init__(self, buffer, csv_logger=None, sample_rate_hz=50):
        self.buffer = buffer
        self.csv_logger = csv_logger
        self.sample_period = 1.0 / sample_rate_hz

        self.running = False
        self.thread = None
        self.t0 = time.time()

    def start(self):
        if self.running:
            print("DummyReader already running")
            return
        print("DummyReader starting thread")
        self.running = True
        self.thread = threading.Thread(target=self._run, daemon=True)
        self.thread.start()

    def stop(self, wait=True):
        """
        Stop the reader thread.

        Args:
            wait (bool): If True, wait for the thread to fully exit.
        """
        self.running = False
        if self.thread and wait:
            self.thread.join(timeout=2)
            self.thread = None

    def _run(self):
        print("DummyReader thread entered _run()")

        # sample counter for debuggung
        self._counter = 0   # initialize once per thread

        while self.running:
            t = time.time() - self.t0
            timestamp = time.time()  # seconds since epoch
            human_ts = datetime.fromtimestamp(timestamp).strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]
            # e.g. "2026-01-19 14:45:06.859"

            # Fake accelerometer signals
            ax = math.sin(2 * math.pi * 0.5 * t)
            ay = math.sin(2 * math.pi * 0.7 * t + 1)
            az = 0.5 * math.sin(2 * math.pi * 1.0 * t)

            self.buffer.add(timestamp, human_ts, ax, ay, az)

            if self.csv_logger:
                self.csv_logger.write(timestamp, human_ts, ax, ay, az)

            # sample counter for debugging
            self._counter += 1
            if self._counter % 100 == 0:
                print(f"Generated {self._counter} samples")

            time.sleep(self.sample_period)
