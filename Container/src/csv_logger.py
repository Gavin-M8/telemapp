from datetime import datetime
import os

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

    def __init__(self, directory="logs"):
        """
        Initialize the CSV logger.

        Args:
            directory (str): Directory to save CSV files.
        """
        self.directory = directory
        os.makedirs(directory, exist_ok=True)
        self.file = None
        self.filepath = None
        self.start_new_log()  # create first log file

    def start_new_log(self):
        """
        Create a new CSV file with a unique timestamped filename.
        Ensures the file does not already exist.
        """
        if self.file:
            self.file.close()

        # create unique filename with milliseconds
        while True:
            human_ts_filename = datetime.now().strftime("%Y-%m-%d_%H-%M-%S-%f")[:-3]  # include ms
            filepath = os.path.join(self.directory, f"telemetry-{human_ts_filename}.csv")
            if not os.path.exists(filepath):
                break  # unique name found

        self.filepath = filepath
        self.file = open(self.filepath, "w", buffering=1)  # line-buffered
        self.file.write("timestamp,human_ts,ax,ay,az\n")  # header

    def write(self, timestamp, human_ts, ax, ay, az):
        """
        Write a row to the CSV file.

        Args:
            timestamp (float): Unix epoch seconds
            ax (float): acceleration x-axis
            ay (float): acceleration y-axis
            az (float): acceleration z-axis
        """
        self.file.write(f"{timestamp},{human_ts},{ax},{ay},{az}\n")

    def flush(self):
        """
        Close the current CSV file and start a new one with a unique timestamp.
        """
        self.start_new_log()
