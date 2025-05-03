#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>

// Firebase settings
const String FIREBASE_URL = "https://ecgdata-f042a-default-rtdb.asia-southeast1.firebasedatabase.app/ecg_readings.json";
const String FIREBASE_AUTH = "AIzaSyA0OGrnWnNx0LDPGzDZHdrzajiRGEjr3AM";

// NTP time setup
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 0;

// WiFi credentials
char* WIFI_SSID = "PLDTHOMEFIBRgky9c";
char* WIFI_PASSWORD = "PLDTWIFIry2fp";

// --- ECG Setup ---
#define AD8232_OUTPUT 35
const int SAMPLING_RATE = 200;
const int BUFFER_SIZE = 32;
float ecgBuffer[BUFFER_SIZE];
int bufferIndex = 0;

int sensitivityLevel = 5;
unsigned long lastFetalPeak = 0;
bool fetalPeakDetected = false;
int fetalBpm = 0;
float smoothedEcg = 0;

// --- Functions for sensitivity ---
float getProminenceForSensitivity() {
  switch (sensitivityLevel) {
    case 1: return 0.15 * 3.0;
    case 2: return 0.15 * 2.0;
    case 3: return 0.15 * 1.5;
    case 4: return 0.15 * 1.25;
    case 5: return 0.15;
    case 6: return 0.15 * 0.85;
    case 7: return 0.15 * 0.7;
    case 8: return 0.15 * 0.55;
    case 9: return 0.15 * 0.4;
    case 10: return 0.15 * 0.25;
    default: return 0.15;
  }
}
int getMinDistanceForSensitivity() {
  float factor;
  switch (sensitivityLevel) {
    case 1: factor = 0.3 * 1.5; break;
    case 2: factor = 0.3 * 1.35; break;
    case 3: factor = 0.3 * 1.2; break;
    case 4: factor = 0.3 * 1.1; break;
    case 5: factor = 0.3; break;
    case 6: factor = 0.3 * 0.9; break;
    case 7: factor = 0.3 * 0.8; break;
    case 8: factor = 0.3 * 0.7; break;
    case 9: factor = 0.3 * 0.6; break;
    case 10: factor = 0.3 * 0.5; break;
    default: factor = 0.3;
  }
  return SAMPLING_RATE * factor;
}
float getDecayRateForSensitivity() {
  switch (sensitivityLevel) {
    case 1: return 0.995;
    case 2: return 0.994;
    case 3: return 0.993;
    case 4: return 0.992;
    case 5: return 0.99;
    case 6: return 0.988;
    case 7: return 0.986;
    case 8: return 0.984;
    case 9: return 0.982;
    case 10: return 0.98;
    default: return 0.99;
  }
}

// --- ECG Filter and Detection ---
float fetalBandpassFilter(int rawEcg) {
  ecgBuffer[bufferIndex] = rawEcg;
  bufferIndex = (bufferIndex + 1) % BUFFER_SIZE;

  float filtered = 0;
  for (int i = 0; i < BUFFER_SIZE; i++) {
    float weight = 0.04 * (1 - abs(i - BUFFER_SIZE/2.0)/(BUFFER_SIZE/2.0));
    filtered += ecgBuffer[(bufferIndex - i + BUFFER_SIZE) % BUFFER_SIZE] * weight;
  }
  return filtered;
}

void detectFetalPeak(float filteredEcg) {
  static float maxValue = 0;
  static float minValue = 4095;
  unsigned long currentTime = millis();

  if (filteredEcg > maxValue) maxValue = filteredEcg;
  if (filteredEcg < minValue) minValue = filteredEcg;

  float prominence = getProminenceForSensitivity();
  int minDistance = getMinDistanceForSensitivity();
  float decayRate = getDecayRateForSensitivity();

  float threshold = minValue + (maxValue - minValue) * prominence;

  if (filteredEcg > threshold && !fetalPeakDetected && (currentTime - lastFetalPeak) >= minDistance) {
    fetalPeakDetected = true;
    if (lastFetalPeak != 0) {
      unsigned long interval = currentTime - lastFetalPeak;
      fetalBpm = 60000 / interval;
      Serial.printf("Fetal Heart Rate: %d BPM\n", fetalBpm);
    }
    lastFetalPeak = currentTime;
  } else if (filteredEcg < threshold * 0.8) {
    fetalPeakDetected = false;
  }

  maxValue *= decayRate;
  minValue *= (2.0 - decayRate);
}

// --- Setup ---
void setup() {
  Serial.begin(115200);
  for (int i = 0; i < BUFFER_SIZE; i++) ecgBuffer[i] = 0;

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\n✅ Connected to Wi-Fi");

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) Serial.println("❌ Failed to get time");
}

// --- Loop ---
void loop() {
  int rawEcg = analogRead(AD8232_OUTPUT);
  if (rawEcg >= 4095) {
    Serial.println("Lead-off detected");
    delay(1000 / SAMPLING_RATE);
    return;
  }

  float filtered = fetalBandpassFilter(rawEcg);
  detectFetalPeak(filtered);
  smoothedEcg = filtered;

  // Get timestamp
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  char timestamp[30];
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", timeinfo);

  // Construct JSON
  String json = "{";
  json += "\"deviceId\":\"esp32\",";
  json += "\"bpm\":" + String(fetalBpm) + ",";
  json += "\"timestamp\":\"" + String(timestamp) + "\",";
  json += "\"rawEcg\":" + String(rawEcg) + ",";
  json += "\"smoothedEcg\":" + String((int)smoothedEcg);
  json += "}";

  // Send to Firebase
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(FIREBASE_URL + "?auth=" + FIREBASE_AUTH);
    http.addHeader("Content-Type", "application/json");

    int httpResponseCode = http.PUT(json);
    if (httpResponseCode > 0) {
      Serial.println("✅ Data sent: " + json);
      Serial.println("Response: " + http.getString());
    } else {
      Serial.print("❌ Error sending data: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  }

  delay(5000);
}
