# Fallen Brothers -- STM32 Fall Detection System

A real-time fall detection system built on the **B-L4S5I-IOT01A** (STM32L4S5VIT6) development board. The system uses on-board sensors and the ISM43362 Wi-Fi module to detect falls, alert nearby personnel via buzzer and LED, and send remote notifications to a Telegram group chat.

## Presentation Slides

[View Presentation (PPTX)](docs/Assignment_52_A0309132N_A0309266Y.pptx)

## Features

- **Fall detection state machine** -- freefall, impact, long-lie confirmation, and recovery stages using accelerometer, gyroscope, and barometer data
- **Moving average filter** -- implemented in both ARM assembly and C for accelerometer noise reduction
- **Barometric altitude tracking** -- LPS22HB pressure sensor with EMA filtering for altitude-based fall validation
- **Buzzer and LED alerts** -- immediate local notification on confirmed fall
- **Wi-Fi push notifications** -- HTTP POST to [ntfy.sh](https://ntfy.sh) via the on-board ISM43362 Wi-Fi module over SPI
- **Telegram integration** -- Python bridge script forwards ntfy.sh alerts to a Telegram group chat via the Bot API

## Hardware

| Component | Description |
|-----------|-------------|
| B-L4S5I-IOT01A | STM32L4+ Discovery kit with IoT node |
| LSM6DSL | On-board accelerometer and gyroscope |
| LPS22HB | On-board barometric pressure sensor |
| ISM43362-M3G-L44 | On-board Wi-Fi module (802.11 b/g/n, 2.4 GHz) |
| Grove Buzzer | External buzzer on Arduino header D3 (PB0) |
| Base Shield V2 | Grove connector interface shield |

## Project Structure

```
Core/
  Inc/          -- Application headers (main.h, wifi.h, es_wifi_io.h)
  Src/          -- Application source (main.c, wifi.c, es_wifi_io.c, mov_avg.s)
Drivers/
  BSP/          -- Board Support Package (sensor drivers, Wi-Fi AT command driver)
  STM32L4xx_HAL_Driver/  -- STM32 HAL library
  CMSIS/        -- ARM CMSIS headers
telegram-bridge/
  fallalertbot.py   -- Python bridge script (ntfy.sh -> Telegram)
docs/
  Assignment_52_A0309132N_A0309266Y.pptx   -- Presentation slides
```

## System Architecture

```
STM32 Board  -->  Phone Hotspot  -->  ntfy.sh (HTTP)  -->  Python Script  -->  Telegram Bot API (HTTPS)  -->  Group Chat
```

The Wi-Fi module only supports plain HTTP. ntfy.sh acts as a protocol bridge -- the board posts alerts over HTTP, and the Python script reads them over HTTPS and forwards to Telegram.

## Build and Flash

1. Open the project in **STM32CubeIDE** (v1.19.0 or later)
2. Build the project (Project > Build All)
3. Flash to the board via ST-Link (Run > Debug)

## Wi-Fi Configuration

In `Core/Src/main.c`, update the Wi-Fi credentials before building:

```c
WIFI_Connect("YOUR_SSID", "YOUR_PASSWORD", WIFI_ECN_WPA_WPA2_PSK);
```

The hotspot must be 2.4 GHz (the ISM43362 does not support 5 GHz).

## Telegram Bridge

Requires Python 3 with the `requests` library. Run on any internet-connected machine:

```
pip install requests
python telegram-bridge/fallalertbot.py
```

Update `TELEGRAM_TOKEN` and `CHAT_ID` in the script with your own bot credentials.

## Authors

- A0309132N
- A0309266Y

CG2028 Assignment, AY2025/26
