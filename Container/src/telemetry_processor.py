import threading
import time
import math
from collections import deque

class TelemetryProcessor:
    """
    Real-time processor for deriving telemetry metrics from accelerometer data.
    
    Calculates:
    - Running max/min g-forces (accel, brake, lateral)
    - Velocity estimation via integration
    - G-G diagram data
    - Event detection (corners, braking, acceleration runs)
    - Performance metrics (0-60 time, etc.)
    """
    
    def __init__(self):
        self.lock = threading.Lock()
        
        # Session statistics
        self.max_accel = 0.0          # Max forward acceleration (g)
        self.max_brake = 0.0           # Max braking (positive g)
        self.max_lateral_left = 0.0    # Max left turn (positive g)
        self.max_lateral_right = 0.0   # Max right turn (positive g)
        self.max_vertical = 0.0        # Max vertical (g)
        
        # Velocity estimation
        self.velocity = 0.0            # Current velocity (m/s)
        self.last_timestamp = None
        
        # 0-60 tracking
        self.zero_to_sixty_time = None
        self.zero_to_sixty_started = False
        self.accel_run_start_time = None
        
        # Event detection
        self.in_corner = False
        self.corner_start_time = None
        self.corner_peak = 0.0
        self.corner_direction = None
        self.total_corners = 0
        
        self.in_braking = False
        self.braking_start_time = None
        self.braking_peak = 0.0
        self.total_braking_events = 0
        
        # Recent history for G-G diagram (last 500 points)
        self.gg_history = deque(maxlen=500)
        
        # Recent acceleration data for smoothing (last 10 samples)
        self.recent_ax = deque(maxlen=10)
        self.recent_ay = deque(maxlen=10)
        self.recent_az = deque(maxlen=10)
        
        # Configuration
        self.corner_threshold = 0.2     # g-force to detect corner
        self.corner_exit_threshold = 0.15
        self.braking_threshold = -0.3   # negative g-force to detect braking
        self.braking_exit_threshold = -0.1
        self.accel_run_threshold = 0.2  # g-force to start 0-60 timer
        
    def reset(self):
        """Reset all statistics (e.g., for a new run)"""
        with self.lock:
            self.max_accel = 0.0
            self.max_brake = 0.0
            self.max_lateral_left = 0.0
            self.max_lateral_right = 0.0
            self.max_vertical = 0.0
            
            self.velocity = 0.0
            self.last_timestamp = None
            
            self.zero_to_sixty_time = None
            self.zero_to_sixty_started = False
            self.accel_run_start_time = None
            
            self.in_corner = False
            self.corner_start_time = None
            self.corner_peak = 0.0
            self.corner_direction = None
            self.total_corners = 0
            
            self.in_braking = False
            self.braking_start_time = None
            self.braking_peak = 0.0
            self.total_braking_events = 0
            
            self.gg_history.clear()
            self.recent_ax.clear()
            self.recent_ay.clear()
            self.recent_az.clear()
    
    def process(self, timestamp, ax, ay, az):
        """
        Process a new telemetry sample and update derived metrics.
        
        :param timestamp: Unix timestamp (seconds)
        :param ax: Longitudinal acceleration (g)
        :param ay: Lateral acceleration (g)
        :param az: Vertical acceleration (g)
        """
        with self.lock:
            # Add to recent history for smoothing
            self.recent_ax.append(ax)
            self.recent_ay.append(ay)
            self.recent_az.append(az)
            
            # Calculate smoothed values (moving average)
            ax_smooth = sum(self.recent_ax) / len(self.recent_ax) if self.recent_ax else ax
            ay_smooth = sum(self.recent_ay) / len(self.recent_ay) if self.recent_ay else ay
            az_smooth = sum(self.recent_az) / len(self.recent_az) if self.recent_az else az
            
            # Update max/min statistics
            if ax_smooth > self.max_accel:
                self.max_accel = ax_smooth
            if ax_smooth < -self.max_brake:  # Store as positive value
                self.max_brake = -ax_smooth
            
            if ay_smooth > self.max_lateral_left:
                self.max_lateral_left = ay_smooth
            if ay_smooth < -self.max_lateral_right:  # Store as positive value
                self.max_lateral_right = -ay_smooth
            
            if abs(az_smooth) > self.max_vertical:
                self.max_vertical = abs(az_smooth)
            
            # Add to G-G diagram history
            self.gg_history.append({
                "ax": ax_smooth,
                "ay": ay_smooth
            })
            
            # Velocity integration
            if self.last_timestamp is not None:
                dt = timestamp - self.last_timestamp
                # Only integrate if dt is reasonable (< 1 second to avoid huge jumps)
                if 0 < dt < 1.0:
                    # Convert g to m/s² and integrate
                    self.velocity += ax_smooth * 9.81 * dt
                    
                    # Clamp velocity to zero if very small (drift correction)
                    if abs(self.velocity) < 0.1:
                        self.velocity = 0.0
            
            self.last_timestamp = timestamp
            
            # 0-60 mph tracking (26.8 m/s)
            if not self.zero_to_sixty_started and ax_smooth > self.accel_run_threshold:
                # Start acceleration run
                self.zero_to_sixty_started = True
                self.accel_run_start_time = timestamp
                self.velocity = 0.0  # Reset velocity at start of run
            
            if self.zero_to_sixty_started and self.zero_to_sixty_time is None:
                if self.velocity >= 26.8:  # 60 mph in m/s
                    self.zero_to_sixty_time = timestamp - self.accel_run_start_time
            
            # Corner detection
            lateral_g = abs(ay_smooth)
            
            if not self.in_corner and lateral_g > self.corner_threshold:
                # Corner entry
                self.in_corner = True
                self.corner_start_time = timestamp
                self.corner_peak = lateral_g
                self.corner_direction = "left" if ay_smooth > 0 else "right"
            
            if self.in_corner:
                # Track peak during corner
                if lateral_g > self.corner_peak:
                    self.corner_peak = lateral_g
                
                # Corner exit
                if lateral_g < self.corner_exit_threshold:
                    self.in_corner = False
                    self.total_corners += 1
            
            # Braking detection
            if not self.in_braking and ax_smooth < self.braking_threshold:
                # Braking start
                self.in_braking = True
                self.braking_start_time = timestamp
                self.braking_peak = ax_smooth
            
            if self.in_braking:
                # Track peak braking force
                if ax_smooth < self.braking_peak:
                    self.braking_peak = ax_smooth
                
                # Braking end
                if ax_smooth > self.braking_exit_threshold:
                    self.in_braking = False
                    self.total_braking_events += 1
    
    def get_stats(self):
        """
        Get current derived statistics as a dictionary.
        
        :return: Dict of all calculated metrics
        """
        with self.lock:
            return {
                # Session maximums
                "max_accel": round(self.max_accel, 3),
                "max_brake": round(self.max_brake, 3),
                "max_lateral_left": round(self.max_lateral_left, 3),
                "max_lateral_right": round(self.max_lateral_right, 3),
                "max_lateral": round(max(self.max_lateral_left, self.max_lateral_right), 3),
                "max_vertical": round(self.max_vertical, 3),
                
                # Velocity
                "velocity_ms": round(self.velocity, 2),
                "velocity_mph": round(self.velocity * 2.237, 2),  # Convert m/s to mph
                "velocity_kph": round(self.velocity * 3.6, 2),    # Convert m/s to km/h
                
                # Performance
                "zero_to_sixty_time": round(self.zero_to_sixty_time, 2) if self.zero_to_sixty_time else None,
                "zero_to_sixty_in_progress": self.zero_to_sixty_started and self.zero_to_sixty_time is None,
                
                # Events
                "in_corner": self.in_corner,
                "corner_peak": round(self.corner_peak, 3) if self.in_corner else None,
                "corner_direction": self.corner_direction if self.in_corner else None,
                "total_corners": self.total_corners,
                
                "in_braking": self.in_braking,
                "braking_peak": round(abs(self.braking_peak), 3) if self.in_braking else None,
                "total_braking_events": self.total_braking_events,
            }
    
    def get_gg_data(self):
        """
        Get G-G diagram data (lateral vs longitudinal acceleration).
        
        :return: List of {ax, ay} points
        """
        with self.lock:
            return list(self.gg_history)
    
    def get_smoothed_current(self):
        """
        Get current smoothed acceleration values.
        
        :return: Dict with smoothed ax, ay, az
        """
        with self.lock:
            return {
                "ax": round(sum(self.recent_ax) / len(self.recent_ax), 3) if self.recent_ax else 0.0,
                "ay": round(sum(self.recent_ay) / len(self.recent_ay), 3) if self.recent_ay else 0.0,
                "az": round(sum(self.recent_az) / len(self.recent_az), 3) if self.recent_az else 0.0,
            }