#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <utility/imumaths.h>
#include <math.h>

// -------- UART Setup --------
#define RX_PIN PA10
#define TX_PIN PA9
HardwareSerial Serial1(RX_PIN, TX_PIN);

// -------- BNO055 Setup --------
Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28, &Wire);

// -------- DS18B20 Setup (non-blocking) --------
#define ONE_WIRE_BUS PC7
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature waterTempSensors(&oneWire);

// -------- Globals --------
float water_temperature = 0.0;
float imu_temperature   = 0.0;
float wave_height       = 0.0;
float z_max = 0.0, z_min = 0.0, z_baseline = 0.0, z_filt = 0.0;

#define safe(x) (isnan(x) ? 0.0 : (x))

// -------- Timing --------
unsigned long lastWaveCalc    = 0;
unsigned long lastSendTime    = 0;
unsigned long lastBaselineCal = 0;
unsigned long lastTempTime    = 0;

const unsigned long measurementInterval = 3000;
const unsigned long baselineInterval    = 300000;
const unsigned long tempInterval        = 1000;
const unsigned long sendInterval        = 3000;

// -------- Baseline Calibration --------
void calibrateBaseline() {
  float sum = 0;
  for (int i = 0; i < 50; i++) {
    sensors_event_t e;
    bno.getEvent(&e, Adafruit_BNO055::VECTOR_LINEARACCEL);
    sum += e.acceleration.z;
    delay(20);
  }
  z_baseline = sum / 50.0;
}

// -------- IMU Update --------
void updateIMU() {
  sensors_event_t e;
  bno.getEvent(&e, Adafruit_BNO055::VECTOR_LINEARACCEL);
  float raw = e.acceleration.z - z_baseline;
  z_filt = 0.1f * raw + 0.9f * z_filt;
  z_max = max(z_max, z_filt);
  z_min = min(z_min, z_filt);
  imu_temperature = bno.getTemp();
}

// -------- Wave Calc --------
void calculateWaveHeight() {
  wave_height = (z_max - z_min) * (0.5f / 10.7f);
  z_max = z_min = z_filt;
}

// -------- Water Temp (non-blocking) --------
void updateWaterTemp() {
  if (waterTempSensors.requestTemperaturesByIndex(0)) {
    float t = waterTempSensors.getTempCByIndex(0);
    if (!isnan(t)) water_temperature = t;
  }
}

// -------- Send Data (no CRC) --------
void sendData() {
  sensors_event_t ori, acc;
  bno.getEvent(&ori, Adafruit_BNO055::VECTOR_EULER);
  bno.getEvent(&acc, Adafruit_BNO055::VECTOR_ACCELEROMETER);

  float vals[9] = {
    ori.orientation.x,
    ori.orientation.y,
    ori.orientation.z,
    imu_temperature,
    wave_height,
    acc.acceleration.x,
    acc.acceleration.y,
    acc.acceleration.z,
    water_temperature
  };

  char buf[96];
  int len = snprintf(buf, sizeof(buf), "#");

  for (int i = 0; i < 9; i++) {
    float v = safe(vals[i]);
    int ip = int(v);
    int dp = abs(int((v - ip) * 100));
    len += snprintf(buf + len, sizeof(buf) - len, "%d.%02d,", ip, dp);
  }

  if (len && buf[len - 1] == ',') {
    buf[--len] = '\0';
  }

  if (len < sizeof(buf) - 2) {  // leave space for \n and null-terminator
    buf[len++] = '\n';
    buf[len] = '\0';
    Serial1.write((uint8_t*)buf, len);
    Serial.print("Sent: ");
    Serial.println(buf);
  } else {
    Serial.println("Payload error");
  }
}

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200);
  Wire.begin();
  waterTempSensors.begin();

  if (!bno.begin()) {
    Serial.println("BNO055 not found!");
    while (1);
  }
  delay(1000);
  calibrateBaseline();

  sensors_event_t ie;
  bno.getEvent(&ie, Adafruit_BNO055::VECTOR_LINEARACCEL);
  z_filt = ie.acceleration.z - z_baseline;
  z_max = z_min = z_filt;
}

void loop() {
  unsigned long now = millis();

  updateIMU();

  if (now - lastTempTime >= tempInterval) {
    updateWaterTemp();
    lastTempTime = now;
  }

  if (now - lastWaveCalc >= measurementInterval) {
    calculateWaveHeight();
    lastWaveCalc = now;
  }

  if (now - lastSendTime >= sendInterval) {
    sendData();
    lastSendTime = now;
  }

  if (now - lastBaselineCal >= baselineInterval) {
    if (fabs(z_max - z_min) < 0.2f) {
      calibrateBaseline();
      sensors_event_t e;
      bno.getEvent(&e, Adafruit_BNO055::VECTOR_LINEARACCEL);
      z_filt = e.acceleration.z - z_baseline;
      z_max = z_min = z_filt;
    }
    lastBaselineCal = now;
  }

  delay(1);
}
