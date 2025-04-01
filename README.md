# Garage CO2 Monitor

A smart CO2 monitoring system built with ESP32 and e-Paper display. This device continuously monitors CO2 levels, temperature, and humidity in your garage or any indoor space, providing a 24-hour history view with an energy-efficient e-Paper display.

## Features

- **Real-time CO2 Monitoring**: Accurate CO2 measurements using the Sensirion SCD4x sensor
- **Environmental Data**: Temperature and humidity monitoring
- **24-hour History**: Visual graph showing CO2 level trends over the last 24 hours
- **Energy Efficient**: E-Paper display only updates when needed, perfect for long-term monitoring
- **Visual Alerts**: Warning indicator when CO2 levels exceed threshold
- **Auto-recovery**: Automatic sensor reconnection if connection is lost
- **Clear Visualization**: Large, easy-to-read numbers and intuitive graph display

## Components Required

### Hardware
- ESP32 Development Board (LILYGO T5 v2.4.1)
- Sensirion SCD40/SCD41 CO2 Sensor
- 6.02" E-Paper Display (648x480 pixels)
- Jumper Wires
- USB-C Cable for power and programming
- Optional: Case for mounting (3D printable)

### Software
- PlatformIO IDE (recommended) or Arduino IDE
- Required Libraries:
  - Sensirion I2C SCD4x
  - Adafruit GFX
  - SPI
  - Wire

## Pin Connections

```
ESP32 (LILYGO T5) -> E-Paper Display
--------------------------------
GPIO 23 (MOSI)   -> DIN
GPIO 18 (SCK)    -> CLK
GPIO 4  (CS)     -> CS
GPIO 16 (DC)     -> DC
GPIO 17 (RST)    -> RST
GPIO 19 (BUSY)   -> BUSY
GND              -> GND
3.3V             -> VCC

ESP32 (LILYGO T5) -> SCD4x Sensor
--------------------------------
GPIO 21 (SDA)    -> SDA
GPIO 22 (SCL)    -> SCL
GND              -> GND
3.3V             -> VCC
```

## Installation

1. Clone this repository:
   ```bash
   git clone https://github.com/yourusername/garage-co2-monitor.git
   ```

2. Open the project in PlatformIO:
   - Open PlatformIO IDE
   - Click "Open Project"
   - Navigate to the cloned repository
   - Select the project folder

3. Install Dependencies:
   - PlatformIO will automatically install required libraries
   - If using Arduino IDE, install the required libraries through Library Manager

4. Configure Settings (optional):
   - Adjust CO2 alarm threshold in `src/main.cpp`
   - Modify update interval if needed
   - Change pin assignments if using different connections

5. Upload the Firmware:
   - Connect your ESP32 board via USB
   - Click the "Upload" button in PlatformIO
   - Wait for the upload to complete

## Usage

1. Power up the device using a USB-C cable
2. The display will show initialization progress
3. Once initialized, you'll see:
   - Current CO2 level in PPM
   - Temperature and humidity readings
   - 24-hour history graph
   - Last update time
4. The display updates every 5 minutes
5. If CO2 levels exceed the threshold (default 1000 PPM):
   - Warning indicator appears
   - Graph bars become hollow

## Troubleshooting

If the sensor fails to initialize:
1. Check power connections
2. Verify I2C wiring (SDA/SCL)
3. Ensure sensor address is correct (0x62)
4. Try power cycling the device

The system will automatically attempt to reconnect to the sensor if connection is lost.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Acknowledgments

- Thanks to LILYGO for the T5 e-Paper board
- Thanks to Sensirion for the excellent CO2 sensor
- Thanks to Adafruit for their GFX library

## Project Status

This project is actively maintained. Feel free to open issues or suggest improvements. 