#include "Display.h"

// Font for text rendering
const GFXfont* currentFont = nullptr;
int16_t cursor_x = 0;
int16_t cursor_y = 0;
uint16_t textColor = COLOR_BLACK;

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
    reset();
    initDisplay();
    
    // Clear display
    fillScreen(COLOR_BLACK);
    update();
    
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
    
    // Based on the working example's initialization sequence
    sendCommand(0x06);   // Booster Soft Start
    sendData(0x17);
    sendData(0x17);
    sendData(0x28);
    sendData(0x17);
    
    sendCommand(0x04);   // POWER ON
    waitUntilIdle();
    
    sendCommand(0x00);   // PANEL SETTING
    sendData(0x0F);      // KW-3f KWR-2F BWROTP 0f BWOTP 1f
    
    sendCommand(0x50);   // VCOM AND DATA INTERVAL SETTING
    sendData(0x20);
    sendData(0x07);
    
    Serial.println("Display: Init commands sent");
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
    while(digitalRead(_busy_pin) == HIGH) {
        delay(10);
    }
    Serial.println("Display: Display is idle now");
}

void Display::update() {
    Serial.println("Display: Updating display...");
    
    // Send black buffer data
    sendCommand(0x10);
    for (uint32_t i = 0; i < WIDTH * HEIGHT / 8; i++) {
        sendData(_buffer[i]);
    }
    
    // Send red buffer data (we're using B/W display so just send 0s)
    sendCommand(0x13);
    for (uint32_t i = 0; i < WIDTH * HEIGHT / 8; i++) {
        sendData(0x00);
    }
    
    // Refresh display
    sendCommand(0x12);
    delay(10);  // Necessary delay
    waitUntilIdle();
    
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
        // Draw CO2 value in large font
        setFont(&FreeMonoBold24pt7b);
        setTextColor(COLOR_WHITE);
        setCursor(20, 60);
        print("CO2: ");
        print(data.co2);
        print(" ppm");
        
        // Add warning icon if CO2 is high
        if (data.co2 >= _co2_alarm_threshold) {
            fillRect(550, 30, 30, 50, COLOR_WHITE);
            setCursor(545, 70);
            setTextColor(COLOR_BLACK);
            setFont(&FreeMonoBold18pt7b);
            print("!");
            setTextColor(COLOR_WHITE);
        }
        
        // Draw temperature and humidity in medium font
        setFont(&FreeMonoBold18pt7b);
        setTextColor(COLOR_WHITE);
        setCursor(20, 120);
        print("Temp: ");
        print(data.temperature, 1);
        print(" C  Hum: ");
        print(data.humidity, 1);
        print("%");
        
        // Draw bar chart
        drawBarChart(co2History, historyIndex);
        
        // Show update time
        setFont(&FreeMonoBold12pt7b);
        setTextColor(COLOR_WHITE);
        setCursor(20, 160);
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
    int chartY = 220;
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
            minCO2 = min(minCO2, co2History[i]);
            maxCO2 = max(maxCO2, co2History[i]);
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
        minCO2 = max(0, (int)avgCO2 - 250);
        maxCO2 = avgCO2 + 250;
    }
    
    minCO2 = max(0, (int)minCO2);
    maxCO2 = max(1000, (int)maxCO2);
    
    // Round to nice values
    minCO2 = (minCO2 / 100) * 100;
    maxCO2 = ((maxCO2 + 99) / 100) * 100;
    
    // Draw bars
    int barWidth = (chartWidth - 10) / _data_history_size;
    barWidth = max(barWidth, 1);
    
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