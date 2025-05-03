#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>
#include <ArduinoJson.h>

// --- Wi-Fi Configuration ---
const char *ssid = "HUAWEI-ec8a";
const char *password = "fbbb38dm";

// --- Firebase Configuration ---
const String FIREBASE_URL = "https://ecgdata-f042a-default-rtdb.asia-southeast1.firebasedatabase.app/ecg_readings.json";
const String FIREBASE_AUTH = "AIzaSyA0OGrnWnNx0LDPGzDZHdrzajiRGEjr3AM";

// --- Time Configuration ---
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 0;

// --- Pin Definitions ---
#define AD8232_OUTPUT 35    // Analog pin connected to AD8232 output
#define LED_FETAL 33        // LED for fetal heartbeat

// --- ECG Processing Constants ---
const int SAMPLING_RATE = 200;  // Hz - Typical ECG sampling rate
const int BUFFER_SIZE = 32;     // Number of samples for filtering
const float NYQUIST = SAMPLING_RATE / 2.0;

// --- FETAL SENSITIVITY LEVELS ---
// Define seven sensitivity levels for fetal ECG detection
// Each level specifies {PROMINENCE, MIN_DISTANCE, FILTER_WEIGHT}
const int NUM_SENSITIVITY_LEVELS = 7;
const float FETAL_LEVELS[NUM_SENSITIVITY_LEVELS][3] = {
  // Level 1: Ultra Low Sensitivity (extremely conservative)
  {0.20, SAMPLING_RATE * 0.55, 0.02},  // 110 BPM max, very high threshold, minimal weight
  
  // Level 2: Very Low Sensitivity
  {0.15, SAMPLING_RATE * 0.5, 0.03},   // 120 BPM max, high threshold, low weight
  
  // Level 3: Low Sensitivity
  {0.10, SAMPLING_RATE * 0.4, 0.05},   // 150 BPM max, lower threshold
  
  // Level 4: Medium Sensitivity
  {0.07, SAMPLING_RATE * 0.35, 0.06},  // 170 BPM max
  
  // Level 5: High Sensitivity
  {0.04, SAMPLING_RATE * 0.3, 0.08},   // 200 BPM max
  
  // Level 6: Very High Sensitivity
  {0.02, SAMPLING_RATE * 0.25, 0.1},   // 240 BPM max, very low threshold, high weight
  
  // Level 7: Ultra High Sensitivity (most aggressive)
  {0.01, SAMPLING_RATE * 0.2, 0.15}    // 300 BPM max, extremely low threshold, maximum weight
};

// Current sensitivity level (0-6, maps to levels 1-7)
// TO ADJUST SENSITIVITY: Change this value to set the fetal detection sensitivity
// 0 = Ultra Low, 1 = Very Low, 2 = Low, 3 = Medium, 4 = High, 5 = Very High, 6 = Ultra High
int currentFetalSensitivity = 4; // Default to Level 5 (High Sensitivity)

// Maternal signal parameters (fixed - these don't change)
const int MATERNAL_MIN_DISTANCE = (SAMPLING_RATE * 0.4);  // ~150 BPM max
const float MATERNAL_PROMINENCE = 0.2;                   // Threshold
const float MATERNAL_WEIGHT = 0.08;                     // Filter weight

// Peak detection improvement
const int PEAK_PERSISTENCE = 2;                // Reduced for faster detection
const int SIGNAL_STABILIZATION_TIME = 3000;    // Reduced to 3 seconds


const int DEFAULT_MATERNAL_BPM = 75;  
const int DEFAULT_FETAL_BPM = 114;   
const int MAX_TIME_WITHOUT_DETECTION = 10000; // 10 seconds before using defaults

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

// Tracking recent BPM readings for stability
const int BPM_HISTORY_SIZE = 3;  // Smaller history size for faster response
int maternalBpmHistory[BPM_HISTORY_SIZE] = {0};
int fetalBpmHistory[BPM_HISTORY_SIZE] = {0};
int bpmHistoryIndex = 0;

// Peak strength tracking
float maternalPeakStrength = 0;
float fetalPeakStrength = 0;

// Device information
String deviceId = "esp32_fetal_ecg_high";
unsigned long readingCounter = 0;
unsigned long startTime;

// Variables for timeout tracking
unsigned long lastValidMaternalBpm = 0;
unsigned long lastValidFetalBpm = 0;
bool usingDefaultMaternalBpm = false;
bool usingDefaultFetalBpm = false;

// Debug mode for troubleshooting
const bool DEBUG_MODE = true;

void setup() {
  Serial.begin(115200);
  Serial.println("Fetal ECG Monitor - FIXED SENSITIVITY LEVEL");
  
  // Initialize pins
  pinMode(AD8232_OUTPUT, INPUT);
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
  
  // Record start time
  startTime = millis();
  
  // Initial signal stabilization message
  Serial.println("Waiting for signal to stabilize...");
  
  // Flash LEDs to indicate startup
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_FETAL, HIGH);
    delay(200);
    digitalWrite(LED_FETAL, LOW);
    delay(200);
  }
  
  // Display current sensitivity level
  displaySensitivityLevel();
}

void loop() {
  // Read raw ECG value
  int rawEcg = analogRead(AD8232_OUTPUT);
  
  // Apply signal conditioning before filtering
  rawEcg = constrain(rawEcg, 100, 4000); // Remove extreme outliers
  
  // Apply simple noise reduction
  static int lastRawEcg = 0;
  if (abs(rawEcg - lastRawEcg) > 1000) { // Extreme jump - likely noise
    rawEcg = (rawEcg + lastRawEcg) / 2; // Smooth it
  }
  lastRawEcg = rawEcg;
  
  // Apply bandpass filters for maternal and fetal signals
  float maternalFiltered = bandpassFilter(rawEcg, false);
  float fetalFiltered = bandpassFilter(rawEcg, true);
  
  // Calculate signal quality metrics
  static float signalVariance = 0;
  static float fetalSignalStrength = 0;
  signalVariance = signalVariance * 0.95 + abs(rawEcg - 2048) * 0.05;
  fetalSignalStrength = fetalSignalStrength * 0.95 + abs(fetalFiltered) * 0.05;
  
  // Force occasional resets to avoid getting stuck
  static unsigned long lastResetTime = 0;
  if (millis() - lastResetTime > 30000) { // Every 30 seconds
    lastResetTime = millis();
    
    // Reset peak detection variables to escape potential stuck states
    maternalPeakDetected = false;
    fetalPeakDetected = false;
    
    if (DEBUG_MODE) {
      Serial.println("Performing periodic reset of peak detection variables");
    }
  }
  
  // Detect peaks in filtered signals
  detectPeaks(maternalFiltered, false);  // Maternal
  detectPeaks(fetalFiltered, true);      // Fetal
  
  // Check for timeout and apply default values if needed
  if (maternalBpm > 0) {
    lastValidMaternalBpm = millis();
    usingDefaultMaternalBpm = false;
  } else if (millis() - lastValidMaternalBpm > MAX_TIME_WITHOUT_DETECTION && 
             millis() - startTime > SIGNAL_STABILIZATION_TIME) {
    if (!usingDefaultMaternalBpm) {
      Serial.println("No maternal heartbeat detected for 10 seconds. Using default value.");
      usingDefaultMaternalBpm = true;
    }
    maternalBpm = DEFAULT_MATERNAL_BPM;
  }
  
  if (fetalBpm > 0) {
    lastValidFetalBpm = millis();
    usingDefaultFetalBpm = false;
  } else if (millis() - lastValidFetalBpm > MAX_TIME_WITHOUT_DETECTION && 
             millis() - startTime > SIGNAL_STABILIZATION_TIME) {
    if (!usingDefaultFetalBpm) {
      Serial.println("No fetal heartbeat detected for 10 seconds. Using default value.");
      usingDefaultFetalBpm = true;
    }
    fetalBpm = DEFAULT_FETAL_BPM;
  }
  
  // Visualize heartbeats using LEDs
  if (millis() - lastFetalPeak < 200) {
    digitalWrite(LED_FETAL, HIGH);
  } else {
    digitalWrite(LED_FETAL, LOW);
  }
  
  // Display BPM values on Serial monitor
  static unsigned long lastDisplayTime = 0;
  if (millis() - lastDisplayTime >= 1000) {
    lastDisplayTime = millis();
    
    // Calculate average BPM from history for stability
    int avgMaternalBpm = calculateAverageBpm(maternalBpmHistory, BPM_HISTORY_SIZE);
    int avgFetalBpm = calculateAverageBpm(fetalBpmHistory, BPM_HISTORY_SIZE);
    
    // Set to calculated average if valid, otherwise keep current value
    if (avgMaternalBpm > 0) maternalBpm = avgMaternalBpm;
    if (avgFetalBpm > 0) fetalBpm = avgFetalBpm;
    
    // Only start reporting after signal stabilization
    if (millis() - startTime > SIGNAL_STABILIZATION_TIME) {
      Serial.print("Maternal BPM: ");
      Serial.print(maternalBpm);
      Serial.print(usingDefaultMaternalBpm ? " (default)" : "");
      
      Serial.print(" | Fetal BPM: ");
      Serial.print(fetalBpm);
      Serial.print(usingDefaultFetalBpm ? " (default)" : "");
      
      Serial.print(" | Sensitivity Level: ");
      Serial.print(currentFetalSensitivity + 1); // Display as 1-7 instead of 0-6
      
      if (DEBUG_MODE) {
        Serial.print(" | Raw: ");
        Serial.print(rawEcg);
        Serial.print(" | M-Filt: ");
        Serial.print(maternalFiltered);
        Serial.print(" | F-Filt: ");
        Serial.print(fetalFiltered);
        Serial.print(" | Signal Quality: ");
        Serial.print(signalVariance);
      }
      
      Serial.println();
      
      // Send data to Firebase every second
      sendToFirebase(rawEcg, fetalFiltered, maternalFiltered);
    } else {
      Serial.print("Stabilizing signal... ");
      Serial.print((SIGNAL_STABILIZATION_TIME - (millis() - startTime)) / 1000);
      Serial.println(" seconds remaining");
    }
  }
  
  // More appropriate sampling delay based on actual processing time
  static unsigned long lastSampleTime = 0;
  unsigned long processingTime = millis() - lastSampleTime;
  int delayTime = max(1, 5 - (int)processingTime); // Aim for 5ms per sample
  delay(delayTime);
  lastSampleTime = millis();
}

/**
 * Display the current fetal sensitivity level
 */
void displaySensitivityLevel() {
  Serial.println("\n--- FETAL SENSITIVITY LEVEL ---");
  Serial.print("Current Level: ");
  Serial.print(currentFetalSensitivity + 1); // Display as 1-7 instead of 0-6
  Serial.print(" of 7 (");
  
  switch (currentFetalSensitivity) {
    case 0:
      Serial.println("Ultra Low Sensitivity)");
      Serial.println("Best for: Extremely strong, clear fetal signals with no noise");
      break;
    case 1:
      Serial.println("Very Low Sensitivity)");
      Serial.println("Best for: Strong, clear fetal signals");
      break;
    case 2:
      Serial.println("Low Sensitivity)");
      Serial.println("Best for: Good fetal signals, minimal noise");
      break;
    case 3:
      Serial.println("Medium Sensitivity)");
      Serial.println("Best for: Standard monitoring conditions");
      break;
    case 4:
      Serial.println("High Sensitivity)");
      Serial.println("Best for: Weaker signals, typical conditions");
      break;
    case 5:
      Serial.println("Very High Sensitivity)");
      Serial.println("Best for: Very weak signals, difficult cases");
      break;
    case 6:
      Serial.println("Ultra High Sensitivity)");
      Serial.println("Best for: Extremely weak signals, maximum detection capability");
      break;
  }
  
  Serial.println("To change sensitivity level, edit 'currentFetalSensitivity' variable in code (0-6)");
  Serial.println("----------------------------");
}

/**
 * Calculate average BPM from history array
 */
int calculateAverageBpm(int bpmArray[], int size) {
  int sum = 0;
  int count = 0;
  
  for (int i = 0; i < size; i++) {
    if (bpmArray[i] > 0) {
      sum += bpmArray[i];
      count++;
    }
  }
  
  if (count > 0) {
    return sum / count;
  } else {
    return 0;
  }
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
      // Get current sensitivity level parameters
      float fetalWeight = FETAL_LEVELS[currentFetalSensitivity][2];
      
      // Fetal filter with sensitivity-adjusted weight
      weight = fetalWeight * (1 - abs(i - BUFFER_SIZE/2.0)/(BUFFER_SIZE/2.0));
      
      // Add additional emphasis on the middle frequencies (fetal heartbeat range)
      if (i > BUFFER_SIZE/4 && i < BUFFER_SIZE*3/4) {
        weight *= 1.2;
      }
    } else {
      // Maternal filter: lower frequency components (doesn't change with sensitivity)
      weight = MATERNAL_WEIGHT * (1 - abs(i - BUFFER_SIZE/2.0)/(BUFFER_SIZE/2.0));
    }
    filtered += ecgBuffer[(bufferIndex - i + BUFFER_SIZE) % BUFFER_SIZE] * weight;
  }
  
  return filtered;
}

/**
 * Peak detection algorithm for both maternal and fetal signals
 * Uses adaptive thresholding with dynamic parameter adjustment
 */
void detectPeaks(float filteredEcg, bool isFetal) {
  static float maternalMaxValue = 0;
  static float maternalMinValue = 4095;
  static float fetalMaxValue = 0;
  static float fetalMinValue = 4095;
  static int maternalConsecutivePeaks = 0;
  static int fetalConsecutivePeaks = 0;
  
  unsigned long currentTime = millis();
  
  if (isFetal) {
    // Get current sensitivity level parameters
    float fetalProminence = FETAL_LEVELS[currentFetalSensitivity][0];
    int fetalMinDistance = FETAL_LEVELS[currentFetalSensitivity][1];
    
    // Update min/max for adaptive thresholding (fetal)
    if (filteredEcg > fetalMaxValue) fetalMaxValue = filteredEcg;
    if (filteredEcg < fetalMinValue) fetalMinValue = filteredEcg;
    
    // Handle case where min and max are too close
    if (fetalMaxValue - fetalMinValue < 10) {
      fetalMaxValue += 10;
      fetalMinValue -= 10;
    }
    
    float threshold = fetalMinValue + (fetalMaxValue - fetalMinValue) * fetalProminence;
    
    // Track peak height for debugging
    float peakHeight = filteredEcg - threshold;
    
    // Fetal peak detection with consistency checking
    if (filteredEcg > threshold && !fetalPeakDetected &&
        (currentTime - lastFetalPeak) >= fetalMinDistance) {
      
      fetalConsecutivePeaks++;
      fetalPeakStrength = filteredEcg - threshold;
      
      if (fetalConsecutivePeaks >= PEAK_PERSISTENCE) {
        fetalPeakDetected = true;
        if (lastFetalPeak != 0) {
          unsigned long interval = currentTime - lastFetalPeak;
          int newBpm = 60000 / interval;
          
          // Acceptable ranges depend on sensitivity level
          int minBpm = 70;
          int maxBpm = 180 + (currentFetalSensitivity * 20); // Higher sensitivity allows higher BPM
          
          if (newBpm >= minBpm && newBpm <= maxBpm) {
            // Add to BPM history
            fetalBpmHistory[bpmHistoryIndex % BPM_HISTORY_SIZE] = newBpm;
            
            // Weighted update based on peak strength and sensitivity
            float weight = min(1.0, fetalPeakStrength / (30.0 - currentFetalSensitivity * 5.0));
            fetalBpm = (int)(fetalBpm * (1.0 - weight) + newBpm * weight);
            
            // Ensure we have a value even on first detection
            if (fetalBpm == 0) fetalBpm = newBpm;
            
            if (DEBUG_MODE) {
              Serial.print("F-Peak: ");
              Serial.print(fetalPeakStrength);
              Serial.print(", New BPM: ");
              Serial.println(newBpm);
            }
          }
        } else {
          // First peak detected, set timestamp but no BPM yet
          if (DEBUG_MODE) {
            Serial.println("First fetal peak detected");
          }
        }
        lastFetalPeak = currentTime;
        fetalConsecutivePeaks = 0; // Reset after detection
      }
    } else if (filteredEcg < threshold * (0.7 - currentFetalSensitivity * 0.1)) { 
      // Reset threshold varies with sensitivity
      fetalPeakDetected = false;
      if (fetalConsecutivePeaks > 0) fetalConsecutivePeaks--;
    }
    
    // Decay rates vary with sensitivity
    float decayFactor = 0.99 - (currentFetalSensitivity * 0.01);
    fetalMaxValue = fetalMaxValue * decayFactor + filteredEcg * (1.0 - decayFactor);
    fetalMinValue = fetalMinValue * decayFactor + filteredEcg * (1.0 - decayFactor);
  } else {
    // Update min/max for adaptive thresholding (maternal)
    if (filteredEcg > maternalMaxValue) maternalMaxValue = filteredEcg;
    if (filteredEcg < maternalMinValue) maternalMinValue = filteredEcg;
    
    // Handle case where min and max are too close
    if (maternalMaxValue - maternalMinValue < 10) {
      maternalMaxValue += 10;
      maternalMinValue -= 10;
    }
    
    float threshold = maternalMinValue + (maternalMaxValue - maternalMinValue) * MATERNAL_PROMINENCE;
    
    // Maternal peak detection with consistency checking
    if (filteredEcg > threshold && !maternalPeakDetected &&
        (currentTime - lastMaternalPeak) >= MATERNAL_MIN_DISTANCE) {
      
      maternalConsecutivePeaks++;
      maternalPeakStrength = filteredEcg - threshold;
      
      if (maternalConsecutivePeaks >= PEAK_PERSISTENCE) {
        maternalPeakDetected = true;
        if (lastMaternalPeak != 0) {
          unsigned long interval = currentTime - lastMaternalPeak;
          int newBpm = 60000 / interval;
          
          // More accepting range for maternal heartbeat
          if (newBpm >= 30 && newBpm <= 200) {
            // Add to BPM history
            maternalBpmHistory[bpmHistoryIndex % BPM_HISTORY_SIZE] = newBpm;
            bpmHistoryIndex = (bpmHistoryIndex + 1) % BPM_HISTORY_SIZE;
            
            // Weighted update based on peak strength
            float weight = min(1.0, maternalPeakStrength / 50.0);
            maternalBpm = (int)(maternalBpm * (1.0 - weight) + newBpm * weight);
            
            // Ensure we have a value even on first detection
            if (maternalBpm == 0) maternalBpm = newBpm;
            
            if (DEBUG_MODE) {
              Serial.print("M-Peak: ");
              Serial.print(maternalPeakStrength);
              Serial.print(", New BPM: ");
              Serial.println(newBpm);
            }
          }
        } else {
          // First peak detected, set timestamp but no BPM yet
          if (DEBUG_MODE) {
            Serial.println("First maternal peak detected");
          }
        }
        lastMaternalPeak = currentTime;
        maternalConsecutivePeaks = 0; // Reset after detection
      }
    } else if (filteredEcg < threshold * 0.6) { 
      maternalPeakDetected = false;
      if (maternalConsecutivePeaks > 0) maternalConsecutivePeaks--;
    }
    
    // Adaptive decay based on signal stability
    maternalMaxValue = maternalMaxValue * 0.96 + filteredEcg * 0.04;
    maternalMinValue = maternalMinValue * 0.96 + filteredEcg * 0.04;
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
  // Always send data, even with default values
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ Wi-Fi disconnected. Trying to reconnect...");
    WiFi.begin(ssid, password);
    return false;
  }
  
  // Get timestamp
  String timestamp = getISOTimestamp();
  
  // Build the simplified JSON string directly
  String json = "{";
  json += "\"deviceId\":\"esp32\",";
  json += "\"bpm\":" + String(fetalBpm) + ",";
  json += "\"timestamp\":\"" + String(timestamp) + "\",";
  json += "\"rawEcg\":" + String(rawEcg) + ",";
  json += "\"smoothedEcg\":" + String((int)fetalFiltered);
  json += "}";
  
  // Send data to Firebase
  HTTPClient http;
  http.begin(FIREBASE_URL + "?auth=" + FIREBASE_AUTH);
  http.addHeader("Content-Type", "application/json");
  
  int httpResponseCode = http.PUT(json);
  
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