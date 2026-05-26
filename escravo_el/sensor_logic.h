#pragma once

// --- Public Sensor Logic API ---

// Initializes the VL6180X sensor with optimal non-blocking settings.
void setupSensor();

// Reads the water level using a non-blocking single measurement + EMA filter.
// Returns the water level in mm, or -1.0 if a persistent sensor timeout occurs.
float readWaterLevel();
