from datetime import datetime
import os
import time

class CSVLogger:
    """
    CSV Logger for telemetry data.

    Each row contains:
        timestamp (float): Unix epoch seconds
        human_ts (str): human-readable timestamp in HH:MM:SS.mmm
        ax (float): acceleration x-axis
        ay (float): acceleration y-axis
        az (float): acceleration z-axis

    The CSV files are named in the format:
        telemetry-YYYY-MM-DD_HH-MM-SS-ms.csv
    and saved in the specified directory.
    """

    def __init__(self, directory=None):
        """
        If directory is None, no logging occurs until start_log() is called.
        """
        self.directory = directory
        self.file = None
        self.filepath = None
        

    def start_log(self):
        """
        Start logging to a new CSV file using a timestamped filename.
        """

        if self.directory is None:
            raise RuntimeError("No logging directory specified.")
        
        t = time.localtime()
        human_ts = time.strftime("%Y%m%d-%H%M%S", t)
        self.filename = f"telemetry-{human_ts}.csv"

        os.makedirs(self.directory, exist_ok=True)
        self.filepath = os.path.join(self.directory, self.filename)
        self.file = open(self.filepath, "w")
        self.file.write("timestamp,human_ts,ax,ay,az\n")

        print(f"CSVLogger: started new log {self.filename}")


    def write(self, timestamp, human_ts, ax, ay, az):
        """
        Write a single row to the CSV file if logging is active.
        """
        if self.file:
            self.file.write(f"{timestamp},{human_ts},{ax},{ay},{az}\n")
            self.file.flush()


    def stop_log(self):
        """
        Stop logging and close the current CSV file.
        """
        if self.file:
            self.file.close()
            print(f"CSVLogger: stopped log {self.filename}")
            self.file = None
