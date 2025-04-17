#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include "Display.h"            // Our display class
#include "CO2Sensor.h"          // Our new sensor class

// Pin Definitions
#define BUZZER_PIN 25                       // Buzzer pin - adjust as needed
#define DATA_HISTORY_SIZE 48                // 24 hours of data with 30 min samples
#define CO2_ALARM_THRESHOLD 1000            // CO2 level to trigger alarm (ppm)
#define BUZZER_INTERVAL 600000              // Buzzer interval (10 minutes in ms)

// Threshold for value change that requires display update
#define CO2_THRESHOLD 50                   // 50 ppm difference
#define TEMP_THRESHOLD 0.5                 // 0.5°C difference
#define HUM_THRESHOLD 2                    // 2% difference

// LILYGO T5 v2.4.1 pins for e-Paper
#define EPD_BUSY 4
#define EPD_CS 5
#define EPD_RST 16
#define EPD_DC 17
#define EPD_SCK 18
#define EPD_MOSI 23

// Global variables
Display* display = nullptr;           // Our display object
CO2Sensor* co2Sensor = nullptr;       // Our CO2 sensor object

// Global variables for sensor values and history
SensorData currentData = {400, 20.0, 50.0};     // Default values
SensorData lastDisplayedData = {0, 0.0, 0.0};   // Last displayed values
uint16_t co2History[DATA_HISTORY_SIZE] = {0};   // History array for CO2 values

// Add history for temperature and humidity (last 12 readings)
float temperatureHistory[12] = {0};
float humidityHistory[12] = {0};
int miniHistoryIndex = 0;
int miniHistoryCount = 0;

int historyIndex = 0;                           // Current index in history array
unsigned long lastFullUpdateTime = 0;           // Time of last full display update
unsigned long lastDataUpdateTime = 0;           // Time of last data collection
unsigned long lastHistoryUpdateTime = 0;        // Time of last history update
unsigned long lastBuzzerTime = 0;               // Time of last buzzer activation
bool buzzerActive = false;                      // Track if buzzer is currently active

// Function prototypes
void updateDisplay(bool fullUpdate);
void updateHistory();
bool significantChange();
void checkAlarm();
void activateBuzzer(bool activate);
bool tryReconnectSensor();

void setup() {
  Serial.begin(115200);
  Serial.println("=== Starting CO2 Monitor with SCD40 sensor ===");
  
  // Setup buzzer pin
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  Serial.println("Buzzer pin initialized");
  
  // Initialize SPI
  SPI.begin(EPD_SCK, -1, EPD_MOSI, EPD_CS);
  Serial.println("SPI initialized");
  
  // Initialize I2C
  Wire.begin();
  Serial.println("I2C initialized");
  
  // Initialize display
  Serial.println("Initializing display...");
  display = new Display(EPD_BUSY, EPD_CS, EPD_RST, EPD_DC, CO2_ALARM_THRESHOLD, DATA_HISTORY_SIZE);
  if (!display || !display->begin()) {
    Serial.println("ERROR: Display initialization failed!");
    while (1) {
      // Flash LED or beep to indicate display failure
      digitalWrite(BUZZER_PIN, HIGH);
      delay(200);
      digitalWrite(BUZZER_PIN, LOW);
      delay(200);
    }
  }
  Serial.println("Display initialized successfully");
  
  // Show loading screen - this causes a single display update
  display->showLoadingScreen();
  
  // Initialize CO2 sensor
  Serial.println("Initializing CO2 sensor...");
  co2Sensor = new CO2Sensor(CO2_ALARM_THRESHOLD);
  bool sensorInitialized = co2Sensor->begin();
  Serial.println("Sensor initialization complete. Connected: " + String(sensorInitialized ? "YES" : "NO"));
  
  // Get initial sensor data
  currentData = co2Sensor->getData();  // Get initial data from sensor
  
  // Initialize history arrays with initial values
  for (int i = 0; i < 12; i++) {
    temperatureHistory[i] = currentData.temperature;
    humidityHistory[i] = currentData.humidity;
  }
  
  // Set initial history count to ensure charts display correctly
  miniHistoryCount = 1; // At least one value in the history
  
  // Initialize CO2 history array with zeros
  Serial.println("Initializing history array...");
  for (int i = 0; i < DATA_HISTORY_SIZE; i++) {
    co2History[i] = 0;  // Start with no data
  }
  
  // Do a single final update with all data
  Serial.println("Performing first display update...");
  updateDisplay(true);
  Serial.println("Display updated");
  
  Serial.println("Setup complete");
}

void loop() {
  unsigned long currentTime = millis();
  
  // Update sensor data every 30 seconds
  if (currentTime - lastDataUpdateTime >= 30000) {
    Serial.println("\n=== Updating sensor data ===");
    Serial.println("Time since last update: " + String((currentTime - lastDataUpdateTime) / 1000) + " seconds");
    
    bool dataUpdated = false;
    
    // If sensor is not connected, try to reconnect
    if (!co2Sensor->isConnected()) {
      Serial.println("Sensor not connected, attempting to reconnect...");
      tryReconnectSensor();
    }
    
    // Update sensor data
    if (co2Sensor->isConnected()) {
      dataUpdated = co2Sensor->update();
      if (dataUpdated) {
        currentData = co2Sensor->getData();
      }
    }
    
    lastDataUpdateTime = currentTime;
    
    // Only check alarm and update display if sensor is connected
    if (co2Sensor->isConnected()) {
      Serial.println("Sensor is connected, checking CO2 levels...");
      // Check CO2 levels for alarm condition
      checkAlarm();
      
      // Check if values changed significantly
      if (dataUpdated && significantChange()) {
        Serial.println("Significant change detected, updating display");
        updateDisplay(true);  // Full update
        lastDisplayedData = currentData;
        lastFullUpdateTime = currentTime;
      } else if (dataUpdated) {
        Serial.println("No significant change detected");
      }
    } else {
      // If sensor is not connected, update display with connection instructions
      Serial.println("Sensor is not connected, showing connection instructions");
      updateDisplay(true);
      lastFullUpdateTime = currentTime;
    }
  }
  
  // Update history every 5 minutes (only if sensor is connected) - changed from 30 minutes for faster testing
  if (co2Sensor->isConnected() && co2Sensor->getValidReadingCount() >= 3 && 
      currentTime - lastHistoryUpdateTime >= 300000) {
    // Update history with current data
    updateHistory();
    lastHistoryUpdateTime = currentTime;
    
    // Perform a full update to keep chart and values in sync
    updateDisplay(true);
    lastDisplayedData = currentData;
    lastFullUpdateTime = currentTime;
    
    Serial.println("Chart updated - full display refresh to keep values in sync");
  }
  
  // Force full refresh every 6 hours to prevent ghosting
  if (currentTime - lastFullUpdateTime >= 21600000) {
    updateDisplay(true);
    lastFullUpdateTime = currentTime;
  }
  
  // Turn off buzzer after 5 seconds if it's active
  if (buzzerActive && (currentTime - lastBuzzerTime >= 5000)) {
    activateBuzzer(false);
  }
  
  delay(1000);  // Small delay to prevent excessive CPU usage
}

void updateDisplay(bool fullUpdate) {
  if (!display) {
    Serial.println("ERROR: Display not initialized");
    return;
  }
  
  if (fullUpdate) {
    // Pass current data and history arrays to display
    display->updateFull(currentData, co2History, historyIndex, co2Sensor->isConnected());
    
    // Update last displayed data
    lastDisplayedData = currentData;
    Serial.println("Full display update completed");
  } else {
    display->updateChart(co2History, historyIndex);
    Serial.println("Chart-only update completed");
  }
}

void updateHistory() {
  // Only add to history if we have collected at least 3 valid readings
  // This ensures the sensor has stabilized before recording data
  if (co2Sensor->getValidReadingCount() >= 3) {
    // Add current CO2 value to main history
    co2History[historyIndex] = currentData.co2;
    historyIndex = (historyIndex + 1) % DATA_HISTORY_SIZE;
    
    // Update mini history for temperature and humidity
    temperatureHistory[miniHistoryIndex] = currentData.temperature;
    humidityHistory[miniHistoryIndex] = currentData.humidity;
    miniHistoryIndex = (miniHistoryIndex + 1) % 12;
    
    // Increment mini history count if not at max
    if (miniHistoryCount < 12) {
      miniHistoryCount++;
    }
    
    Serial.println("Updated CO2 history");
    Serial.print("Current index: ");
    Serial.println(historyIndex);
  } else {
    Serial.println("Not enough valid readings yet, skipping history update");
    Serial.print("Current valid reading count: ");
    Serial.println(co2Sensor->getValidReadingCount());
  }
}

bool significantChange() {
  // Check if current values differ significantly from last displayed values
  if (abs((int)currentData.co2 - (int)lastDisplayedData.co2) >= CO2_THRESHOLD) {
    return true;
  }
  
  if (abs(currentData.temperature - lastDisplayedData.temperature) >= TEMP_THRESHOLD) {
    return true;
  }
  
  if (abs(currentData.humidity - lastDisplayedData.humidity) >= HUM_THRESHOLD) {
    return true;
  }
  
  return false;
}

void checkAlarm() {
  unsigned long currentTime = millis();
  
  // Check if CO2 is above threshold and if enough time has passed since last alarm
  if (currentData.co2 >= CO2_ALARM_THRESHOLD && 
      (currentTime - lastBuzzerTime >= BUZZER_INTERVAL)) {
    // Activate buzzer
    activateBuzzer(true);
    lastBuzzerTime = currentTime;
  }
}

void activateBuzzer(bool activate) {
  if (activate) {
    digitalWrite(BUZZER_PIN, HIGH);
    buzzerActive = true;
    Serial.println("ALARM: High CO2 levels detected!");
  } else {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerActive = false;
  }
}

bool tryReconnectSensor() {
  Serial.println("Attempting to reconnect CO2 sensor...");
  
  // Try to reset and reinitialize the sensor
  if (co2Sensor->reset()) {
    Serial.println("Successfully reconnected CO2 sensor");
    return true;
  }
  
  Serial.println("Failed to reconnect CO2 sensor");
  return false;
}
