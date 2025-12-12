

## 1. What Our Project Does
This system automates the dewatering process in underground mines by continuously
monitoring water level, flow rate, and turbidity, and then controlling pumps 
accordingly. Two ESP32 units communicate via LoRa—one placed inside the mine
(Mine Unit) and one with the operator (Admin Unit). The system ensures efficient,
safe, and long-range control of the dewatering process.

---

## 2. Problem Statement & Solution

### Problem
Mines face rapid flooding due to groundwater seepage. Manual monitoring is unsafe,
slow, and inefficient. Turbidity changes also require dosing control to avoid pump 
damage. Communication is difficult underground due to lack of network coverage.

### Solution
Our solution uses:
- ESP32-based automated control inside the mine
- Long-range LoRa communication to send live water-level, turbidity, and flow data
- Automatic pump activation with dosing pump control
- Manual override through an Admin Unit with LCD display and buzzer alerts
- Optional cloud connectivity for remote monitoring

This removes the need for manual inspection and ensures faster, safer dewatering.

---

## 3. Features
- Automatic + Manual Pump Modes
- Live water level monitoring (ultrasonic sensor)
- Real-time turbidity detection and dosing pump activation (L298N)
- Flow rate measurement for volume tracking
- Long-range communication using LoRa SX1278
- Admin Unit with:
  - 20x4 I2C LCD display
  - Mode & speed control
  - Buzzer alerts
  - Thinger.io cloud connectivity
- Stable PWM pump control using BTS7960 driver
- Safety logic to prevent dry running

---

## 4. Technologies Used

### Hardware
- ESP32 Dev Module (Mine Unit + Admin Unit)
- LoRa SX1278 
- Ultrasonic Sensor (HC-SR04)
- Flow Sensor (YF-S201 or similar)
- Turbidity Sensor (Analog)
- BTS7960 Motor Driver (Main Pump)
- L298N Motor Driver (Dosing Pump)
- 20x4 I2C LCD (Admin Unit)
- Buzzer
- Potentiometer (Manual control)

### Software & Tools
- Arduino IDE
- ESP32 Core
- LoRa Library (Sandeep Mistry)
- ThingerESP32 (Admin unit cloud support)
- LiquidCrystal_I2C library
- SPI & WiFi (built-in)

---

## 5. Steps to Install and Run

### A. Install Board Package
In Arduino IDE:
1. Open **Boards Manager**
2. Install **ESP32 by Espressif Systems**

---

### B. Install Required Libraries
Go to: **Sketch → Include Library → Manage Libraries**

Install these:
- **LoRa** (Sandeep Mistry)
- **LiquidCrystal_I2C** (Admin unit only)
- **ThingerESP32** (Admin unit only)

Built-in libraries (no installation needed):
- SPI.h  
- WiFi.h  

---

### C. Upload Mine Unit Code
1. Open `project/Mine_unit/Mine_final_led.ino`
2. Select board: **ESP32 Dev Module**
3. Install LoRa library
4. Upload the code
5. Ensure LoRa frequency = 433E6

---

### D. Upload Admin Unit Code
1. Open `project/Admin_unit/Admin_code.ino`
2. Install:
   - LoRa
   - LiquidCrystal_I2C
   - ThingerESP32
3. Upload the code
4. LCD should display system status

---

## 6. Required Environment Variables
(Not strictly required for Arduino projects.)

If using cloud mode:
SSID = "Your WiFi Name"
PASSWORD = "Your WiFi Password"
THINGER_USERNAME = "YourUser"
THINGER_DEVICE_ID = "DeviceID"
THINGER_CREDENTIAL = "DeviceCredential"
