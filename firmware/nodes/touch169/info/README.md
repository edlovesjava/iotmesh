# Waveshare ESP32-S3-Touch-LCD-1.69 Reference

## Product Links

- **Product Page**: https://www.waveshare.com/esp32-s3-touch-lcd-1.69.htm
- **Wiki/Documentation**: https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.69
- **Demo Code**: https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.69#Demo

## Hardware Specifications

| Component | Specification |
|-----------|---------------|
| MCU | ESP32-S3R8 (Xtensa 32-bit LX7 dual-core, 240MHz) |
| SRAM | 512KB |
| ROM | 384KB |
| PSRAM | 8MB (onboard) |
| Flash | 16MB (external) |
| Display | 1.69" IPS LCD, 240x280 pixels, 262K colors |
| Display Driver | ST7789V2 |
| Touch Controller | CST816T (capacitive) |
| IMU | QMI8658 (3-axis accelerometer + 3-axis gyroscope) |
| RTC | PCF85063 |
| WiFi | 2.4GHz 802.11 b/g/n |
| Bluetooth | BLE 5.0 |
| USB | USB-C (power + programming) |
| Battery | 3.7V LiPo support (MX1.25 connector) |

## Pinout

### Display (ST7789V2 - SPI)

| Function | GPIO | Notes |
|----------|------|-------|
| MOSI | 7 | SPI data |
| SCK | 6 | SPI clock |
| CS | 5 | Chip select |
| DC | 4 | Data/Command |
| RST | 8 | Reset |
| BL | 15 | Backlight (PWM capable) |

### I2C Bus

| Function | GPIO | Notes |
|----------|------|-------|
| SDA | 11 | I2C data |
| SCL | 10 | I2C clock |

### I2C Addresses

| Device | Address | Description |
|--------|---------|-------------|
| CST816T | 0x15 | Touch controller |
| QMI8658 | 0x6B | IMU (accelerometer/gyroscope) |
| PCF85063 | 0x51 | RTC |

### Other Pins

| Function | GPIO | Notes |
|----------|------|-------|
| BOOT | 0 | Boot button |
| Battery ADC | - | Voltage monitoring |

## TFT_eSPI Configuration

Required build flags for PlatformIO:

```ini
-DUSER_SETUP_LOADED=1
-DST7789_DRIVER=1
-DTFT_WIDTH=240
-DTFT_HEIGHT=280
-DUSE_HSPI_PORT=1
-DTFT_MOSI=7
-DTFT_SCLK=6
-DTFT_CS=5
-DTFT_DC=4
-DTFT_RST=8
-DTFT_BL=15
-DTFT_BACKLIGHT_ON=HIGH
-DTFT_MISO=-1
-DSPI_FREQUENCY=27000000
```

## Arduino_GFX Configuration

From Waveshare demo code:

```cpp
#include "Arduino_GFX_Library.h"

Arduino_DataBus *bus = new Arduino_ESP32SPI(4, 5, 6, 7);  // DC, CS, SCK, MOSI
Arduino_GFX *gfx = new Arduino_ST7789(bus, 8, 0, true, 240, 280, 0, 20, 0, 0);

// Backlight
pinMode(15, OUTPUT);
digitalWrite(15, HIGH);
```

## Libraries

### Display
- **TFT_eSPI** (Bodmer) - Fast SPI display library
- **Arduino_GFX** (moononournation) - Alternative display library

### Touch
- **CST816S** or similar I2C touch library

### IMU
- **QMI8658** library for motion sensing

### RTC
- **PCF85063** library for real-time clock

## Notes

1. **ESP32-S3 SPI**: Must use `USE_HSPI_PORT=1` as VSPI is reserved for flash/PSRAM
2. **Display Offset**: ST7789 on this board has a 20-pixel Y offset (handled in Arduino_GFX constructor)
3. **USB Serial**: Uses native USB CDC, access via `USBSerial` or `Serial` depending on Arduino core version
4. **Power Management**: Board includes battery charging circuit and power control pins

See https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.69 for details