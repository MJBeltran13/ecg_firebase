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

char* WIFI_SSID = "PLDTHOMEFIBRgky9c"; char* WIFI_PASSWORD = "PLDTWIFIry2fp";

void setup() {
  Serial.begin(115200);

  // Connect to Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ Connected to Wi-Fi");

  // Sync time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("❌ Failed to get time");
  }
}

void loop() {
  // Dummy data
  int bpm = random(70, 90);
  int rawEcg = random(500, 550);
  int smoothedEcg = random(20, 40);

  // Get timestamp
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  char timestamp[30];
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", timeinfo);

  // Construct JSON
  String json = "{";
  json += "\"deviceId\":\"esp32\",";
  json += "\"bpm\":" + String(bpm) + ",";
  json += "\"timestamp\":\"" + String(timestamp) + "\",";
  json += "\"rawEcg\":" + String(rawEcg) + ",";
  json += "\"smoothedEcg\":" + String(smoothedEcg);
  json += "}";

  // Send data using HTTP PUT
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(FIREBASE_URL + "?auth=" + FIREBASE_AUTH); // Add auth token to URL
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