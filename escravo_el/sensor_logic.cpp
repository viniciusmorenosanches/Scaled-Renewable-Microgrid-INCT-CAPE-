#include "sensor_logic.h"
#include <Arduino.h>
#include <Wire.h>
#include <VL6180X.h>

// --- Configuration Constants ---
static const float CUP_HEIGHT = 47.0;       // Container height in mm
static const float EMA_ALPHA = 0.05;        // Smoothing factor (0.0 to 1.0, lower means more filtering)
static const int MAX_ALLOWED_ERRORS = 3;    // Number of consecutive timeouts before reporting failure

// Static/internal variables
static VL6180X sensor;
static float filteredDistance = -1.0; 
static int consecutiveErrors = 0;

void setupSensor() {
  sensor.init();
  sensor.configureDefault();
  sensor.setTimeout(50); // Set a fast 50ms timeout for non-blocking responsiveness

  // Optimize convergence and averaging sample periods
  sensor.writeReg(VL6180X::SYSRANGE__MAX_CONVERGENCE_TIME, 30);
  sensor.writeReg(VL6180X::READOUT__AVERAGING_SAMPLE_PERIOD, 10);
}

float readWaterLevel() {
  // 1. Take a single, quick range reading (takes approx. 10ms execution time)
  uint8_t rawRange = sensor.readRangeSingleMillimeters();

  // 2. Check for sensor timeouts or read failures (255 is an error sentinel for this sensor)
  if (sensor.timeoutOccurred() || rawRange == 255) {
    consecutiveErrors++;
    if (consecutiveErrors >= MAX_ALLOWED_ERRORS) {
      return -1.0; // Return error indicator to the control loop
    }
    // For transient errors, skip the current reading and retain the last filtered value
  } else {
    consecutiveErrors = 0; // Reset error counter on a successful read

    // 3. Apply Exponential Moving Average (EMA) filter
    if (filteredDistance < 0) {
      filteredDistance = rawRange; // Initialize filter on first successful reading
    } else {
      filteredDistance = (EMA_ALPHA * rawRange) + ((1.0 - EMA_ALPHA) * filteredDistance);
    }
  }

  // If we haven't received a single valid reading yet, return 0.0 to prevent negative values
  if (filteredDistance < 0) {
    return 0.0;
  }

  // 4. Calculate inverted water level
  float waterLevelDistance = CUP_HEIGHT - filteredDistance;
  if (waterLevelDistance < 0) {
    waterLevelDistance = 0.0;
  }

  // 5. Apply custom calibration scaling (15/16) without rounding
  waterLevelDistance = waterLevelDistance * 0.9375;

  return waterLevelDistance;
}
