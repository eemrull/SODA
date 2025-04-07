#include <WiFi.h>
#include <HTTPClient.h>

// WiFi Credentials
const char* ssid     = "LouraBuoy";
const char* password = "HQAZP2612";

// API Endpoint & Key
const char* serverUrl = "http://192.168.1.106:2612/loura-marine";
const char* apiKey    = "azp-261211102024-aquadrox";

// Setup UART2 for communication with STM32
HardwareSerial stmSerial(2);
#define RX_PIN 19  
#define TX_PIN 18

void setup() {
  Serial.begin(115200);
  stmSerial.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN); // Configure UART2 for STM32

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  // Variables for all sensor readings:
  float heading         = 0;
  float roll            = 0;
  float pitch           = 0;
  float imu_temperature = 0;  // Box temperature from BNO055
  float wave_height     = 0;
  float accelX          = 0;
  float accelY          = 0;
  float accelZ          = 0;
  float water_temperature = 0;
  
  // Check if sensor data is available from STM32
  if (stmSerial.available()) {
    String sensorData = stmSerial.readStringUntil('\n');
    sensorData.trim();
    Serial.println("Received sensor data: " + sensorData);
    
    // Expected CSV format:
    // heading,roll,pitch,imu_temperature,wave_height,accelX,accelY,accelZ,water_temperature
    int idx1 = sensorData.indexOf(',');
    int idx2 = sensorData.indexOf(',', idx1 + 1);
    int idx3 = sensorData.indexOf(',', idx2 + 1);
    int idx4 = sensorData.indexOf(',', idx3 + 1);
    int idx5 = sensorData.indexOf(',', idx4 + 1);
    int idx6 = sensorData.indexOf(',', idx5 + 1);
    int idx7 = sensorData.indexOf(',', idx6 + 1);
    int idx8 = sensorData.indexOf(',', idx7 + 1);
    
    if (idx1 == -1 || idx2 == -1 || idx3 == -1 || idx4 == -1 || 
        idx5 == -1 || idx6 == -1 || idx7 == -1 || idx8 == -1) {
      Serial.println("Error parsing sensor data.");
    } else {
      heading         = sensorData.substring(0, idx1).toFloat();
      roll            = sensorData.substring(idx1 + 1, idx2).toFloat();
      pitch           = sensorData.substring(idx2 + 1, idx3).toFloat();
      imu_temperature = sensorData.substring(idx3 + 1, idx4).toFloat();
      wave_height     = sensorData.substring(idx4 + 1, idx5).toFloat();
      accelX          = sensorData.substring(idx5 + 1, idx6).toFloat();
      accelY          = sensorData.substring(idx6 + 1, idx7).toFloat();
      accelZ          = sensorData.substring(idx7 + 1, idx8).toFloat();
      water_temperature = sensorData.substring(idx8 + 1).toFloat();
    }
  }
  
  // Sensor Data String
  String sensorDataStr = "{'heading': " + String(heading, 2) +
                         ", 'roll': " + String(roll, 2) +
                         ", 'pitch': " + String(pitch, 2) +
                         ", 'box_temp': " + String(imu_temperature, 2) +
                         ", 'wave_height': " + String(wave_height, 2) +
                         ", 'accel_x': " + String(accelX, 2) +
                         ", 'accel_y': " + String(accelY, 2) +
                         ", 'accel_z': " + String(accelZ, 2) +
                         ", 'water_temp': " + String(water_temperature, 2) +
                         "}";
  
  // JSON Payload
  String jsonPayload = "{\"project\":\"smartbuoy\", \"data\": \"" + sensorDataStr + "\"}";
  
  Serial.println("Sending data to API: " + jsonPayload);
  
  // Send HTTP POST request if WiFi is connected
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-API-KEY", apiKey);
    
    int httpResponseCode = http.POST(jsonPayload);
    
    if (httpResponseCode > 0) {
      Serial.print("Server Response Code: ");
      Serial.println(httpResponseCode);
      Serial.print("Response: ");
      Serial.println(http.getString());
    } else {
      Serial.print("Error on sending POST: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  } else {
    Serial.println("WiFi not connected!");
  }
  
  delay(3000); // Send data every 3 seconds
}
