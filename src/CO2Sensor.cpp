#include "CO2Sensor.h"

CO2Sensor::CO2Sensor(int co2AlarmThreshold)
  : _connected(false),
    _validReadingCount(0),
    _co2AlarmThreshold(co2AlarmThreshold) {
  // Initialize default sensor data
  _currentData.co2 = 400;         // Default CO2 level (outdoor fresh air)
  _currentData.temperature = 20.0; // Default temperature
  _currentData.humidity = 50.0;    // Default humidity
}

bool CO2Sensor::begin() {
  Serial.println("Initializing CO2 sensor...");
  
  // Initialize the sensor
  _scd4x.begin(Wire);
  
  // Allow time for the sensor to boot up
  delay(1000);
  
  // Check if sensor is connected
  if (!checkConnection()) {
    Serial.println("ERROR: Failed to connect to SCD4x sensor");
    _connected = false;
    return false;
  }
  
  // Stop any ongoing measurements
  if (!stopMeasurement()) {
    Serial.println("ERROR: Failed to stop ongoing measurements");
    _connected = false;
    return false;
  }
  
  // Wait for a moment
  delay(500);
  
  // Start periodic measurements
  if (!startMeasurement()) {
    Serial.println("ERROR: Failed to start measurements");
    _connected = false;
    return false;
  }
  
  // Allow time for the sensor to initialize and take first measurements
  Serial.println("Waiting for sensor to initialize (15 seconds)...");
  for (int i = 0; i < 15; i++) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println();
  
  _connected = true;
  _validReadingCount = 0;
  
  Serial.println("CO2 sensor initialized successfully");
  return true;
}

bool CO2Sensor::update() {
  if (!_connected) {
    Serial.println("Sensor not connected, skipping update");
    return false;
  }
  
  bool isDataReady = false;
  uint16_t error = _scd4x.getDataReadyFlag(isDataReady);
  
  if (error) {
    Serial.print("ERROR: Failed to check data ready flag. Error code: ");
    Serial.println(error);
    _connected = false;
    _validReadingCount = 0;
    return false;
  }
  
  if (!isDataReady) {
    Serial.println("Data not ready yet, waiting...");
    return false;
  }
  
  // Read measurement
  uint16_t co2;
  float temperature;
  float humidity;
  
  error = _scd4x.readMeasurement(co2, temperature, humidity);
  if (error) {
    Serial.print("ERROR: Failed to read measurement. Error code: ");
    Serial.println(error);
    _connected = false;
    _validReadingCount = 0;
    return false;
  }
  
  // Validate readings
  if (co2 == 0) {
    Serial.println("ERROR: Invalid CO2 reading (value = 0)");
    return false;
  }
  
  // Store the readings
  _currentData.co2 = co2;
  _currentData.temperature = temperature;
  _currentData.humidity = humidity;
  
  // Increment valid reading count up to max of 5
  if (_validReadingCount < 5) {
    _validReadingCount++;
    Serial.print("Valid reading count: ");
    Serial.println(_validReadingCount);
  }
  
  // Debug output
  Serial.print("CO2: ");
  Serial.print(_currentData.co2);
  Serial.print(" ppm, Temp: ");
  Serial.print(_currentData.temperature);
  Serial.print(" C, Humidity: ");
  Serial.print(_currentData.humidity);
  Serial.println("%");
  
  return true;
}

SensorData CO2Sensor::getData() const {
  return _currentData;
}

bool CO2Sensor::isConnected() const {
  return _connected;
}

uint8_t CO2Sensor::getValidReadingCount() const {
  return _validReadingCount;
}

bool CO2Sensor::reset() {
  // Stop ongoing measurements
  if (!stopMeasurement()) {
    return false;
  }
  
  // Wait for sensor to settle
  delay(1000);
  
  // Retry initialization
  return begin();
}

bool CO2Sensor::checkConnection() {
  Serial.println("Checking sensor connection...");
  
  // Try to get serial number as a connectivity test
  uint16_t serial0;
  uint16_t serial1;
  uint16_t serial2;
  uint16_t error = _scd4x.getSerialNumber(serial0, serial1, serial2);
  
  if (error) {
    Serial.print("ERROR: Failed to get serial number. Error code: ");
    Serial.println(error);
    
    // Scan I2C bus to help with debugging
    scanI2CBus();
    return false;
  }
  
  Serial.print("Sensor serial number: ");
  Serial.print(serial0, HEX);
  Serial.print(serial1, HEX);
  Serial.println(serial2, HEX);
  Serial.println("Sensor connection verified");
  return true;
}

void CO2Sensor::scanI2CBus() {
  Serial.println("Scanning I2C bus for devices...");
  byte error, address;
  int deviceCount = 0;
  
  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    
    if (error == 0) {
      Serial.print("I2C device found at address 0x");
      if (address < 16) {
        Serial.print("0");
      }
      Serial.print(address, HEX);
      
      // SCD40 should be at address 0x62
      if (address == 0x62) {
        Serial.println(" (SCD40 sensor)");
      } else {
        Serial.println();
      }
      deviceCount++;
    }
  }
  
  if (deviceCount == 0) {
    Serial.println("No I2C devices found - check wiring");
  } else {
    Serial.println("I2C scan complete, found " + String(deviceCount) + " devices");
    Serial.println("SCD40 sensor should be at address 0x62");
  }
}

bool CO2Sensor::stopMeasurement() {
  Serial.println("Stopping ongoing measurements...");
  uint16_t error = _scd4x.stopPeriodicMeasurement();
  
  if (error) {
    Serial.print("ERROR: Failed to stop measurement. Error code: ");
    Serial.println(error);
    return false;
  }
  
  Serial.println("Successfully stopped measurements");
  return true;
}

bool CO2Sensor::startMeasurement() {
  Serial.println("Starting periodic measurements...");
  uint16_t error = _scd4x.startPeriodicMeasurement();
  
  if (error) {
    Serial.print("ERROR: Failed to start measurement. Error code: ");
    Serial.println(error);
    return false;
  }
  
  Serial.println("Successfully started periodic measurements");
  return true;
} 