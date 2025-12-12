/* ADMIN UNIT - OFFLINE SUPPORT + BUZZER + BACKGROUND WIFI */
#include <SPI.h>
#include <LoRa.h>
#include <ThingerESP32.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h> // Required for manual WiFi handling

#define USERNAME "ShashankT"
#define DEVICE_ID "Mine_water_extruding"
#define DEVICE_CREDENTIAL "Mine_water_extruding"
#define SSID "AKSHAY"
#define PASSWORD "12345678"

ThingerESP32 thing(USERNAME, DEVICE_ID, DEVICE_CREDENTIAL);
LiquidCrystal_I2C lcd(0x27, 20, 4); 

// --- PINS ---
#define SS 5
#define RST 14
#define DIO0 4  // Make sure this matches your physical wire!
#define BUZZER_PIN 13 // <--- NEW: Buzzer Pin

// --- VARIABLES ---
int rem_waterLevel = 0; 
float rem_flowRate = 0.0;
unsigned long rem_totalVol = 0; 
int rem_turbidityRaw = 0;
int rem_turbidityPercent = 0;

// Control
bool cmd_AutoMode = false; // Default to Manual (False) for safety
int cmd_Speed = 0;
bool prev_AutoMode = false; 
int prev_Speed = 0;

// Timers
unsigned long lastHeartbeat = 0;
unsigned long lastWiFiCheck = 0;
unsigned long buzzerStartTime = 0;
bool isBuzzerActive = false;
bool isOnline = false;

void setup() {
  Serial.begin(115200);
  
  // Hardware Init
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  
  lcd.init(); lcd.backlight(); 
  lcd.setCursor(0,0); lcd.print("Mine Admin System");
  
  // --- 1. WIFI SETUP (With Timeout) ---
  lcd.setCursor(0,1); lcd.print("Connecting WiFi...");
  
  // We handle WiFi manually so it doesn't block forever
  WiFi.begin(SSID, PASSWORD);
  
  unsigned long startAttempt = millis();
  bool connected = false;
  
  // Try to connect for 5 seconds
  while (millis() - startAttempt < 10000) {
    if (WiFi.status() == WL_CONNECTED) {
      connected = true;
      break;
    }
    delay(100);
  }

  if (connected) {
    isOnline = true;
    lcd.setCursor(0,1); lcd.print("WiFi Connected!   ");
    // Thinger setup
    thing.add_wifi(SSID, PASSWORD); // Pass creds to thinger library
  } else {
    isOnline = false;
    lcd.setCursor(0,1); lcd.print("Offline Mode      ");
    cmd_AutoMode = false; // Force Manual Mode if Offline
  }
  delay(1000);

  // --- 2. LORA SETUP ---
  LoRa.setPins(SS, RST, DIO0);
  if (!LoRa.begin(433E6)) { 
    lcd.setCursor(0,2); lcd.print("LoRa Error!"); 
    while (1); 
  }

  // Fast Settings + CRC
  LoRa.setTxPower(20, PA_OUTPUT_PA_BOOST_PIN);
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.setSyncWord(0x22);
  LoRa.enableCrc();
  
  lcd.clear(); lcd.print("System Ready"); 
  if(!isOnline) { lcd.setCursor(0,1); lcd.print("(Offline)"); }
  delay(1000);

  // --- 3. THINGER RESOURCES ---
  thing["MineData"] >> [](pson& out){
    out["Level_cm"] = rem_waterLevel; 
    out["Flow_Lmin"] = rem_flowRate;
    out["Total_mL"] = rem_totalVol; 
    out["Turbidity"] = rem_turbidityRaw;
  };
  thing["ModeControl"] << [](pson& in){ 
    if(in.is_empty()) in = cmd_AutoMode; 
    else cmd_AutoMode = in; 
  };
  thing["SpeedControl"] << [](pson& in){ 
    if(in.is_empty()) in = cmd_Speed; 
    else cmd_Speed = in; 
  };
}

void loop() {
  // --- 1. BACKGROUND WIFI CHECK ---
  // If we have internet, handle Thinger.
  if (WiFi.status() == WL_CONNECTED) {
    thing.handle();
    if (!isOnline) {
       isOnline = true; // We just recovered connection
       // Optional: Beep once to say "Online"
    }
  } 
  else {
    isOnline = false;
    // Try to reconnect every 30 seconds (Non-blocking)
    if (millis() - lastWiFiCheck > 30000) {
      WiFi.reconnect();
      lastWiFiCheck = millis();
    }
  }

  // --- 2. BUZZER LOGIC (Non-Blocking) ---
  // Detect Mode Change
  if (cmd_AutoMode != prev_AutoMode) {
    // Mode changed! Start Buzzer
    digitalWrite(BUZZER_PIN, HIGH);
    buzzerStartTime = millis();
    isBuzzerActive = true;
  }
  
  // Turn off buzzer after 2 seconds
  if (isBuzzerActive && (millis() - buzzerStartTime > 2000)) {
    digitalWrite(BUZZER_PIN, LOW);
    isBuzzerActive = false;
  }

  // --- 3. LISTEN FOR LORA DATA ---
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String incoming = LoRa.readString();
    
    if (incoming.length() > 0) {
      int idx1 = incoming.indexOf(',');
      int idx2 = incoming.indexOf(',', idx1 + 1);
      int idx3 = incoming.indexOf(',', idx2 + 1);
      
      if (idx1 > 0 && idx2 > 0 && idx3 > 0) {
        rem_waterLevel = incoming.substring(0, idx1).toInt();
        rem_flowRate = incoming.substring(idx1 + 1, idx2).toFloat();
        rem_totalVol = incoming.substring(idx2 + 1, idx3).toInt();
        rem_turbidityRaw = incoming.substring(idx3 + 1).toInt();
        
        // Map 1000-3000 to 0-100%
        rem_turbidityPercent = map(rem_turbidityRaw, 1000, 3000, 0, 100);
        if(rem_turbidityPercent > 100) rem_turbidityPercent = 100;
        if(rem_turbidityPercent < 0) rem_turbidityPercent = 0;
        
        updateLCD();
      }
    }
  }

  // --- 4. SEND COMMANDS ---
  bool dataChanged = (cmd_AutoMode != prev_AutoMode) || (cmd_Speed != prev_Speed);
  bool timeForHeartbeat = (millis() - lastHeartbeat > 2000); 

  if (dataChanged || timeForHeartbeat) {
    String cmdPacket = String(cmd_AutoMode ? 1 : 0) + "," + String(cmd_Speed);
    LoRa.beginPacket(); LoRa.print(cmdPacket); LoRa.endPacket();
    
    // Update Memory
    prev_AutoMode = cmd_AutoMode; 
    prev_Speed = cmd_Speed;
    lastHeartbeat = millis();
  }
}

void updateLCD() {
  lcd.setCursor(0, 0); 
  lcd.print("Lvl:"); 
  if(rem_waterLevel < 10) lcd.print("0");
  lcd.print(rem_waterLevel); lcd.print("cm  M:");
  lcd.print(cmd_AutoMode ? "Auto" : "Man ");
  
  lcd.setCursor(0, 1); 
  lcd.print("Flow: "); lcd.print(rem_flowRate); lcd.print(" L/m  ");
  
  lcd.setCursor(0, 2); 
  lcd.print("Vol : "); lcd.print(rem_totalVol/1000); lcd.print(" L    ");
  
  lcd.setCursor(0, 3); 
  lcd.print("Pure: "); lcd.print(rem_turbidityPercent); lcd.print("% ");
  
  if (!isOnline) {
    lcd.print("(!Wifi)");
  } else {
    lcd.print("       "); // Clear space
  }
}