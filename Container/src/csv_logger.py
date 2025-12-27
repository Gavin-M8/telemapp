import csv
import threading
import os

class CSVLogger:
    def __init__(self, filepath):
        """
        Initialize a CSV logger.
        :param filepath: Path to CSV file
        """
        self.filepath = filepath
        self.lock = threading.Lock()

        # Ensure directory exists
        os.makedirs(os.path.dirname(filepath), exist_ok=True)

        # Open file in append mode
        self.file = open(filepath, mode='a', newline='')
        self.writer = csv.writer(self.file)

        # Write header if file is empty
        if os.path.getsize(filepath) == 0:
            self.writer.writerow(["timestamp", "ax", "ay", "az"])
            self.file.flush()

    def write(self, timestamp, ax, ay, az):
        """
        Append a row to the CSV file.
        :param timestamp: Timestamp in ms
        :param ax: Acceleration X
        :param ay: Acceleration Y
        :param az: Acceleration Z
        """
        with self.lock:
            self.writer.writerow([timestamp, ax, ay, az])
            self.file.flush()  # Flush immediately to reduce data loss

    def close(self):
        """Close the CSV file cleanly."""
        with self.lock:
            if not self.file.closed:
                self.file.close()
