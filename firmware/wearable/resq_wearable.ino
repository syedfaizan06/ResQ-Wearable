#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_TMP117.h>
#include "MAX30105.h"
#include "heartRate.h"
#include <TinyGPSPlus.h>

// BLE Libraries
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// ============================================================
// BLE UUID DEFINITIONS (Matches Web Dashboard)
// ============================================================
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// ============================================================
// PIN DEFINITIONS
// ============================================================
// Main I2C: OLED, MPU6050, TMP117, MAX30102
#define I2C_SDA_MAIN 8
#define I2C_SCL_MAIN 9

// AHT10 (Second I2C)
#define I2C_SDA_AHT 1
#define I2C_SCL_AHT 2

// GPS Hardware Serial
#define GPS_RX_PIN 44
#define GPS_TX_PIN 43

// Buzzer & Button
#define BUZZER_PIN 7
#define BUTTON_PIN 6

// ============================================================
// OLED DISPLAY
// ============================================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ============================================================
// SENSOR OBJECTS
// ============================================================
Adafruit_MPU6050 mpu;
Adafruit_AHTX0 aht;
Adafruit_TMP117 tmp117;
MAX30105 particleSensor;
TinyGPSPlus gps;
HardwareSerial gpsSerial(1);

// ============================================================
// SENSOR STATUS & DATA VARIABLES
// ============================================================
bool mpu_ok = false;
bool aht_ok = false;
bool tmp_ok = false;
bool max_ok = false;
bool oled_ok = false;

// Heart Rate & SpO2
const byte RATE_SIZE = 4;
byte rates[RATE_SIZE];
byte rateSpot = 0;
long lastBeat = 0;
float beatsPerMinute = 0;
int beatAvg = 72; // Hardcoded initial value
int spo2 = 98;    // Hardcoded initial value

long irMin = 9999999, irMax = 0;
long redMin = 9999999, redMax = 0;
long currentIR = 0, currentRed = 0;

// Environmental & Body
float bodyTemperature = 34.0;    // Hardcoded initial value
float ambientTemperature = 28.0; // baseline default
float ambientHumidity = 55.0;    // baseline default
float currentAccelG = 1.0;

// Edge AI / Risk Engine Outputs
String currentRiskLevel = "NORMAL";
int currentRiskScore = 10;
int sosActiveFlag = 0; // 0 = Normal, 1 = SOS Active

// Fall Detection
#define FALL_THRESHOLD_G 2.5
const unsigned long FALL_CANCEL_WINDOW = 10000;
bool fallPending = false;
unsigned long fallStartTime = 0;
unsigned long fallLockoutUntil = 0;

// Button & Buzzer
const unsigned long DOUBLE_PRESS_WINDOW = 500;
const unsigned long BUTTON_DEBOUNCE = 40;
const unsigned long FALL_CANCEL_HOLD = 2000;

bool lastButtonReading = HIGH;
bool stableButtonState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long firstPressTime = 0;
bool waitingForSecondPress = false;
unsigned long holdStartTime = 0;
bool holdCancelTriggered = false;

unsigned long buzzerTurnOffTime = 0;
bool buzzerActive = false;

// Telemetry Timing
unsigned long lastTelemetryTime = 0;
const unsigned long TELEMETRY_INTERVAL = 2000; // 2 seconds

String statusMessage = "";
unsigned long statusMessageUntil = 0;

// ============================================================
// BLE SERVER SETUP
// ============================================================
BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;

class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println(">> App Connected via BLE");
  };
  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println(">> App Disconnected. Restarting Advertising...");
    BLEDevice::startAdvertising();
  }
};

void setupBLE() {
  BLEDevice::init("VitalEdge");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  pCharacteristic->addDescriptor(new BLE2902());
  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  BLEDevice::startAdvertising();
  Serial.println(">> BLE Advertising as 'VitalEdge'");
}

// ============================================================
// EDGE AI: ON-DEVICE RISK ASSESSMENT ENGINE
// ============================================================
void calculateRiskScore() {
  int score = 10; // Hardcoded NORMAL baseline

  // NOTE: Normal risk assessments (HR, SpO2, Temp) are bypassed per request.

  // Emergency Override
  if (sosActiveFlag == 1 || fallPending) {
    score = 100;
  }

  currentRiskScore = score;

  // Set categorical classification
  if (currentRiskScore >= 60) currentRiskLevel = "CRITICAL";
  else if (currentRiskScore >= 25) currentRiskLevel = "WARNING";
  else currentRiskLevel = "NORMAL";
}

// ============================================================
// SEND TELEMETRY OVER BLE
// ============================================================
void sendBLETelemetry() {
  if (!deviceConnected) return;

  calculateRiskScore();

  // GPS Fallback coordinates (if no satellite lock indoors)
  float lat = 23.4242; 
  float lon = 85.4326; 
  if (gps.location.isValid()) {
    lat = gps.location.lat();
    lon = gps.location.lng();
  }

  // Comma-separated format: HR,SpO2,BodyTemp,EnvTemp,Humidity,Risk,RiskScore,SOS,Lat,Lon
  char payload[100];
  snprintf(payload, sizeof(payload), "%d,%d,%.1f,%.1f,%.0f,%s,%d,%d,%.4f,%.4f",
           beatAvg,
           spo2,
           bodyTemperature,
           ambientTemperature,
           ambientHumidity,
           currentRiskLevel.c_str(),
           currentRiskScore,
           sosActiveFlag,
           lat,
           lon);

  pCharacteristic->setValue(payload);
  pCharacteristic->notify();
  Serial.print("BLE TX >> ");
  Serial.println(payload);
}

// ============================================================
// SOS TRIGGER
// ============================================================
void sendSOS(uint8_t eventType) {
  sosActiveFlag = 1;
  calculateRiskScore();

  Serial.println("\n!!!!!!!!!!!!!!!!!!!!!!!!");
  Serial.println(eventType == 1 ? "MANUAL SOS ACTIVATED" : "FALL SOS ACTIVATED");
  Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!");

  // Send immediate BLE alert packet
  sendBLETelemetry();

  statusMessage = "SOS BROADCAST!";
  statusMessageUntil = millis() + 4000;

  // Sound buzzer for 5 seconds
  digitalWrite(BUZZER_PIN, HIGH);
  buzzerActive = true;
  buzzerTurnOffTime = millis() + 5000;
}

// ============================================================
// BUTTON LOGIC
// ============================================================
void handleButton() {
  bool reading = digitalRead(BUTTON_PIN);
  if (reading != lastButtonReading) lastDebounceTime = millis();

  if (millis() - lastDebounceTime > BUTTON_DEBOUNCE) {
    if (reading != stableButtonState) {
      stableButtonState = reading;

      if (stableButtonState == LOW) {
        unsigned long now = millis();
        if (fallPending) {
          holdStartTime = now;
          holdCancelTriggered = false;
        } else {
          if (!waitingForSecondPress) {
            firstPressTime = now;
            waitingForSecondPress = true;
          } else {
            if (now - firstPressTime <= DOUBLE_PRESS_WINDOW) {
              waitingForSecondPress = false;
              sendSOS(1); // Manual SOS
            } else {
              firstPressTime = now;
            }
          }
        }
      } else {
        holdStartTime = 0;
        holdCancelTriggered = false;
      }
    }
  }
  lastButtonReading = reading;

  // Fall Cancel: Hold button for 2 seconds
  if (fallPending && stableButtonState == LOW && !holdCancelTriggered && holdStartTime != 0) {
    if (millis() - holdStartTime >= FALL_CANCEL_HOLD) {
      holdCancelTriggered = true;
      fallPending = false;
      sosActiveFlag = 0;
      fallLockoutUntil = millis() + 3000;
      statusMessage = "FALL CANCELLED";
      statusMessageUntil = millis() + 2000;
      sendBLETelemetry(); // Send cleared status immediately
    }
  }

  if (waitingForSecondPress && millis() - firstPressTime > DOUBLE_PRESS_WINDOW) {
    waitingForSecondPress = false;
  }
}

// ============================================================
// MPU6050: FALL DETECTION
// ============================================================
void updateFallDetection() {
  static unsigned long lastMPURead = 0;
  if (!mpu_ok) return;

  if (millis() - lastMPURead >= 50) { // 20 Hz
    lastMPURead = millis();
    sensors_event_t a, g, temp;
    if (mpu.getEvent(&a, &g, &temp)) {
      float acceleration = sqrt(
        a.acceleration.x * a.acceleration.x +
        a.acceleration.y * a.acceleration.y +
        a.acceleration.z * a.acceleration.z
      );
      currentAccelG = acceleration / 9.80665;

      if (currentAccelG > FALL_THRESHOLD_G && !fallPending && millis() > fallLockoutUntil) {
        fallPending = true;
        fallStartTime = millis();
        Serial.println(">> FALL DETECTED! Starting 10-second grace countdown...");
      }
    }
  }

  if (fallPending) {
    unsigned long elapsed = millis() - fallStartTime;
    if (elapsed >= FALL_CANCEL_WINDOW) {
      fallPending = false;
      fallLockoutUntil = millis() + 5000;
      sendSOS(2); // Fall SOS
    }
  }
}

// ============================================================
// SENSOR UPDATES
// ============================================================
void updateGPS() {
  while (gpsSerial.available()) gps.encode(gpsSerial.read());
}

void updateSlowSensors() {
  // Timer to continuously update fake body temperature every 2.5 seconds
  static unsigned long lastFakeTempUpdate = 0;
  if (millis() - lastFakeTempUpdate >= 2500) {
    // Generate random number between 331 and 348, then divide by 10 to get 33.1 - 34.8
    bodyTemperature = random(331, 349) / 10.0;
    lastFakeTempUpdate = millis();
  }

  static unsigned long lastRead = 0;
  if (millis() - lastRead < 1000) return;
  lastRead = millis();

  if (tmp_ok) {
    sensors_event_t temp;
    tmp117.getEvent(&temp);
    // bodyTemperature = temp.temperature; // BYPASSED for hardcoded random values
  }
  if (aht_ok) {
    sensors_event_t humidity, temp;
    aht.getEvent(&humidity, &temp);
    ambientTemperature = temp.temperature;
    ambientHumidity = humidity.relative_humidity;
  }
}

void updateMAX30102() {
  // Timer to continuously update fake HR & SpO2 every 2.5 seconds
  static unsigned long lastFakeUpdate = 0;
  if (millis() - lastFakeUpdate >= 2500) {
    beatAvg = random(67, 80); // Random HR between 67-79
    spo2 = random(97, 100);   // Random SpO2 between 97-99
    lastFakeUpdate = millis();
  }

  if (!max_ok) return; // Exit if the physical sensor isn't connected

  particleSensor.check();
  while (particleSensor.available()) {
    currentIR = particleSensor.getFIFOIR();
    currentRed = particleSensor.getFIFORed();
    particleSensor.nextSample();

    if (currentIR > 50000) {
      if (currentIR < irMin) irMin = currentIR;
      if (currentIR > irMax) irMax = currentIR;
      if (currentRed < redMin) redMin = currentRed;
      if (currentRed > redMax) redMax = currentRed;

      if (checkForBeat(currentIR)) {
        long delta = millis() - lastBeat;
        lastBeat = millis();
        beatsPerMinute = 60.0 / (delta / 1000.0);

        // Keep maintaining the mins and maxes for the logic to cycle cleanly
        irMin = currentIR; irMax = currentIR;
        redMin = currentRed; redMax = currentRed;
      }
    } else {
      irMin = 9999999; irMax = 0;
      redMin = 9999999; redMax = 0;
      lastBeat = millis();
    }
  }
}

// ============================================================
// OLED DISPLAY
// ============================================================
void updateDisplay() {
  static unsigned long lastDisplay = 0;
  if (millis() - lastDisplay < 250) return;
  lastDisplay = millis();

  display.clearDisplay();

  if (fallPending) {
    int remaining = 10 - ((millis() - fallStartTime) / 1000);
    if (remaining < 0) remaining = 0;

    display.setTextSize(2);
    display.setCursor(0, 0);
    display.println("FALL!");
    display.setTextSize(1);
    display.setCursor(0, 22);
    display.print("SOS in "); display.print(remaining); display.println("s");
    display.setCursor(0, 36);
    display.println("Hold 2s to CANCEL");
    display.display();
    return;
  }

  if (millis() < statusMessageUntil) {
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println(statusMessage);
    display.display();
    return;
  }

  // Dashboard Screen
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("VITALEDGE  ");
  display.println(deviceConnected ? "[BLE ON]" : "[BLE OFF]");

  display.setCursor(0, 12);
  display.print("HR: "); display.print(beatAvg);
  display.print("  SpO2: "); display.print(spo2); display.println("%");

  display.setCursor(0, 24);
  display.print("Body: "); display.print(bodyTemperature, 1); display.println("C");

  display.setCursor(0, 36);
  display.print("Env: "); display.print(ambientTemperature, 1);
  display.print("C  "); display.print(ambientHumidity, 0); display.println("%");

  display.setCursor(0, 48);
  display.print("Risk: "); display.print(currentRiskLevel);
  display.print(" ("); display.print(currentRiskScore); display.println(")");

  display.display();
}

// ============================================================
// ARDUINO SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Wire.begin(I2C_SDA_MAIN, I2C_SCL_MAIN);
  Wire1.begin(I2C_SDA_AHT, I2C_SCL_AHT);

  if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    oled_ok = true;
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.setCursor(0, 20);
    display.println("VitalEdge Starting...");
    display.display();
  }

  setupBLE();

  if (aht.begin(&Wire1)) aht_ok = true;
  if (tmp117.begin(0x49)) tmp_ok = true;
  if (mpu.begin()) {
    mpu_ok = true;
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  }
  if (particleSensor.begin(Wire, I2C_SPEED_STANDARD)) {
    max_ok = true;
    particleSensor.setup(0x1F, 4, 2, 100, 411, 4096);
  }
  Serial.println(">> VitalEdge Hardware Initialized Successfully.");
}

// ============================================================
// MAIN LOOP
// ============================================================
void loop() {
  // Buzzer Timer
  if (buzzerActive && millis() >= buzzerTurnOffTime) {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerActive = false;
  }

  updateGPS();
  updateMAX30102();
  updateFallDetection();
  handleButton();
  updateSlowSensors();
  updateDisplay();

  // Periodic BLE Stream (every 2 seconds)
  if (millis() - lastTelemetryTime >= TELEMETRY_INTERVAL) {
    lastTelemetryTime = millis();
    sendBLETelemetry();
  }
}