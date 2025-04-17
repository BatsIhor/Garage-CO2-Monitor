#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Fonts/FreeMonoBold24pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>

// Define display colors enum
enum DisplayColor {
    COLOR_BLACK = 0,  // Black text/graphics
    COLOR_WHITE = 1   // White background
};

// Air quality status based on CO2 levels
enum AirQualityStatus {
    AIR_EXCELLENT,    // Below 600 ppm
    AIR_GOOD,         // 600-800 ppm
    AIR_FAIR,         // 800-1000 ppm
    AIR_POOR,         // 1000-1500 ppm
    AIR_UNHEALTHY     // Above 1500 ppm
};

// Structure to hold sensor data
struct SensorData {
  uint16_t co2;
  float temperature;
  float humidity;
};

// Structure to hold historical data for mini charts
struct HistoricalData {
  uint16_t co2[12];    // Last 12 CO2 readings
  float temp[12];      // Last 12 temperature readings
  float humidity[12];  // Last 12 humidity readings
  int count;           // Number of valid readings (up to 12)
  int index;           // Current index in circular buffer
};

class Display : public Adafruit_GFX {
public:
  // Constructor with all pin definitions
  Display(uint8_t busy_pin, uint8_t cs_pin, uint8_t rst_pin, uint8_t dc_pin, 
          int co2_alarm_threshold, uint8_t data_history_size);
  
  // Destructor
  ~Display();
          
  // Initialize the display
  bool begin();
  
  // Update the full display with sensor data
  void updateFull(const SensorData& data, const uint16_t* co2History, 
                 int historyIndex, bool sensorConnected);
  
  // Update just the chart area
  void updateChart(const uint16_t* co2History, int historyIndex);
  
  // Display connection instructions when sensor is not connected
  void showConnectionInstructions();
  
  // Display loading screen during device startup
  void showLoadingScreen();
  
  // GFX drawing functions
  void drawPixel(int16_t x, int16_t y, uint16_t color);
  void fillScreen(uint16_t color);
  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color);
  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color);
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
  void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
  
  // Display control
  void update();
  void sleep();
  
  // Constants for the display
  static const uint16_t WIDTH = 648;
  static const uint16_t HEIGHT = 480;

private:
  // Display pins
  uint8_t _busy_pin;
  uint8_t _cs_pin;
  uint8_t _rst_pin;
  uint8_t _dc_pin;
  
  // Other parameters
  int _co2_alarm_threshold;
  uint8_t _data_history_size;

  // Display buffer
  uint8_t* _buffer;
  
  // Low-level communication functions
  void sendCommand(uint8_t command);
  void sendData(uint8_t data);
  void waitUntilIdle();
  
  // EPD initialization
  void reset();
  void initDisplay();
  
  // Helper methods
  void drawBarChart(const uint16_t* co2History, int historyIndex);
  void drawMiniChart(int x, int y, int width, int height, const float* data, int count, int index, float min, float max, uint16_t color);
  void drawCO2MiniChart(int x, int y, int width, int height, const uint16_t* data, int count, int index, uint16_t color);
  void drawCO2Value(uint16_t co2Value, int x, int y);
  void drawTemperatureValue(float temperature, int x, int y);
  void drawHumidityValue(float humidity, int x, int y);
  const char* getAirQualityMessage(uint16_t co2Value);
  
  // Draw large digit at x,y position with given width and height
  void drawLargeDigit(uint8_t digit, int16_t x, int16_t y, int16_t width, int16_t height, uint16_t color);
  
  // Draw large number with specified parameters
  void drawLargeNumber(uint16_t number, int16_t x, int16_t y, int16_t digitWidth, 
                      int16_t digitHeight, int16_t spacing, uint16_t color);
  
  void setCursor(int16_t x, int16_t y);
  void setTextColor(uint16_t color);
  void setFont(const GFXfont* font);
  void print(const char* text);
  void print(int value);
  void print(float value, int precision);
};

#endif // DISPLAY_H 