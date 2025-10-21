#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>

// ---------------- WiFi Configuration ----------------
const char* ssid = "nodeMCU Car";
const char* password = "";
const char* serverIP = "192.168.4.1";

// ---------------- MPU6050 Registers ----------------
const int MPU6050_ADDR = 0x68;
const int PWR_MGMT_1 = 0x6B;
const int ACCEL_XOUT_H = 0x3B;

// ---------------- Gesture Variables ----------------
float pitchThreshold = 35.0;
float rollThreshold = 40.0;
String currentCommand = "S";
String lastCommand = "S";
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 100;

// ---------------- Setup ----------------
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== ESP32 Hand Gesture Controller ===");
  
  // Initialize I2C
  Wire.begin(21, 22);
  Wire.setClock(400000);  // 400kHz
  delay(100);
  
  // Wake up MPU6050
  Serial.println("Initializing MPU6050...");
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(PWR_MGMT_1);
  Wire.write(0);  // Wake up
  byte error = Wire.endTransmission();
  
  if (error == 0) {
    Serial.println("MPU6050 initialized successfully!");
  } else {
    Serial.print("MPU6050 initialization failed! Error: ");
    Serial.println(error);
    while(1) delay(10);
  }
  
  delay(100);
  
  // Connect to WiFi
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi Connection Failed!");
  }
  
  Serial.println("\n=== Gesture Control Active ===");
  Serial.println("Tilt forward: Forward");
  Serial.println("Tilt backward: Backward");
  Serial.println("Tilt left: Left");
  Serial.println("Tilt right: Right");
  Serial.println("Keep flat: Stop\n");
}

// ---------------- Loop ----------------
void loop() {
  // Read accelerometer data
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(ACCEL_XOUT_H);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU6050_ADDR, 6, true);
  
  int16_t ax = Wire.read() << 8 | Wire.read();
  int16_t ay = Wire.read() << 8 | Wire.read();
  int16_t az = Wire.read() << 8 | Wire.read();
  
  // Convert to g-force (±2g range: 16384 LSB/g)
  float accelX = ax / 16384.0;
  float accelY = ay / 16384.0;
  float accelZ = az / 16384.0;
  
  // Calculate pitch and roll
  float pitch = atan2(accelY, sqrt(accelX * accelX + accelZ * accelZ)) * 180 / PI;
  float roll = atan2(-accelX, accelZ) * 180 / PI;
  
  // Get gesture command
  currentCommand = getGestureCommand(pitch, roll);
  
  // Send command
  if (millis() - lastSendTime >= sendInterval) {
    // Always show gesture detection (even without WiFi)
    if (currentCommand != lastCommand) {
      Serial.print("Command: ");
      Serial.print(currentCommand);
      Serial.print(" | Pitch: ");
      Serial.print(pitch, 1);
      Serial.print("° | Roll: ");
      Serial.print(roll, 1);
      Serial.print("°");
      
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println(" [SENT]");
      } else {
        Serial.println(" [WiFi OFF]");
      }
      
      lastCommand = currentCommand;
    }
    
    // Try to send if WiFi connected
    if (WiFi.status() == WL_CONNECTED) {
      sendCommand(currentCommand);
    }
    
    lastSendTime = millis();
  }
  
  delay(50);
}

// ---------------- Get Gesture Command ----------------
String getGestureCommand(float pitch, float roll) {
  // Diagonal movements
  if (pitch > pitchThreshold && roll < -rollThreshold) {
    return "I";  // Forward Right
  } else if (pitch > pitchThreshold && roll > rollThreshold) {
    return "G";  // Forward Left
  } else if (pitch < -pitchThreshold && roll < -rollThreshold) {
    return "J";  // Backward Right
  } else if (pitch < -pitchThreshold && roll > rollThreshold) {
    return "H";  // Backward Left
  }
  // Cardinal directions
  else if (pitch > pitchThreshold) {
    return "F";  // Forward
  } else if (pitch < -pitchThreshold) {
    return "B";  // Backward
  } else if (roll < -rollThreshold) {
    return "R";  // Right
  } else if (roll > rollThreshold) {
    return "L";  // Left
  } else {
    return "S";  // Stop
  }
}

// ---------------- Send Command ----------------
void sendCommand(String command) {
  HTTPClient http;
  String url = "http://" + String(serverIP) + "/?State=" + command;
  
  http.begin(url);
  http.setTimeout(500);  // Short timeout
  int httpResponseCode = http.GET();
  
  if (httpResponseCode < 0) {
    Serial.print("HTTP Error: ");
    Serial.println(httpResponseCode);
  }
  
  http.end();
}
