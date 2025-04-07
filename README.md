# SmartBuoy Ocean Data Acquisition

A modular ocean data acquisition system built around an STM32 “data‐logger” and an ESP32 “gateway” to measure water temperature, IMU orientation, and wave height, then forward the data via Wi‑Fi to a REST API.

---

## Table of Contents

- [Features](#features)  
- [Hardware Overview](#hardware-overview)  
- [Software Architecture](#software-architecture)  
  - [STM32 Serial Data Logger](#stm32-serial-data-logger)  
  - [ESP32 HTTP Gateway](#esp32-http-gateway)  
- [Wiring & Pinout](#wiring--pinout)  
- [Getting Started](#getting-started)  
  - [Prerequisites](#prerequisites)  
  - [Building & Uploading STM32 Code](#building--uploading-stm32-code)  
  - [Building & Uploading ESP32 Code](#building--uploading-esp32-code)  
- [Data Format](#data-format)  
- [API Integration](#api-integration)  
- [Calibration & Tuning](#calibration--tuning)  
- [Troubleshooting](#troubleshooting)  
- [License](#license)  

---

## Features

- **Water Temperature** via DS18B20 (DallasTemperature over OneWire)  
- **IMU Orientation & Acceleration** via BNO055 (Adafruit Sensor library)  
- **Wave Height** estimation from vertical acceleration peaks  
- **UART** link (115200 bps) between STM32 and ESP32  
- **Wi‑Fi HTTP POST** of JSON payloads to a configurable REST endpoint  

---

## Hardware Overview

1. **STM32 “Data Logger”**  
   - BNO055 9‑DOF IMU  
   - DS18B20 waterproof temperature probe  
   - Serial1 UART (PA9 TX / PA10 RX)  

2. **ESP32 “Gateway”**  
   - UART2 interface (GPIO 18 TX / 19 RX) to STM32  
   - 2.4 GHz Wi‑Fi client  

---

## Software Architecture

### STM32 Serial Data Logger

- **Language & Framework**: Arduino‑style C++ on STM32duino  
- **Key Libraries**:  
  - `Adafruit_BNO055` for IMU  
  - `DallasTemperature` + `OneWire` for water temp  
- **Main Tasks**:  
  1. **Baseline Calibration**: sample 50 Z‑axis accel readings every 5 min  
  2. **Continuous IMU Update**: track z‑axis peaks for wave height  
  3. **Temp Update**: read DS18B20 every 1 s  
  4. **Wave Height Calc**: every 3 s, Δz × calibration factor  
  5. **Serial Send**: every 3 s, send CSV:  
     ```csv
     heading,roll,pitch,imu_temp,wave_height,accelX,accelY,accelZ,water_temp
     ```  
- **Source**: `stm32_serialdata.ino`

### ESP32 HTTP Gateway

- **Language & Framework**: Arduino‑style C++ on ESP32  
- **Key Libraries**: `WiFi.h`, `HTTPClient.h`  
- **Main Tasks**:  
  1. **Wi‑Fi Connection** to SSID `LouraBuoy`  
  2. **UART2 Read** from STM32 (115200 bps)  
  3. **CSV Parsing** into floats  
  4. **JSON Payload Construction**:  
     ```json
     {
       "project": "smartbuoy",
       "data": "{'heading': X, 'roll': Y, … }"
     }
     ```  
  5. **HTTP POST** to `http://192.168.1.106:2612/loura-marine`  
     - Header `X-API-KEY: azp-261211102024-aquadrox`  
     - Content-Type `application/json`  
  6. **Retry** every 3 s  
- **Source**: `esp32_http.ino`

---

## Wiring & Pinout

| Component        | STM32 Pin | ESP32 Pin  | Notes                         |
|------------------|-----------|------------|-------------------------------|
| BNO055 SDA       | PB7       | —          | Shared with Wire bus on STM32 |
| BNO055 SCL       | PB6       | —          |                               |
| DS18B20 Data     | PC7       | —          | OneWire bus                   |
| STM32 UART TX    | PA9       | → GPIO 19  | ESP32 RX                      |
| STM32 UART RX    | PA10      | ← GPIO 18  | ESP32 TX                      |
| ESP32 Wi‑Fi      | —         | —          | SSID/password in code         |

---

## Getting Started

### Prerequisites

- Arduino IDE (≥1.8.x) or PlatformIO  
- STM32duino core installed  
- ESP32 Arduino core installed  
- BNO055 & DallasTemperature libraries  

### Building & Uploading STM32 Code

1. Open `stm32_serialdata.ino` in Arduino IDE.  
2. Select your STM32 board (e.g. “Generic STM32F1 series”).  
3. Install & include:  
   - **Adafruit BNO055**  
   - **Adafruit Unified Sensor**  
   - **OneWire**  
   - **DallasTemperature**  
4. Upload at **115200 bps**.

### Building & Uploading ESP32 Code

1. Open `esp32_http.ino` in Arduino IDE.  
2. Select your ESP32 board (e.g. “ESP32 Dev Module”).  
3. Update Wi‑Fi SSID/password, REST endpoint & API key if needed.  
4. Upload at **115200 bps**.

---

## Data Format

- **CSV over UART** from STM32:  
  ```csv
  heading,roll,pitch,imu_temp,wave_height,accelX,accelY,accelZ,water_temp
  ```
  
---

## API Integration

- **Endpoint**: `POST /loura-marine`  
- **Headers**:  
  - `Content-Type: application/json`  
  - `X-API-KEY: <your-api-key>`  
- **Body**:  
  ```json
  {
    "project": "smartbuoy",
    "data": "{'heading': …, 'water_temp': …}"
  }
- **Response**: HTTP 200 on success
  
---

## Calibration & Tuning

- **Wave Height Factor**  
  - Default `0.5/10.7` in code — adjust to match physical buoy sensitivity.  
- **Baseline Recalibration**  
  - Every 5 min if motion <0.2 g.  

---

## Troubleshooting

- **No BNO055 detected**  
  - Check I²C wiring, pull‑ups, and address (`0x28` vs `0x29`).  
- **DS18B20 reads −127 °C**  
  - Verify OneWire bus wiring and sensor power.  
- **Wi‑Fi fails to connect**  
  - Confirm SSID/password and AP range.  
- **HTTP POST errors**  
  - Inspect `httpResponseCode` in serial log.  

---
