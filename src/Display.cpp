#include "Display.h"
#include <math.h>

// Font for text rendering
const GFXfont* currentFont = nullptr;
int16_t cursor_x = 0;
int16_t cursor_y = 0;
uint16_t textColor = COLOR_BLACK;

// Helper functions for min and max since the Arduino ones are causing linter errors
template <typename T>
T getMin(T a, T b) {
    return (a < b) ? a : b;
}

template <typename T>
T getMax(T a, T b) {
    return (a > b) ? a : b;
}

Display::Display(uint8_t busy_pin, uint8_t cs_pin, uint8_t rst_pin, uint8_t dc_pin,
                int co2_alarm_threshold, uint8_t data_history_size) 
  : Adafruit_GFX(WIDTH, HEIGHT),
    _busy_pin(busy_pin), _cs_pin(cs_pin), _rst_pin(rst_pin), _dc_pin(dc_pin),
    _co2_alarm_threshold(co2_alarm_threshold), _data_history_size(data_history_size) {
    
    // Allocate buffer for display
    _buffer = new uint8_t[WIDTH * HEIGHT / 8];
    if (_buffer) {
        memset(_buffer, 0x00, WIDTH * HEIGHT / 8); // Clear buffer (black)
    }
}

Display::~Display() {
    if (_buffer) {
        delete[] _buffer;
    }
}

bool Display::begin() {
    Serial.println("Display: Initializing...");
    
    // Setup pins
    pinMode(_busy_pin, INPUT);
    pinMode(_rst_pin, OUTPUT);
    pinMode(_dc_pin, OUTPUT);
    pinMode(_cs_pin, OUTPUT);
    
    digitalWrite(_cs_pin, HIGH);
    digitalWrite(_dc_pin, HIGH);
    digitalWrite(_rst_pin, HIGH);
    
    // Initialize display
    initDisplay();
    
    // Don't clear or update the display yet - wait for actual content
    
    Serial.println("Display: Initialization complete");
    return true;
}

void Display::reset() {
    Serial.println("Display: Resetting...");
    digitalWrite(_rst_pin, LOW);
    delay(10);
    digitalWrite(_rst_pin, HIGH);
    delay(10);
}

void Display::initDisplay() {
    Serial.println("Display: Sending init commands...");
    
    // Reset the display first
    reset();
    delay(100);  // Give it more time after reset
    
    // Based on the working example's initialization sequence for GDEY0583T81
    sendCommand(0x06);   // Booster Soft Start
    sendData(0x17);      // A
    sendData(0x17);      // B
    sendData(0x28);      // C
    sendData(0x17);      // D
    
    delay(10);  // Add small delay between commands
    
    sendCommand(0x04);   // POWER ON
    delay(100);          // Give more time for power-on
    waitUntilIdle();
    
    sendCommand(0x00);   // PANEL SETTING
    sendData(0x0F);      // KW-3f KWR-2F BWROTP 0f BWOTP 1f
    
    delay(10);  // Add small delay between commands
    
    sendCommand(0x50);   // VCOM AND DATA INTERVAL SETTING
    sendData(0x20);      // Default value for this display
    sendData(0x07);
    
    delay(10);  // Add small delay between commands
    
    sendCommand(0x61);   // Resolution Setting
    sendData(WIDTH >> 8); // High byte of width
    sendData(WIDTH & 0xFF); // Low byte of width
    sendData(HEIGHT >> 8); // High byte of height
    sendData(HEIGHT & 0xFF); // Low byte of height
    
    delay(10);  // Add small delay between commands
    
    Serial.println("Display: Init commands sent");
    
    // Instead of clearing the display during initialization,
    // just prepare the buffer for later use but don't send to display
    fillScreen(COLOR_BLACK);
    
    // Set flag to indicate we have not actually updated the display yet
    // This will be done by the first content rendering
    Serial.println("Display: Display initialized (buffer prepared, no refresh yet)");
}

void Display::sendCommand(uint8_t command) {
    digitalWrite(_dc_pin, LOW);   // Command mode
    digitalWrite(_cs_pin, LOW);
    SPI.transfer(command);
    digitalWrite(_cs_pin, HIGH);
}

void Display::sendData(uint8_t data) {
    digitalWrite(_dc_pin, HIGH);  // Data mode
    digitalWrite(_cs_pin, LOW);
    SPI.transfer(data);
    digitalWrite(_cs_pin, HIGH);
}

void Display::waitUntilIdle() {
    Serial.println("Display: Waiting for display to be idle...");
    
    // Add a timeout to prevent getting stuck in a loop
    unsigned long startTime = millis();
    const unsigned long timeout = 5000; // 5 second timeout
    
    while(digitalRead(_busy_pin) == HIGH) {
        delay(10);
        
        // Check for timeout
        if (millis() - startTime > timeout) {
            Serial.println("Display: BUSY timeout - forcing continue");
            break;
        }
    }
    
    Serial.println("Display: Display is idle now");
}

void Display::update() {
    Serial.println("Display: Updating display...");
    
    // Send black buffer data
    sendCommand(0x10);
    delay(10);  // Add a small delay after command
    
    // Send black buffer data
    for (uint32_t i = 0; i < WIDTH * HEIGHT / 8; i++) {
        sendData(_buffer[i]);
        
        // Add a tiny delay every 1024 bytes to prevent overrunning the display controller
        if ((i % 1024) == 0) {
            delayMicroseconds(100);
        }
    }
    
    delay(10);  // Add a small delay after data transfer
    
    // Send red buffer data (we're using B/W display so just send 0s)
    sendCommand(0x13);
    delay(10);  // Add a small delay after command
    
    // Send red buffer data (all zeros)
    for (uint32_t i = 0; i < WIDTH * HEIGHT / 8; i++) {
        sendData(0x00);
        
        // Add a tiny delay every 1024 bytes to prevent overrunning the display controller
        if ((i % 1024) == 0) {
            delayMicroseconds(100);
        }
    }
    
    delay(10);  // Add a small delay after data transfer
    
    // Refresh display
    sendCommand(0x12);
    delay(100);  // Increased delay to give more time before asking for busy status
    waitUntilIdle();
    
    // Add an additional delay after the update to ensure the display has time to stabilize
    delay(100);
    
    Serial.println("Display: Update complete");
}

void Display::sleep() {
    Serial.println("Display: Going to sleep...");
    sendCommand(0x02);  // Power off
    waitUntilIdle();
    delay(100);
    sendCommand(0x07);  // Deep sleep
    sendData(0xA5);
    Serial.println("Display: Now sleeping");
}

void Display::fillScreen(uint16_t color) {
    Serial.println("Display: Filling screen...");
    uint8_t fillValue = (color == COLOR_WHITE) ? 0xFF : 0x00;
    memset(_buffer, fillValue, WIDTH * HEIGHT / 8);
}

void Display::drawPixel(int16_t x, int16_t y, uint16_t color) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return;
    
    uint32_t byte_idx = (y * WIDTH + x) / 8;
    uint8_t bit_pos = 7 - ((y * WIDTH + x) % 8);
    
    if (color == COLOR_WHITE) {
        _buffer[byte_idx] |= (1 << bit_pos);  // Set bit (white)
    } else {
        _buffer[byte_idx] &= ~(1 << bit_pos);  // Clear bit (black)
    }
}

void Display::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
    for (int16_t i = y; i < y + h; i++) {
        drawPixel(x, i, color);
    }
}

void Display::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
    for (int16_t i = x; i < x + w; i++) {
        drawPixel(i, y, color);
    }
}

void Display::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    for (int16_t i = x; i < x + w; i++) {
        for (int16_t j = y; j < y + h; j++) {
            drawPixel(i, j, color);
        }
    }
}

void Display::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    // Draw horizontal lines
    drawFastHLine(x, y, w, color);
    drawFastHLine(x, y + h - 1, w, color);
    
    // Draw vertical lines
    drawFastVLine(x, y, h, color);
    drawFastVLine(x + w - 1, y, h, color);
}

void Display::setCursor(int16_t x, int16_t y) {
    cursor_x = x;
    cursor_y = y;
}

void Display::setTextColor(uint16_t color) {
    textColor = color;
}

void Display::setFont(const GFXfont* font) {
    currentFont = font;
    Adafruit_GFX::setFont(font);
}

void Display::print(const char* text) {
    setCursor(cursor_x, cursor_y);
    Adafruit_GFX::setTextColor(textColor);
    Adafruit_GFX::print(text);
}

void Display::print(int value) {
    setCursor(cursor_x, cursor_y);
    Adafruit_GFX::setTextColor(textColor);
    Adafruit_GFX::print(value);
}

void Display::print(float value, int precision) {
    setCursor(cursor_x, cursor_y);
    Adafruit_GFX::setTextColor(textColor);
    Adafruit_GFX::print(value, precision);
}

void Display::updateFull(const SensorData& data, const uint16_t* co2History, 
                       int historyIndex, bool sensorConnected) {
    Serial.println("Display: Performing full update");
    
    // Clear display
    fillScreen(COLOR_BLACK);
    
    if (!sensorConnected) {
        // Display connection instructions if sensor is not connected
        showConnectionInstructions();
    } else {
        const int leftPanelX = 20;
        const int centerPanelX = WIDTH / 2;
        const int rightPanelX = WIDTH - 180;
        const int topY = 80;
        const int middleY = 240;
        const int miniChartHeight = 100;
        const int miniChartWidth = 160;
        
        // Draw panel borders
        drawRect(leftPanelX - 10, topY - 70, 180, 290, COLOR_WHITE);  // Temperature panel
        drawRect(centerPanelX - 120, topY - 70, 240, 290, COLOR_WHITE);  // CO2 panel
        drawRect(rightPanelX - 10, topY - 70, 180, 290, COLOR_WHITE);  // Humidity panel
        
        // Center panel - CO2 value
        setFont(&FreeMonoBold24pt7b);
        setTextColor(COLOR_WHITE);
        
        // Draw air quality message
        setCursor(centerPanelX - 100, topY - 30);
        print(getAirQualityMessage(data.co2));
        
        // Draw CO2 value
        drawCO2Value(data.co2, centerPanelX, topY);
        
        // Draw mini CO2 chart below
        drawCO2MiniChart(centerPanelX - 100, topY + 180, 200, miniChartHeight, co2History, 12, historyIndex, COLOR_WHITE);
        
        // Left panel - Temperature
        drawTemperatureValue(data.temperature, leftPanelX + 80, topY);
        
        // Get temperature history from the main program
        // We access it via external reference
        extern float temperatureHistory[12];
        extern int miniHistoryIndex;
        extern int miniHistoryCount;
        
        // Calculate min/max temperature for scaling
        float minTemp = data.temperature;
        float maxTemp = data.temperature;
        for (int i = 0; i < miniHistoryCount; i++) {
            minTemp = getMin(minTemp, temperatureHistory[i]);
            maxTemp = getMax(maxTemp, temperatureHistory[i]);
        }
        
        // Add some margins
        minTemp = getMax(0.0f, minTemp - 1.0f);
        maxTemp = maxTemp + 1.0f;
        
        // Ensure min and max are different to prevent division by zero
        if (fabs(maxTemp - minTemp) < 0.1f) {
            maxTemp = minTemp + 2.0f; // Add a 2 degree range if values are identical
        }
        
        // Draw temperature mini chart
        drawMiniChart(leftPanelX, topY + 80, miniChartWidth, miniChartHeight, 
                     temperatureHistory, miniHistoryCount, miniHistoryIndex, 
                     minTemp, maxTemp, COLOR_WHITE);
        
        // Right panel - Humidity
        drawHumidityValue(data.humidity, rightPanelX + 80, topY);
        
        // Get humidity history from the main program
        extern float humidityHistory[12];
        
        // Calculate min/max humidity for scaling
        float minHum = data.humidity;
        float maxHum = data.humidity;
        for (int i = 0; i < miniHistoryCount; i++) {
            minHum = getMin(minHum, humidityHistory[i]);
            maxHum = getMax(maxHum, humidityHistory[i]);
        }
        
        // Add some margins
        minHum = getMax(0.0f, minHum - 2.0f);
        maxHum = getMin(100.0f, maxHum + 2.0f);
        
        // Ensure min and max are different to prevent division by zero
        if (fabs(maxHum - minHum) < 0.1f) {
            maxHum = minHum + 5.0f; // Add a 5% range if values are identical
        }
        
        // Draw humidity mini chart
        drawMiniChart(rightPanelX, topY + 80, miniChartWidth, miniChartHeight, 
                     humidityHistory, miniHistoryCount, miniHistoryIndex, 
                     minHum, maxHum, COLOR_WHITE);
        
        // Draw main CO2 history chart at the bottom
        drawBarChart(co2History, historyIndex);
        
        // Show update time
        setFont(&FreeMonoBold12pt7b);
        setTextColor(COLOR_WHITE);
        setCursor(20, HEIGHT - 20);
        unsigned long uptime = millis() / 1000 / 60;  // Minutes
        print("Last update: ");
        print((int)uptime);
        print(" min ago");
    }
    
    // Send to display
    update();
}

void Display::updateChart(const uint16_t* co2History, int historyIndex) {
    Serial.println("Display: Updating chart area");
    
    // Clear only the chart area
    fillRect(0, 200, WIDTH, 100, COLOR_BLACK);
    
    // Draw the chart
    drawBarChart(co2History, historyIndex);
    
    // Update display
    update();
}

void Display::showConnectionInstructions() {
    setTextColor(COLOR_WHITE);
    setFont(&FreeMonoBold18pt7b);
    setCursor(20, 60);
    print("CO2 Sensor Not Connected");
    
    setFont(&FreeMonoBold12pt7b);
    setCursor(20, 100);
    print("Please check:");
    
    setCursor(30, 130);
    print("1. Power connection to sensor");
    
    setCursor(30, 160);
    print("2. I2C wiring (SDA/SCL)");
    
    setCursor(30, 190);
    print("3. Sensor address (0x62)");
    
    setCursor(20, 230);
    print("The system will automatically");
    setCursor(20, 260);
    print("reconnect when sensor is available");
}

void Display::drawBarChart(const uint16_t* co2History, int historyIndex) {
    int chartX = 70;
    int chartY = 320;
    int chartWidth = WIDTH - 100;
    int chartHeight = 160;
    
    // Draw chart border
    for (int i = 0; i < chartWidth; i++) {
        drawPixel(chartX + i, chartY, COLOR_WHITE);
        drawPixel(chartX + i, chartY + chartHeight, COLOR_WHITE);
    }
    for (int i = 0; i < chartHeight; i++) {
        drawPixel(chartX, chartY + i, COLOR_WHITE);
        drawPixel(chartX + chartWidth, chartY + i, COLOR_WHITE);
    }
    
    // Find min and max values for scaling
    uint16_t minCO2 = 10000;
    uint16_t maxCO2 = 0;
    bool hasValidData = false;
    
    for (int i = 0; i < _data_history_size; i++) {
        if (co2History[i] > 0) {  // Only consider valid readings
            minCO2 = getMin(minCO2, co2History[i]);
            maxCO2 = getMax(maxCO2, co2History[i]);
            hasValidData = true;
        }
    }
    
    // Handle case where no valid data exists
    if (!hasValidData) {
        minCO2 = 400;  // Base CO2 level
        maxCO2 = 1000; // Typical threshold
    }
    
    // Ensure minimum range for better visualization
    if (maxCO2 - minCO2 < 500) {
        uint16_t avgCO2 = (maxCO2 + minCO2) / 2;
        minCO2 = getMax(0, (int)avgCO2 - 250);
        maxCO2 = avgCO2 + 250;
    }
    
    minCO2 = getMax(0, (int)minCO2);
    maxCO2 = getMax(1000, (int)maxCO2);
    
    // Ensure min and max are different to prevent division by zero
    if (maxCO2 <= minCO2) {
        maxCO2 = minCO2 + 500; // Add a 500 ppm range if values are identical
    }
    
    // Round to nice values
    minCO2 = (minCO2 / 100) * 100;
    maxCO2 = ((maxCO2 + 99) / 100) * 100;
    
    // Draw bars
    int barWidth = (chartWidth - 10) / _data_history_size;
    barWidth = getMax(barWidth, 1);
    
    // Draw the title
    setFont(&FreeMonoBold12pt7b);
    setTextColor(COLOR_WHITE);
    setCursor(chartX, chartY - 5);
    print("CO2 History (24h)");
    
    // Draw left-side scale labels (y-axis)
    int labelWidth = 60;
    drawFastVLine(chartX - 5, chartY, chartHeight, COLOR_WHITE);
    
    // Top label (max value)
    char buffer[10];
    sprintf(buffer, "%4d", (int)maxCO2);
    setFont(&FreeMonoBold12pt7b);
    setTextColor(COLOR_WHITE);
    setCursor(chartX - labelWidth, chartY + 15);
    print(buffer);
    
    // Bottom label (min value)
    sprintf(buffer, "%4d", (int)minCO2);
    setCursor(chartX - labelWidth, chartY + chartHeight - 5);
    print(buffer);
    
    // Mid-point label
    if (maxCO2 != minCO2) {
        uint16_t midCO2 = (maxCO2 + minCO2) / 2;
        sprintf(buffer, "%4d", (int)midCO2);
        setCursor(chartX - labelWidth, chartY + chartHeight/2 + 5);
        print(buffer);
    }
    
    // Mark the alarm threshold on the Y axis
    if (_co2_alarm_threshold >= minCO2 && _co2_alarm_threshold <= maxCO2) {
        int thresholdY = map(_co2_alarm_threshold, minCO2, maxCO2, 
                            chartY + chartHeight - 5, chartY + 5);
        
        // Draw dashed line for threshold
        for (int x = chartX + 2; x < chartX + chartWidth - 4; x += 6) {
            drawFastHLine(x, thresholdY, 3, COLOR_WHITE);
        }
    }
    
    // Draw bars from newest to oldest
    for (int i = 0; i < _data_history_size; i++) {
        int idx = (historyIndex - i + _data_history_size) % _data_history_size;
        
        if (co2History[idx] > 0) {
            int barHeight = map(co2History[idx], minCO2, maxCO2, 5, chartHeight - 10);
            barHeight = constrain(barHeight, 5, chartHeight - 10);
            
            if (co2History[idx] >= _co2_alarm_threshold) {
                // Draw as a hollow bar for high CO2 levels
                for (int j = 0; j < barHeight; j++) {
                    drawPixel(chartX + 5 + i * barWidth, chartY + chartHeight - 5 - j, COLOR_WHITE);
                    drawPixel(chartX + 5 + i * barWidth + barWidth - 1, chartY + chartHeight - 5 - j, COLOR_WHITE);
                }
                drawFastHLine(chartX + 5 + i * barWidth, chartY + chartHeight - 5 - barHeight, barWidth, COLOR_WHITE);
                drawFastHLine(chartX + 5 + i * barWidth, chartY + chartHeight - 5, barWidth, COLOR_WHITE);
            } else {
                // Draw as a filled bar for normal CO2 levels
                fillRect(
                    chartX + 5 + i * barWidth,
                    chartY + chartHeight - 5 - barHeight,
                    barWidth,
                    barHeight,
                    COLOR_WHITE
                );
            }
        } else {
            // Draw a small empty placeholder bar
            drawRect(
                chartX + 5 + i * barWidth,
                chartY + chartHeight - 5 - 5,
                barWidth,
                5,
                COLOR_WHITE
            );
        }
    }
    
    // Draw reference lines
    drawFastHLine(chartX, chartY + chartHeight/2, chartWidth, COLOR_WHITE);
}

// New helper methods for drawing the mini charts and values
void Display::drawCO2Value(uint16_t co2Value, int x, int y) {
    char buffer[20];
    setFont(&FreeMonoBold24pt7b);
    setTextColor(COLOR_WHITE);
    
    // Calculate text width to center it
    sprintf(buffer, "%d", co2Value);
    int16_t x1, y1;
    uint16_t w, h;
    getTextBounds(buffer, 0, 0, &x1, &y1, &w, &h);
    
    setCursor(x - w/2, y);
    print(co2Value);
    
    // Draw "ppm" in smaller font below
    setFont(&FreeMonoBold12pt7b);
    setCursor(x - 20, y + 30);
    print("ppm");
}

void Display::drawTemperatureValue(float temperature, int x, int y) {
    char buffer[20];
    setFont(&FreeMonoBold18pt7b);
    setTextColor(COLOR_WHITE);
    
    // Draw temperature value
    sprintf(buffer, "%.1f C", temperature);
    int16_t x1, y1;
    uint16_t w, h;
    getTextBounds(buffer, 0, 0, &x1, &y1, &w, &h);
    
    setCursor(x - w/2, y);
    print(temperature, 1);
    
    // Draw °C
    setFont(&FreeMonoBold12pt7b);
    setCursor(x + 20, y);
    print("C");
    
    // Draw label above
    setCursor(x - 60, y - 40);
    print("Temperature");
}

void Display::drawHumidityValue(float humidity, int x, int y) {
    char buffer[20];
    setFont(&FreeMonoBold18pt7b);
    setTextColor(COLOR_WHITE);
    
    // Draw humidity value
    sprintf(buffer, "%.1f%%", humidity);
    int16_t x1, y1;
    uint16_t w, h;
    getTextBounds(buffer, 0, 0, &x1, &y1, &w, &h);
    
    setCursor(x - w/2, y);
    print(humidity, 1);
    
    // Draw % symbol
    setCursor(x + 20, y);
    print("%");
    
    // Draw label above
    setFont(&FreeMonoBold12pt7b);
    setCursor(x - 40, y - 40);
    print("Humidity");
}

void Display::drawCO2MiniChart(int x, int y, int width, int height, const uint16_t* data, int count, int index, uint16_t color) {
    // Find min and max values for scaling
    uint16_t minVal = 10000;
    uint16_t maxVal = 0;
    
    // Calculate how many valid data points we want to include
    int validCount = getMin(count, 12);  // Use at most 12 recent points
    
    for (int i = 0; i < validCount; i++) {
        int dataIndex = (index - i + _data_history_size) % _data_history_size;
        if (data[dataIndex] > 0) {
            minVal = getMin(minVal, data[dataIndex]);
            maxVal = getMax(maxVal, data[dataIndex]);
        }
    }
    
    // Set reasonable defaults if no valid data
    if (minVal == 10000) minVal = 400;
    if (maxVal == 0) maxVal = 1000;
    
    // Add margins to range
    minVal = getMax(0, (int)minVal - 100);
    maxVal = maxVal + 100;
    
    // Ensure min and max are different to prevent division by zero
    if (maxVal <= minVal) {
        maxVal = minVal + 200; // Add a 200 ppm range if values are identical
    }
    
    // Draw chart border
    drawRect(x, y, width, height, color);
    
    // Draw bars
    int barWidth = (width - 4) / validCount;
    barWidth = getMax(barWidth, 4);  // Ensure minimum width

    // Draw the title
    setFont(&FreeMonoBold12pt7b);
    setTextColor(COLOR_WHITE);
    setCursor(x, y - 5);
    print("CO2 Trend");
    
    // Draw recent data points from right to left
    for (int i = 0; i < validCount; i++) {
        int dataIndex = (index - i + _data_history_size) % _data_history_size;
        uint16_t value = data[dataIndex];
        
        if (value > 0) {  // Only draw valid data
            int barHeight = map(value, minVal, maxVal, 2, height - 4);
            int barX = x + width - 2 - (i + 1) * barWidth;
            int barY = y + height - 2 - barHeight;
            
            // Draw filled or hollow bar based on threshold
            if (value >= _co2_alarm_threshold) {
                // Draw hollow bar
                drawRect(barX, barY, barWidth - 1, barHeight, color);
            } else {
                // Draw filled bar
                fillRect(barX, barY, barWidth - 1, barHeight, color);
            }
        }
    }
}

void Display::drawMiniChart(int x, int y, int width, int height, const float* data, int count, int index, float min, float max, uint16_t color) {
    // Draw chart border
    drawRect(x, y, width, height, color);
    
    // Calculate bar width
    int barWidth = (width - 4) / count;
    barWidth = getMax(barWidth, 4);  // Ensure minimum width
    
    // Check if min and max are equal to prevent division by zero
    if (fabs(max - min) < 0.0001f) {  // Using small epsilon for float comparison
        // If min and max are the same, just draw a flat line in the middle
        int midY = y + height / 2;
        drawFastHLine(x + 2, midY, width - 4, color);
        return;
    }
    
    // Draw bars
    for (int i = 0; i < count; i++) {
        // For simplicity, we're using the full array directly
        float value = data[i];
        
        // Calculate bar height using direct math instead of map
        float normalizedValue = (value - min) / (max - min);  // 0.0 to 1.0
        int barHeight = 2 + (int)((height - 6) * normalizedValue);
        barHeight = constrain(barHeight, 2, height - 4);
        
        int barX = x + width - 2 - (i + 1) * barWidth;
        int barY = y + height - 2 - barHeight;
        
        // Draw bar
        fillRect(barX, barY, barWidth - 1, barHeight, color);
    }
}

const char* Display::getAirQualityMessage(uint16_t co2Value) {
    if (co2Value < 600) {
        return "EXCELLENT";
    } else if (co2Value < 800) {
        return "GOOD";
    } else if (co2Value < 1000) {
        return "FAIR";
    } else if (co2Value < 1500) {
        return "POOR";
    } else {
        return "UNHEALTHY";
    }
}

void Display::showLoadingScreen() {
    Serial.println("Display: Showing loading screen");
    
    // Clear display
    fillScreen(COLOR_BLACK);
    
    // Draw title
    setFont(&FreeMonoBold24pt7b);
    setTextColor(COLOR_WHITE);
    setCursor(120, 100);
    print("CO2 Monitor");
    
    // Draw subtitle
    setFont(&FreeMonoBold18pt7b);
    setCursor(90, 150);
    print("Initializing System...");
    
    // Draw loading bar - already complete
    int barWidth = 500;
    int barHeight = 40;
    int barX = (WIDTH - barWidth) / 2;
    int barY = 200;
    
    // Draw border and fill completely
    drawRect(barX, barY, barWidth, barHeight, COLOR_WHITE);
    fillRect(barX + 3, barY + 3, barWidth - 6, barHeight - 6, COLOR_WHITE);
    
    // Show 100% text
    setFont(&FreeMonoBold18pt7b);
    setCursor(barX + barWidth / 2 - 40, barY + barHeight + 35);
    print("100%");
    
    // Show "starting" message
    setCursor(170, 320);
    print("Starting...");
    
    // Only update the display once with all elements already drawn
    update();
    
    // Short delay before continuing
    delay(1000);
    
    Serial.println("Display: Loading screen complete");
}

// Draw a large digit using rectangles
void Display::drawLargeDigit(uint8_t digit, int16_t x, int16_t y, int16_t width, int16_t height, uint16_t color) {
    // Segment thickness as a proportion of height (increase from 5 to 4 for thicker segments)
    int16_t segThickness = height / 4;  
    
    // Horizontal segment width (leave some space on the sides)
    int16_t hSegWidth = width - segThickness;
    
    // Vertical segment height
    int16_t vSegHeight = (height / 2) - (segThickness / 2);
    
    // Positions for segments
    int16_t topX = x + segThickness/2;
    int16_t midX = topX;
    int16_t botX = topX;
    
    int16_t topY = y;
    int16_t midY = y + (height / 2) - (segThickness / 2);
    int16_t botY = y + height - segThickness;
    
    int16_t leftTopX = x;
    int16_t leftBotX = x;
    int16_t rightTopX = x + width - segThickness;
    int16_t rightBotX = rightTopX;
    
    int16_t leftTopY = y + segThickness/2;
    int16_t leftBotY = midY + segThickness;
    int16_t rightTopY = leftTopY;
    int16_t rightBotY = leftBotY;
    
    // Draw segments based on digit
    switch (digit) {
        case 0:
            fillRect(topX, topY, hSegWidth, segThickness, color);           // Top
            fillRect(leftTopX, leftTopY, segThickness, vSegHeight, color);  // Left Top
            fillRect(rightTopX, rightTopY, segThickness, vSegHeight, color);// Right Top
            fillRect(leftBotX, leftBotY, segThickness, vSegHeight, color);  // Left Bottom
            fillRect(rightBotX, rightBotY, segThickness, vSegHeight, color);// Right Bottom
            fillRect(botX, botY, hSegWidth, segThickness, color);           // Bottom
            break;
        case 1:
            fillRect(rightTopX, rightTopY, segThickness, vSegHeight, color);// Right Top
            fillRect(rightBotX, rightBotY, segThickness, vSegHeight, color);// Right Bottom
            break;
        case 2:
            fillRect(topX, topY, hSegWidth, segThickness, color);           // Top
            fillRect(rightTopX, rightTopY, segThickness, vSegHeight, color);// Right Top
            fillRect(midX, midY, hSegWidth, segThickness, color);           // Middle
            fillRect(leftBotX, leftBotY, segThickness, vSegHeight, color);  // Left Bottom
            fillRect(botX, botY, hSegWidth, segThickness, color);           // Bottom
            break;
        case 3:
            fillRect(topX, topY, hSegWidth, segThickness, color);           // Top
            fillRect(rightTopX, rightTopY, segThickness, vSegHeight, color);// Right Top
            fillRect(midX, midY, hSegWidth, segThickness, color);           // Middle
            fillRect(rightBotX, rightBotY, segThickness, vSegHeight, color);// Right Bottom
            fillRect(botX, botY, hSegWidth, segThickness, color);           // Bottom
            break;
        case 4:
            fillRect(leftTopX, leftTopY, segThickness, vSegHeight, color);  // Left Top
            fillRect(rightTopX, rightTopY, segThickness, vSegHeight, color);// Right Top
            fillRect(midX, midY, hSegWidth, segThickness, color);           // Middle
            fillRect(rightBotX, rightBotY, segThickness, vSegHeight, color);// Right Bottom
            break;
        case 5:
            fillRect(topX, topY, hSegWidth, segThickness, color);           // Top
            fillRect(leftTopX, leftTopY, segThickness, vSegHeight, color);  // Left Top
            fillRect(midX, midY, hSegWidth, segThickness, color);           // Middle
            fillRect(rightBotX, rightBotY, segThickness, vSegHeight, color);// Right Bottom
            fillRect(botX, botY, hSegWidth, segThickness, color);           // Bottom
            break;
        case 6:
            fillRect(topX, topY, hSegWidth, segThickness, color);           // Top
            fillRect(leftTopX, leftTopY, segThickness, vSegHeight, color);  // Left Top
            fillRect(midX, midY, hSegWidth, segThickness, color);           // Middle
            fillRect(leftBotX, leftBotY, segThickness, vSegHeight, color);  // Left Bottom
            fillRect(rightBotX, rightBotY, segThickness, vSegHeight, color);// Right Bottom
            fillRect(botX, botY, hSegWidth, segThickness, color);           // Bottom
            break;
        case 7:
            fillRect(topX, topY, hSegWidth, segThickness, color);           // Top
            fillRect(rightTopX, rightTopY, segThickness, vSegHeight, color);// Right Top
            fillRect(rightBotX, rightBotY, segThickness, vSegHeight, color);// Right Bottom
            break;
        case 8:
            fillRect(topX, topY, hSegWidth, segThickness, color);           // Top
            fillRect(leftTopX, leftTopY, segThickness, vSegHeight, color);  // Left Top
            fillRect(rightTopX, rightTopY, segThickness, vSegHeight, color);// Right Top
            fillRect(midX, midY, hSegWidth, segThickness, color);           // Middle
            fillRect(leftBotX, leftBotY, segThickness, vSegHeight, color);  // Left Bottom
            fillRect(rightBotX, rightBotY, segThickness, vSegHeight, color);// Right Bottom
            fillRect(botX, botY, hSegWidth, segThickness, color);           // Bottom
            break;
        case 9:
            fillRect(topX, topY, hSegWidth, segThickness, color);           // Top
            fillRect(leftTopX, leftTopY, segThickness, vSegHeight, color);  // Left Top
            fillRect(rightTopX, rightTopY, segThickness, vSegHeight, color);// Right Top
            fillRect(midX, midY, hSegWidth, segThickness, color);           // Middle
            fillRect(rightBotX, rightBotY, segThickness, vSegHeight, color);// Right Bottom
            fillRect(botX, botY, hSegWidth, segThickness, color);           // Bottom
            break;
    }
}

// Draw a large number with specified parameters
void Display::drawLargeNumber(uint16_t number, int16_t x, int16_t y, int16_t digitWidth, 
                             int16_t digitHeight, int16_t spacing, uint16_t color) {
    // Convert number to string to iterate through digits
    char numStr[6]; // Max 5 digits for 16-bit int plus null terminator
    sprintf(numStr, "%d", number);
    
    int16_t curX = x;
    for (int i = 0; numStr[i] != '\0'; i++) {
        uint8_t digit = numStr[i] - '0';  // Convert char to int
        drawLargeDigit(digit, curX, y, digitWidth, digitHeight, color);
        curX += digitWidth + spacing;  // Move to next position
    }
} 