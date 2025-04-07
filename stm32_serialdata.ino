#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <utility/imumaths.h>

// -------- UART Setup --------
#define RX_PIN PA10
#define TX_PIN PA9
HardwareSerial Serial1(RX_PIN, TX_PIN);

// -------- BNO055 Setup --------
Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28, &Wire);

// -------- DS18B20 Setup --------
#define ONE_WIRE_BUS PC7
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature waterTempSensors(&oneWire);

// -------- Global Variables --------
float water_temperature = 0.0;
float imu_temperature = 0.0;
float wave_height = 0.0;

float z_max = 0.0;
float z_min = 0.0;
float z_baseline = 0.0;

// -------- Timing Variables --------
unsigned long lastWaveCalc = 0;
unsigned long lastSendTime = 0;
unsigned long lastBaselineCalib = 0;
unsigned long lastTempTime = 0;

const unsigned long measurementInterval = 3000;  // 3 seconds
const unsigned long baselineInterval = 300000;   // 5 minutes
const unsigned long tempInterval = 1000;         // 1 second
const unsigned long sendInterval = 3000;         // 3 seconds

// -------- Baseline Calibration --------
void calibrateBaseline() {
  Serial.println(F("Calibrating baseline..."));
  float sum = 0.0;
  const int samples = 50;
  for (int i = 0; i < samples; i++) {
    sensors_event_t tempEvent;
    bno.getEvent(&tempEvent, Adafruit_BNO055::VECTOR_LINEARACCEL);
    sum += tempEvent.acceleration.z;
    delay(20);  // Reduced delay for faster sampling
  }
  z_baseline = sum / samples;
  Serial.print(F("New baseline (z): "));
  Serial.println(z_baseline);
}

// -------- IMU Update --------
void updateIMU() {
  sensors_event_t linearAccelData;
  bno.getEvent(&linearAccelData, Adafruit_BNO055::VECTOR_LINEARACCEL);
  float az = linearAccelData.acceleration.z - z_baseline;

  // Update peaks
  if (az > z_max) z_max = az;
  if (az < z_min) z_min = az;

  // Update IMU temperature
  imu_temperature = bno.getTemp();
}

// -------- Calculate Wave Height --------
void calculateWaveHeight() {
  float measuredDelta = z_max - z_min;
  float calibrationFactor = 0.5 / 10.7;  // Adjust based on your calibration
  wave_height = measuredDelta * calibrationFactor;

  // Reset peaks with current reading to prevent spikes
  sensors_event_t currentEvent;
  bno.getEvent(&currentEvent, Adafruit_BNO055::VECTOR_LINEARACCEL);
  float currentZ = currentEvent.acceleration.z - z_baseline;
  z_max = z_min = currentZ;

  Serial.print(F("Wave Height: "));
  Serial.print(wave_height);
  Serial.println(F(" meters"));
}

// -------- Water Temperature Update --------
void updateWaterTemp() {
  waterTempSensors.requestTemperatures();
  water_temperature = waterTempSensors.getTempCByIndex(0);
}

// -------- Send Data --------
void sendData() {
  sensors_event_t orientation, accel;
  bno.getEvent(&orientation);
  bno.getEvent(&accel, Adafruit_BNO055::VECTOR_ACCELEROMETER);

  String data = String(orientation.orientation.x, 2) + "," +
                String(orientation.orientation.y, 2) + "," +
                String(orientation.orientation.z, 2) + "," +
                String(imu_temperature, 2) + "," +
                String(wave_height, 2) + "," +
                String(accel.acceleration.x, 2) + "," +
                String(accel.acceleration.y, 2) + "," +
                String(accel.acceleration.z, 2) + "," +
                String(water_temperature, 2);

  Serial1.println(data);
  Serial.print("Sent: ");
  Serial.println(data);
}

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200);
  Wire.begin();
  waterTempSensors.begin();

  if (!bno.begin()) {
    Serial.println("BNO055 not detected!");
    while (1);
  }
  calibrateBaseline();

  // Initialize peaks with first reading
  sensors_event_t initialEvent;
  bno.getEvent(&initialEvent, Adafruit_BNO055::VECTOR_LINEARACCEL);
  float initialZ = initialEvent.acceleration.z - z_baseline;
  z_max = z_min = initialZ;
}

void loop() {
  unsigned long currentMillis = millis();

  // Continuously update IMU readings
  updateIMU();

  // Update temperature every second
  if (currentMillis - lastTempTime >= tempInterval) {
    updateWaterTemp();
    lastTempTime = currentMillis;
  }

  // Calculate wave height every 3 seconds
  if (currentMillis - lastWaveCalc >= measurementInterval) {
    calculateWaveHeight();
    lastWaveCalc = currentMillis;
  }

  // Send data every 3 seconds
  if (currentMillis - lastSendTime >= sendInterval) {
    sendData();
    lastSendTime = currentMillis;
  }

  // Periodic baseline calibration
  if (currentMillis - lastBaselineCalib >= baselineInterval) {
    if (fabs(z_max - z_min) < 0.2) {
      calibrateBaseline();
      // Reinitialize peaks after calibration
      sensors_event_t postCalibEvent;
      bno.getEvent(&postCalibEvent, Adafruit_BNO055::VECTOR_LINEARACCEL);
      float postCalibZ = postCalibEvent.acceleration.z - z_baseline;
      z_max = z_min = postCalibZ;
    }
    lastBaselineCalib = currentMillis;
  }
}