#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>
#include <ArduinoJson.h>

// --- Wi-Fi Configuration ---
const char* ssid = "PLDTHOMEFIBRgky9c";
const char* password = "PLDTWIFIry2fp";

// --- Firebase Configuration ---
const String FIREBASE_URL = "https://ecgdata-f042a-default-rtdb.asia-southeast1.firebasedatabase.app/ecg_readings.json";
const String FIREBASE_AUTH = "AIzaSyA0OGrnWnNx0LDPGzDZHdrzajiRGEjr3AM";

// --- Time Configuration ---
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 0;

// --- Pin Definitions ---
#define AD8232_OUTPUT 35    // Analog pin connected to AD8232 output
#define LED_MATERNAL 32     // LED for maternal heartbeat
#define LED_FETAL 33        // LED for fetal heartbeat

// --- ECG Processing Constants ---
const int SAMPLING_RATE = 200;  // Hz - Typical ECG sampling rate
const int BUFFER_SIZE = 32;     // Number of samples for filtering
const float NYQUIST = SAMPLING_RATE / 2.0;

// --- MEDIUM SENSITIVITY Settings ---
// These settings provide a balanced approach to fetal ECG detection
// Appropriate for: Most standard monitoring cases, typical pregnancies
// Balances detection ability with false positive rejection
const float FETAL_PROMINENCE = 0.10;            // Medium threshold
const int FETAL_MIN_DISTANCE = SAMPLING_RATE * 0.4;  // ~150 BPM max
const float FETAL_WEIGHT = 0.05;                // Medium weight for high-frequency components

// Maternal signal parameters (fixed)
const int MATERNAL_MIN_DISTANCE = (SAMPLING_RATE * 0.6);  // ~100 BPM max
const float MATERNAL_PROMINENCE = 0.5;                   // Prominence threshold
const float MATERNAL_WEIGHT = 0.05;                      // Filter weight

// Circular buffers for filtering
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
String deviceId = "esp32_fetal_ecg_medium";
unsigned long readingCounter = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("Fetal ECG Monitor - MEDIUM SENSITIVITY Mode");
  
  // Initialize pins
  pinMode(AD8232_OUTPUT, INPUT);
  pinMode(LED_MATERNAL, OUTPUT);
  pinMode(LED_FETAL, OUTPUT);
  
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

void loop() {
  // Read raw ECG value
  int rawEcg = analogRead(AD8232_OUTPUT);
  
  // Apply bandpass filters for maternal and fetal signals
  float maternalFiltered = bandpassFilter(rawEcg, false);
  float fetalFiltered = bandpassFilter(rawEcg, true);
  
  // Detect peaks in filtered signals
  detectPeaks(maternalFiltered, false);  // Maternal
  detectPeaks(fetalFiltered, true);      // Fetal
  
  // Visualize heartbeats using LEDs
  if (millis() - lastMaternalPeak < 200) {
    digitalWrite(LED_MATERNAL, HIGH);
  } else {
    digitalWrite(LED_MATERNAL, LOW);
  }
  
  if (millis() - lastFetalPeak < 200) {
    digitalWrite(LED_FETAL, HIGH);
  } else {
    digitalWrite(LED_FETAL, LOW);
  }
  
  // Display BPM values on Serial monitor
  static unsigned long lastDisplayTime = 0;
  if (millis() - lastDisplayTime >= 1000) {
    lastDisplayTime = millis();
    Serial.print("Maternal BPM: ");
    Serial.print(maternalBpm);
    Serial.print(" | Fetal BPM: ");
    Serial.println(fetalBpm);
    
    // Send data to Firebase every second
    sendToFirebase(rawEcg, fetalFiltered, maternalFiltered);
  }
  
  // Maintain the sampling rate (200Hz = 5ms per sample)
  delay(5);
}

/**
 * Bandpass filter implementation for ECG signals
 * Uses a weighted moving average to approximate bandpass filtering
 */
float bandpassFilter(int rawEcg, bool isFetalFilter) {
  // Update circular buffer
  ecgBuffer[bufferIndex] = rawEcg;
  bufferIndex = (bufferIndex + 1) % BUFFER_SIZE;
  
  float filtered = 0;
  
  // Apply different filter weights based on signal type
  for (int i = 0; i < BUFFER_SIZE; i++) {
    float weight;
    if (isFetalFilter) {
      // Fetal filter: higher frequency components (15-40 Hz)
      weight = FETAL_WEIGHT * 
               (1 - abs(i - BUFFER_SIZE/2.0)/(BUFFER_SIZE/2.0));
    } else {
      // Maternal filter: lower frequency components (5-15 Hz)
      weight = MATERNAL_WEIGHT * 
               (1 - abs(i - BUFFER_SIZE/2.0)/(BUFFER_SIZE/2.0));
    }
    filtered += ecgBuffer[(bufferIndex - i + BUFFER_SIZE) % BUFFER_SIZE] * weight;
  }
  
  return filtered;
}

/**
 * Peak detection algorithm for both maternal and fetal signals
 * Uses adaptive thresholding based on signal amplitude
 */
void detectPeaks(float filteredEcg, bool isFetal) {
  static float maternalMaxValue = 0;
  static float maternalMinValue = 4095;
  static float fetalMaxValue = 0;
  static float fetalMinValue = 4095;
  
  unsigned long currentTime = millis();
  
  if (isFetal) {
    // Update min/max for adaptive thresholding (fetal)
    if (filteredEcg > fetalMaxValue) fetalMaxValue = filteredEcg;
    if (filteredEcg < fetalMinValue) fetalMinValue = filteredEcg;
    
    float threshold = fetalMinValue + (fetalMaxValue - fetalMinValue) * FETAL_PROMINENCE;
    
    // Fetal peak detection
    if (filteredEcg > threshold && !fetalPeakDetected &&
        (currentTime - lastFetalPeak) >= FETAL_MIN_DISTANCE &&
        (currentTime - lastMaternalPeak) >= (SAMPLING_RATE * 0.1)) {  // Avoid maternal artifacts
      
      fetalPeakDetected = true;
      if (lastFetalPeak != 0) {
        unsigned long interval = currentTime - lastFetalPeak;
        fetalBpm = 60000 / interval;
        
        // Sanity check - ignore unreasonable values
        if (fetalBpm < 100 || fetalBpm > 190) {
          fetalBpm = 0; // Invalid reading
        }
      }
      lastFetalPeak = currentTime;
    } else if (filteredEcg < threshold * 0.8) {
      fetalPeakDetected = false;
    }
    
    // Decay fetal max/min values for adaptive threshold
    fetalMaxValue *= 0.99; // Medium decay
    fetalMinValue *= 1.01; // Medium rise
  } else {
    // Update min/max for adaptive thresholding (maternal)
    if (filteredEcg > maternalMaxValue) maternalMaxValue = filteredEcg;
    if (filteredEcg < maternalMinValue) maternalMinValue = filteredEcg;
    
    float threshold = maternalMinValue + (maternalMaxValue - maternalMinValue) * MATERNAL_PROMINENCE;
    
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
    
    // Decay maternal max/min values for adaptive threshold
    maternalMaxValue *= 0.995;
    maternalMinValue *= 1.005;
  }
}

/**
 * Get current timestamp in ISO 8601 format for Firebase
 */
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

/**
 * Send ECG data to Firebase database
 */
bool sendToFirebase(int rawEcg, float fetalFiltered, float maternalFiltered) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ Wi-Fi disconnected. Trying to reconnect...");
    WiFi.begin(ssid, password);
    return false;
  }
  
  // Generate a unique ID for this reading
  readingCounter++;
  String readingId = deviceId + "_" + String(readingCounter);
  String timestamp = getISOTimestamp();
  
  // Create the JSON payload
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
  doc["sensitivityMode"] = "MEDIUM";
  
  // Serialize JSON to string
  String jsonOutput;
  serializeJson(doc, jsonOutput);
  
  // Send data to Firebase
  HTTPClient http;
  http.begin(FIREBASE_URL + "?auth=" + FIREBASE_AUTH);
  http.addHeader("Content-Type", "application/json");
  
  int httpResponseCode = http.PUT(jsonOutput);
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("✅ Data sent to Firebase");
    http.end();
    return true;
  } else {
    Serial.print("❌ Firebase Error: ");
    Serial.println(httpResponseCode);
    http.end();
    return false;
  }
} 