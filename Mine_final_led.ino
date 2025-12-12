/* MINE UNIT - BTS7960 + L298N + POTENTIOMETER CONTROL */
#include <SPI.h>
#include <LoRa.h>

// --- PINS ---
#define TRIG_PIN 12
#define ECHO_PIN 13   
#define RPWM_PIN 25   // Main Pump (BTS7960)
#define FLOW_PIN 15   
#define TURB_PIN 34   
#define POT_PIN  35   // <--- NEW: Potentiometer for Manual Speed

// --- L298N & LED PINS ---
#define TURB_IN1 32   // L298N IN1
#define TURB_IN2 33   // L298N IN2
#define LED_RED  26   // Low Level Warning
#define LED_GREEN 27  // Safe Level Indicator

// --- LORA PINS ---
#define SS 5
#define RST 14
#define DIO0 4 

// --- VARIABLES ---
const int TOTAL_HEIGHT = 40; 
const int DEAD_ZONE = 20;    

int waterLevel = 0;
long duration; 
float distance;

volatile int flowPulseCount = 0; 
float flowRate = 0.0;
unsigned int flowMilliLitres = 0;
unsigned long totalMilliLitres = 0;
unsigned long oldTime = 0;

int turbidityValue = 0;
int turbidityThreshold = 2000; 

// --- TURBIDITY TIMER VARIABLES ---
unsigned long relayStartTime = 0;
bool isTurbPumpActive = false;   
bool turbPumpHasRun = false;     

// Control
bool isAutoMode = true;
int manualSpeed = 0;
int currentSpeed = 0;

unsigned long lastSendTime = 0;
unsigned long lastUltraMeasure = 0;

const int freq = 5000; const int pwmChannel = 0; const int resolution = 8;

void IRAM_ATTR pulseCounter() { flowPulseCount++; }

void setup() {
  Serial.begin(115200);

  // Pins
  pinMode(TRIG_PIN, OUTPUT); pinMode(ECHO_PIN, INPUT);
  pinMode(LED_RED, OUTPUT); pinMode(LED_GREEN, OUTPUT);
  pinMode(TURB_IN1, OUTPUT); pinMode(TURB_IN2, OUTPUT);
  pinMode(POT_PIN, INPUT); // <--- Potentiometer Input

  digitalWrite(TURB_IN1, LOW); 
  digitalWrite(TURB_IN2, LOW);
  
  // Main Pump PWM
  ledcSetup(pwmChannel, freq, resolution);
  ledcAttachPin(RPWM_PIN, pwmChannel); 

  pinMode(FLOW_PIN, INPUT_PULLUP); attachInterrupt(digitalPinToInterrupt(FLOW_PIN), pulseCounter, FALLING);

  // LoRa
  LoRa.setPins(SS, RST, DIO0);
  if (!LoRa.begin(433E6)) { Serial.println("LoRa Error"); while (1); }
  
  // Settings + CRC
  LoRa.setTxPower(20, PA_OUTPUT_PA_BOOST_PIN);
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.setSyncWord(0x22);
  LoRa.enableCrc();
  
  Serial.println("Mine Unit Ready.");
}

void loop() {
  // 1. ULTRASONIC
  if (millis() - lastUltraMeasure > 200) {
    digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    duration = pulseIn(ECHO_PIN, HIGH, 30000); 
    
    if (duration > 0) {
      distance = (duration * 0.0343) / 2;
      if (distance < DEAD_ZONE) distance = DEAD_ZONE; 
      if (distance > TOTAL_HEIGHT) distance = TOTAL_HEIGHT;
      waterLevel = TOTAL_HEIGHT - (int)distance;
    }
    lastUltraMeasure = millis();
  }

  // 2. SENSORS
  turbidityValue = analogRead(TURB_PIN);
  
  if ((millis() - oldTime) > 1000) {
    detachInterrupt(digitalPinToInterrupt(FLOW_PIN));
    flowRate = ((1000.0 / (millis() - oldTime)) * flowPulseCount) / 7.5;
    flowMilliLitres = (flowRate / 60) * 1000;
    totalMilliLitres += flowMilliLitres;
    flowPulseCount = 0; oldTime = millis();
    attachInterrupt(digitalPinToInterrupt(FLOW_PIN), pulseCounter, FALLING);
  }

  // 3. LISTEN FOR COMMANDS (Only for Auto/Manual Switch now)
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String incoming = LoRa.readString(); 
    if (incoming.length() > 0) {
      int commaIndex = incoming.indexOf(',');
      if (commaIndex > 0) {
        int modeVal = incoming.substring(0, commaIndex).toInt();
        // We ignore the speed sent via LoRa because we use the Potentiometer now
        isAutoMode = (modeVal == 1);
      }
    }
  }

  // 4. CONTROL LOGIC
  
  // --- LED STATUS ---
  if (waterLevel < 3) {
    digitalWrite(LED_RED, HIGH);   // Low Level
    digitalWrite(LED_GREEN, LOW);
  } else {
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_GREEN, HIGH); // Safe Level
  }
  
  // --- TURBIDITY PUMP (L298N) ---
  if (isTurbPumpActive) {
    if (millis() - relayStartTime > 5000) { // 5 Second Timer
      digitalWrite(TURB_IN1, LOW);
      digitalWrite(TURB_IN2, LOW);
      isTurbPumpActive = false;
    }
  } 
  else {
    if (turbidityValue < turbidityThreshold) { // Dirty
      if (!turbPumpHasRun) {
        digitalWrite(TURB_IN1, HIGH);
        digitalWrite(TURB_IN2, LOW);
        relayStartTime = millis();
        isTurbPumpActive = true;
        turbPumpHasRun = true; 
      }
    } else { 
      turbPumpHasRun = false; 
    }
  }

  // --- MAIN PUMP LOGIC (BTS7960) ---
  if (isAutoMode) {
    // AUTOMATIC: Speed based on Water Level
    if (waterLevel < 3) currentSpeed = 0;
    else if (waterLevel >= 3 && waterLevel < 19) currentSpeed = map(waterLevel, 3, 19, 150, 255);
    else currentSpeed = 255;
  } 
  else {
    // MANUAL: Speed based on Potentiometer
    int potValue = analogRead(POT_PIN); // Read Pot (0-4095)
    
    // Map Pot value to PWM (0-255)
    int manualPotSpeed = map(potValue, 0, 4095, 0, 255);
    
    // Safety Cutoff still applies!
    if (waterLevel >= 3) {
      currentSpeed = manualPotSpeed;
    } else {
      currentSpeed = 0; // Force stop if water is too low
    }
  }
  
  ledcWrite(pwmChannel, currentSpeed);

  // 5. SEND DATA
  if (millis() - lastSendTime > 1000) {
    char dataPacket[50];
    sprintf(dataPacket, "%02d,%s,%04lu,%04d", 
            waterLevel, String(flowRate, 1).c_str(), totalMilliLitres, turbidityValue);
    LoRa.beginPacket(); LoRa.print(dataPacket); LoRa.endPacket();
    lastSendTime = millis();
  }
}