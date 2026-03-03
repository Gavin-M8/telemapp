# Telemapp

Note: look at README in Container directory for more info

## Features

### Raw Data Collection
- Reads 3-axis accelerometer data from Arduino/ESP32 via serial (19200 baud)
- Buffers recent data in memory for live visualization
- Logs all data to timestamped CSV files

### Real-Time Derived Telemetry
- **Session Statistics**: Track max acceleration, braking, and lateral g-forces
- **Velocity Estimation**: Real-time velocity calculation via integration
- **0-60 mph Timer**: Automatic acceleration run timing
- **Event Detection**: Automatically detects and tracks corners and braking events
- **G-G Diagram**: Visualizes your vehicle's traction envelope
- **Data Smoothing**: Moving average filter reduces sensor noise

### Live Web Dashboard
- Beautiful, responsive web interface
- Real-time performance metrics display
- Interactive charts (time-series + G-G diagram)
- Session statistics and event counters
- Visual indicators for active events (cornering, braking, acceleration runs)

## File Structure

```
├── app_updated.py              # Main Flask application (UPDATED)
├── telemetry_processor.py      # Real-time telemetry processing (NEW)
├── telemetry_buffer.py         # In-memory data buffer
├── csv_logger.py               # CSV file logging
├── serial_reader_updated.py    # Arduino/ESP32 serial interface (UPDATED)
├── dummy_reader_updated.py     # Simulated data source for testing (UPDATED)
└── index_updated.html          # Web dashboard (UPDATED)
```

## Quick Start

### 1. Update Your Files

Replace your existing files with the updated versions:

```bash
# Backup your current files first!
cp app.py app_backup.py
cp serial_reader.py serial_reader_backup.py
cp dummy_reader.py dummy_reader_backup.py
cp index.html index_backup.html

# Then replace with updated versions
cp app_updated.py app.py
cp serial_reader_updated.py serial_reader.py
cp dummy_reader_updated.py dummy_reader.py
cp index_updated.html ../web/index.html

# Add the new processor
cp telemetry_processor.py .
```

### 2. Test with Dummy Data

```bash
# Set USE_DUMMY environment variable
export USE_DUMMY=true

# Run the app
python app.py
```

Open your browser to `http://localhost:5000` and you should see:
- Real-time acceleration charts updating
- Performance metrics updating (max g-forces, velocity, etc.)
- G-G diagram filling in with data points

### 3. Connect Your Arduino/ESP32

Once tested, connect your real hardware:

```bash
# Set your serial port
export USE_DUMMY=false
export SERIAL_PORT=/dev/ttyUSB0  # or /dev/ttyACM0, COM3, etc.
export BAUD_RATE=19200

# Run the app
python app.py
```

## Dashboard Guide

### Performance Metrics

**Max Acceleration** - Highest forward acceleration recorded (g-forces)
**Max Braking** - Highest braking force recorded (g-forces)  
**Max Lateral** - Highest cornering force in either direction (g-forces)
**Velocity** - Current estimated speed (mph/kph)
**0-60 mph Time** - Time to accelerate from 0 to 60 mph (auto-detected)
**Total Corners** - Number of cornering events detected
**Braking Events** - Number of hard braking events detected
**Current Accel** - Live smoothed longitudinal acceleration

### Charts

**Real-Time Acceleration** - Time-series plot showing Ax (longitudinal), Ay (lateral), Az (vertical) over time

**G-G Diagram** - Scatter plot of lateral vs longitudinal acceleration. The outer boundary shows your vehicle's performance envelope. A perfect circle means balanced acceleration in all directions.

### Controls

**▶ Start** - Begin data collection from sensor
**⏸ Stop** - Pause data collection
**New CSV Log** - Start a new CSV file and reset stats
**⏹ Stop Logging** - Stop writing to CSV (data still displays)
**Reset Stats** - Reset performance metrics without stopping logging
**Delete All Logs** - Remove all CSV files (use with caution!)

## Arduino/ESP32 Setup

Your Arduino should output data in this format via serial:

```
timestamp,ax,ay,az
1234567890,0.05,-0.12,0.98
1234567891,0.15,-0.18,1.02
```

Where:
- `timestamp` - Milliseconds since boot (from `millis()`)
- `ax` - Longitudinal acceleration in g-forces
- `ay` - Lateral acceleration in g-forces  
- `az` - Vertical acceleration in g-forces

Example Arduino code:

```cpp
#include <Wire.h>
// Include your accelerometer library here

void setup() {
  Serial.begin(19200);
  // Initialize your accelerometer
}

void loop() {
  float ax = readAccelX();  // Your function to read accel
  float ay = readAccelY();
  float az = readAccelZ();
  
  Serial.print(millis());
  Serial.print(",");
  Serial.print(ax, 3);
  Serial.print(",");
  Serial.print(ay, 3);
  Serial.print(",");
  Serial.println(az, 3);
  
  delay(20);  // 50 Hz sample rate
}
```

## Understanding the Metrics

### Velocity Integration

The system estimates velocity by integrating longitudinal acceleration over time. **Important limitations:**

- Drift accumulates over time
- Best for short bursts (acceleration runs, single corners)
- Resets when starting a new log
- Adding GPS would eliminate drift

### 0-60 mph Detection

Automatically starts timing when it detects sustained forward acceleration (>0.2g). Completes when velocity reaches 26.8 m/s (60 mph).

The card will pulse when an acceleration run is in progress!

### Event Detection

**Corners** - Detected when lateral acceleration exceeds 0.2g
**Braking** - Detected when longitudinal acceleration drops below -0.3g

These thresholds can be adjusted in `telemetry_processor.py`:

```python
self.corner_threshold = 0.2     # Minimum g-force for corner
self.braking_threshold = -0.3   # Minimum g-force for braking
```

## API Endpoints

The system exposes these new endpoints:

```
GET  /api/stats         - Get all derived statistics
GET  /api/gg_data       - Get G-G diagram data points
GET  /api/current       - Get current smoothed acceleration
POST /api/reset_stats   - Reset all statistics
```

Existing endpoints:
```
GET  /data              - Raw telemetry buffer
POST /api/start         - Start data reader
POST /api/stop          - Stop data reader
POST /api/start_log     - Start new CSV log
POST /api/stop_log      - Stop CSV logging
POST /api/delete_logs   - Delete all CSV files
GET  /api/status        - Reader status
```

## Configuration

Adjust processor settings in `telemetry_processor.py`:

```python
# Detection thresholds
self.corner_threshold = 0.2          # g-force to trigger corner detection
self.braking_threshold = -0.3        # g-force to trigger braking detection
self.accel_run_threshold = 0.2       # g-force to start 0-60 timer

# Smoothing window
self.recent_ax = deque(maxlen=10)    # 10 samples for moving average
```

## Troubleshooting

**No data showing up:**
- Check serial port is correct (`ls /dev/tty*` on Linux/Mac)
- Verify baud rate matches Arduino (19200)
- Check Arduino is actually sending data (open Serial Monitor)

**Velocity seems wrong:**
- Integration drift is normal over time
- Reset stats at the start of each run
- Ensure accelerometer is mounted level with the vehicle

**Charts not updating:**
- Check browser console for errors
- Verify Flask server is running
- Try refreshing the page

**Stats seem inaccurate:**
- Calibrate your accelerometer
- Adjust detection thresholds in processor
- Ensure sensor axes align with vehicle (X=forward, Y=left, Z=up)

## CSV Output Format

Logged CSV files contain:

```csv
timestamp,human_ts,ax,ay,az
1738123456.789,2026-02-06 14:23:45.789,0.05,-0.12,0.98
```

You can later analyze these with Python:

```python
import pandas as pd
import matplotlib.pyplot as plt

df = pd.read_csv('telemetry-20260206-142345.csv')

# Calculate magnitude
df['magnitude'] = (df['ax']**2 + df['ay']**2 + df['az']**2)**0.5

# Plot
df.plot(x='timestamp', y=['ax', 'ay', 'az'])
plt.show()
```

## Tips for Best Results

1. **Mount the accelerometer level** - Align axes with vehicle (X=forward/back, Y=left/right, Z=up/down)

2. **Calibrate your sensor** - Most accelerometers need calibration to read exactly 1g when stationary

3. **Reset stats before each run** - Click "Reset Stats" before driving to get clean metrics

4. **Start with dummy data** - Test the system without hardware first

5. **Use "New CSV Log" for each run** - This creates a timestamped file and resets stats automatically

6. **Review your G-G diagram** - It shows where you're pushing limits and where you have room to improve

## Future Enhancements

Potential additions:
- GPS integration for accurate velocity (eliminates drift!)
- Gyroscope for roll/pitch/yaw angles
- Temperature sensors for brakes/tires
- Wheel speed sensors for slip detection
- Data export/comparison tools
- Lap timing with GPS coordinates
- Mobile app for in-car display
