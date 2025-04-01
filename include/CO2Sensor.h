#ifndef CO2SENSOR_H
#define CO2SENSOR_H

#include <Arduino.h>
#include <Wire.h>
#include <SensirionI2CScd4x.h>
#include "Display.h"  // For SensorData struct

class CO2Sensor {
public:
  // Constructor
  CO2Sensor(int co2AlarmThreshold);
  
  // Initialize the sensor
  bool begin();
  
  // Update sensor data (returns true if data was updated)
  bool update();
  
  // Get the current sensor data
  SensorData getData() const;
  
  // Check if the sensor is connected
  bool isConnected() const;
  
  // Get the number of valid readings since initialization
  uint8_t getValidReadingCount() const;
  
  // Reset the sensor
  bool reset();
  
private:
  SensirionI2CScd4x _scd4x;
  SensorData _currentData;
  bool _connected;
  uint8_t _validReadingCount;
  int _co2AlarmThreshold;
  
  // Internal methods
  bool checkConnection();
  void scanI2CBus();
  bool stopMeasurement();
  bool startMeasurement();
};

#endif // CO2SENSOR_H 