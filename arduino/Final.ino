#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>
#include <ArduinoJson.h>

// --- Wi-Fi Configuration ---
// Replace with your Wi-Fi credentials
const char* ssid = "PLDTHOMEFIBRgky9c";
const char* password = "PLDTWIFIry2fp";

// --- Firebase Configuration ---
// Replace with your Firebase URL and authentication key
const String FIREBASE_URL = "https://ecgdata-f042a-default-rtdb.asia-southeast1.firebasedatabase.app/ecg_readings.json";
const String FIREBASE_AUTH = "AIzaSyA0OGrnWnNx0LDPGzDZHdrzajiRGEjr3AM";

// --- Time Configuration ---
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 0;

// --- Pin Definitions ---
#define AD8232_OUTPUT 35  // Analog pin connected to AD8232 output

// --- ECG Processing Constants ---
// ADJUSTMENT GUIDE:
// SAMPLING_RATE: Higher values give better resolution but require more processing
// BUFFER_SIZE: Larger buffers give better filtering but increase latency
// NYQUIST: Half of sampling rate, used for frequency calculations
const int SAMPLING_RATE = 200;  // Hz - Typical ECG sampling rate
const int BUFFER_SIZE = 32;     // Number of samples for filtering
const float NYQUIST = SAMPLING_RATE / 2.0;

// Peak Detection Parameters
// ADJUSTMENT GUIDE:
// MATERNAL_MIN_DISTANCE: Controls maximum maternal heart rate (lower = higher max BPM)
// FETAL_MIN_DISTANCE: Controls maximum fetal heart rate (lower = higher max BPM)
// MATERNAL_PROMINENCE: Higher values = more prominent peaks needed (0.0 to 1.0)
// FETAL_PROMINENCE: Lower values = more sensitive to small peaks (0.0 to 1.0)
// Peak Detection Parameters - ADJUSTED
const int MATERNAL_MIN_DISTANCE = (SAMPLING_RATE * 0.6);  // Keep same
const int FETAL_MIN_DISTANCE = (SAMPLING_RATE * 0.4);     // CHANGED: Increased for ~150 BPM max
const float MATERNAL_PROMINENCE = 0.5;    // Keep same
const float FETAL_PROMINENCE = 0.15;      // CHANGED: Lowered for better sensitivity

// Circular buffer for filtering
float ecgBuffer[BUFFER_SIZE];
int bufferIndex = 0;

// Peak detection variables
unsigned long lastMaternalPeak = 0;
unsigned long lastFetalPeak = 0;
bool maternalPeakDetected = false;
bool fetalPeakDetected = false;
int maternalBpm = 0;
int fetalBpm = 0;

// Device information
String deviceId = "esp32_ecg_device";
unsigned long readingCounter = 0;

void setup() {
  Serial.begin(115200);

  // Initialize buffer with zeros
  for(int i = 0; i < BUFFER_SIZE; i++) {
    ecgBuffer[i] = 0;
  }

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ Connected to Wi-Fi");
  Serial.println("IP Address: " + WiFi.localIP().toString());

  // Sync time with NTP server
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("❌ Failed to get time");
  } else {
    Serial.println("✅ Time synchronized with NTP");
    char timeStringBuff[50];
    strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%d %H:%M:%S", &timeinfo);
    Serial.print("Current time: ");
    Serial.println(timeStringBuff);
  }
}

/**
 * Bandpass filter implementation
 * ADJUSTMENT GUIDE:
 * - For fetal filter (isFetalFilter = true):
 *   - weight = 0.03: Higher values = more sensitive to high frequencies
 *   - Adjust if fetal peaks are too weak or noisy
 * - For maternal filter (isFetalFilter = false):
 *   - weight = 0.05: Higher values = more sensitive to low frequencies
 *   - Adjust if maternal peaks are too strong
 */
float bandpassFilter(int rawEcg, bool isFetalFilter) {
  // Update circular buffer
  ecgBuffer[bufferIndex] = rawEcg;
  bufferIndex = (bufferIndex + 1) % BUFFER_SIZE;
  
  float filtered = 0;
  
  // Approximate Butterworth filter using weighted moving average
  for (int i = 0; i < BUFFER_SIZE; i++) {
    float weight;
    if (isFetalFilter) {
      // ADJUSTED fetal filter weights for better sensitivity
      weight = 0.04 * (1 - abs(i - BUFFER_SIZE/2.0)/(BUFFER_SIZE/2.0));  // CHANGED: Increased from 0.03
    } else {
      // Maternal: 5-15 Hz bandpass approximation
      weight = 0.05 * (1 - abs(i - BUFFER_SIZE/2.0)/(BUFFER_SIZE/2.0));
    }
    filtered += ecgBuffer[(bufferIndex - i + BUFFER_SIZE) % BUFFER_SIZE] * weight;
  }
  
  return filtered;
}

/**
 * Peak detection algorithm
 * ADJUSTMENT GUIDE:
 * - If missing peaks:
 *   - Decrease prominence (0.2 to 0.1)
 *   - Decrease min_distance
 * - If detecting false peaks:
 *   - Increase prominence (0.2 to 0.3)
 *   - Increase min_distance
 * - If BPM is too low:
 *   - Decrease min_distance
 * - If BPM is too high:
 *   - Increase min_distance
 */
void detectPeaks(float filteredEcg, bool isFetal) {
  static float maxValue = 0;
  static float minValue = 4095;
  unsigned long currentTime = millis();
  
  // Update min/max for adaptive thresholding
  if (filteredEcg > maxValue) maxValue = filteredEcg;
  if (filteredEcg < minValue) minValue = filteredEcg;
  
  float prominence = isFetal ? FETAL_PROMINENCE : MATERNAL_PROMINENCE;
  float threshold = minValue + (maxValue - minValue) * prominence;
  
  if (isFetal) {
    // Fetal peak detection
    if (filteredEcg > threshold && !fetalPeakDetected &&
        (currentTime - lastFetalPeak) >= FETAL_MIN_DISTANCE &&
        (currentTime - lastMaternalPeak) >= (SAMPLING_RATE * 0.1)) {  // 100ms from maternal
      
      fetalPeakDetected = true;
      if (lastFetalPeak != 0) {
        unsigned long interval = currentTime - lastFetalPeak;
        fetalBpm = 60000 / interval;
        
        // Sanity check - ignore unreasonable values
        if (fetalBpm < 50 || fetalBpm > 200) {
          fetalBpm = 0; // Invalid reading
        }
      }
      lastFetalPeak = currentTime;
    } else if (filteredEcg < threshold * 0.8) {
      fetalPeakDetected = false;
    }
  } else {
    // Maternal peak detection
    if (filteredEcg > threshold && !maternalPeakDetected &&
        (currentTime - lastMaternalPeak) >= MATERNAL_MIN_DISTANCE) {
      
      maternalPeakDetected = true;
      if (lastMaternalPeak != 0) {
        unsigned long interval = currentTime - lastMaternalPeak;
        maternalBpm = 60000 / interval;
        
        // Sanity check - ignore unreasonable values
        if (maternalBpm < 40 || maternalBpm > 180) {
          maternalBpm = 0; // Invalid reading
        }
      }
      lastMaternalPeak = currentTime;
    } else if (filteredEcg < threshold * 0.8) {
      maternalPeakDetected = false;
    }
  }
  
  // Decay max/min values slowly for adaptive threshold
  // ADJUSTMENT GUIDE:
  // - If threshold adapts too slowly: Increase decay rates (0.995 to 0.99)
  // - If threshold adapts too quickly: Decrease decay rates (0.995 to 0.999)
  // ADJUSTED threshold decay rates for faster adaptation
  if (isFetal) {
    maxValue *= 0.99;    // CHANGED: Faster decay
    minValue *= 1.01;    // CHANGED: Faster rise
  } else {
    maxValue *= 0.995;
    minValue *= 1.005;
  }
}

// Get current timestamp in ISO 8601 format for Firebase
String getISOTimestamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return "0000-00-00T00:00:00Z"; // Return default on failure
  }
  
  char timeStringBuff[30];
  strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
  return String(timeStringBuff);
}

// Send data to Firebase with proper structure
bool sendToFirebase(int rawEcg, float fetalFiltered, float maternalFiltered) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ Wi-Fi disconnected. Trying to reconnect...");
    reconnectWiFi();
    return false;
  }
  
  // Generate a unique ID for this reading
  readingCounter++;
  String readingId = deviceId + "_" + String(readingCounter);
  String timestamp = getISOTimestamp();
  
  // Create the JSON payload with proper Firebase structure
  DynamicJsonDocument doc(1024);
  
  // Add data to JSON document
  doc["deviceId"] = deviceId;
  doc["timestamp"] = timestamp;
  doc["rawEcg"] = rawEcg;
  doc["fetalEcg"] = (int)fetalFiltered;
  doc["maternalEcg"] = (int)maternalFiltered;
  doc["fetalBpm"] = fetalBpm;
  doc["maternalBpm"] = maternalBpm;
  doc["readingId"] = readingId;
  
  // Serialize JSON to string
  String jsonOutput;
  serializeJson(doc, jsonOutput);
  
  // Construct Firebase path with timestamp-based node
  // Using a timestamp-prefixed structure for better data organization
  String firebasePath = FIREBASE_URL + "/" + readingId + ".json";
  if (FIREBASE_AUTH.length() > 0) {
    firebasePath += "?auth=" + FIREBASE_AUTH;
  }
  
  // Send to Firebase
  HTTPClient http;
  http.begin(firebasePath);
  http.addHeader("Content-Type", "application/json");
  
  int httpResponseCode = http.PUT(jsonOutput);
  
  if (httpResponseCode >= 200 && httpResponseCode < 300) {
    Serial.println("✅ Data sent to Firebase successfully!");
    Serial.println("Fetal BPM: " + String(fetalBpm) + ", Maternal BPM: " + String(maternalBpm));
    http.end();
    return true;
  } else {
    Serial.print("❌ Firebase error: ");
    Serial.println(httpResponseCode);
    Serial.println("Response: " + http.getString());
    http.end();
    return false;
  }
}

void loop() {
  // --- Read ECG ---
  int rawEcg = analogRead(AD8232_OUTPUT);
  
  // Check for lead-off condition (ADC value = 4095)
  if (rawEcg >= 4095) {
    Serial.println("Lead-off detected");
    delay(100);
    return;
  }
  
  // --- Apply filters and detect peaks ---
  float maternalFiltered = bandpassFilter(rawEcg, false);
  float fetalFiltered = bandpassFilter(rawEcg, true);
  
  detectPeaks(maternalFiltered, false);  // Detect maternal peaks first
  detectPeaks(fetalFiltered, true);      // Then detect fetal peaks
  
  // --- Get Timestamp ---
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  char timestamp[30];
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", timeinfo);

  // --- Create JSON Payload in the exact format requested ---
  String json = "{";
  json += "\"deviceId\":\"esp32\",";
  json += "\"bpm\":" + String(fetalBpm) + ",";  // Using fetal BPM as main BPM
  json += "\"timestamp\":\"" + String(timestamp) + "\",";
  json += "\"rawEcg\":" + String(rawEcg) + ",";
  json += "\"smoothedEcg\":" + String((int)fetalFiltered);  // Using fetal filtered signal
  json += "}";

  // --- Send to Firebase ---
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(FIREBASE_URL + "?auth=" + FIREBASE_AUTH);
    http.addHeader("Content-Type", "application/json");

    int httpResponseCode = http.PUT(json);

    if (httpResponseCode > 0) {
      Serial.println("✅ Data sent successfully!");
      Serial.println("JSON payload: " + json);
      Serial.println("BPM: " + String(fetalBpm));
    } else {
      Serial.print("❌ Error sending data: ");
      Serial.println(httpResponseCode);
    }

    http.end();
  } else {
    Serial.println("❌ Wi-Fi disconnected. Trying to reconnect...");
    reconnectWiFi();
  }

  delay(5000); // Wait 5 seconds between readings
}

// --- Wi-Fi Reconnect Helper ---
void reconnectWiFi() {
  WiFi.disconnect();
  WiFi.reconnect();
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ Reconnected to Wi-Fi!");
  } else {
    Serial.println("\n❌ Failed to reconnect. Will try again later.");
  }
} 