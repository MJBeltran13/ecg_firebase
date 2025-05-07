#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>
#include <ArduinoJson.h>

// --- Wi-Fi Configuration ---
const char *ssid = "BatStateU-DevOps";
const char *password = "Dev3l$06";

// --- Firebase Configuration ---
const String FIREBASE_URL = "https://ecgdata-f042a-default-rtdb.asia-southeast1.firebasedatabase.app/ecg_readings.json";
const String FIREBASE_AUTH = "AIzaSyA0OGrnWnNx0LDPGzDZHdrzajiRGEjr3AM";

// --- Time Configuration ---
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 0;

// --- Pin Definitions ---
#define AD8232_OUTPUT 35    // Analog pin connected to AD8232 output
#define LED_FETAL 25        // LED for fetal heartbeat
#define LED_MATERNAL 26     // LED for maternal heartbeat
#define LED_STATUS 27       // LED for status indicator
#define LO_PLUS 33          // LO+ pin for lead-off detection
#define LO_MINUS 32         // LO- pin for lead-off detection
#define POTENTIOMETER_PIN 14 // Potentiometer for calibration (changed from 26 to 14)

// --- ECG Processing Constants ---
const int SAMPLING_RATE = 200;  // Hz - Typical ECG sampling rate
const int BUFFER_SIZE = 40;     // Increased from 32 for better filtering
const float NYQUIST = SAMPLING_RATE / 2.0;

// --- CALIBRATION RANGES ---
const float MIN_PROMINENCE_VALUE = 0.005;  // Minimum prominence threshold
const float MAX_PROMINENCE_VALUE = 0.25;   // Maximum prominence threshold
const float MIN_DISTANCE_FACTOR = 0.15;    // Corresponds to ~400 BPM max
const float MAX_DISTANCE_FACTOR = 0.6;     // Corresponds to ~100 BPM max
const float MIN_FILTER_WEIGHT = 0.01;      // Minimum filter weight
const float MAX_FILTER_WEIGHT = 0.2;       // Maximum filter weight

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
int currentFetalSensitivity = 5; // Default to Level 6 (Very High Sensitivity)

// Maternal signal parameters (fixed - these don't change)
const int MATERNAL_MIN_DISTANCE = (SAMPLING_RATE * 0.4);  // ~150 BPM max
const float MATERNAL_PROMINENCE = 0.2;                   // Threshold
const float MATERNAL_WEIGHT = 0.08;                     // Filter weight

// Dynamic calibration parameters (adjusted by potentiometer)
float calibratedFetalProminence;    // Will be set by potentiometer
float calibratedFetalMinDistance;   // Will be set by potentiometer
float calibratedFetalFilterWeight;  // Will be set by potentiometer
bool usingCalibration = false;      // Flag to use calibrated values

// Peak detection improvement
const int PEAK_PERSISTENCE = 1;                // Reduced from 2 for faster response
const int SIGNAL_STABILIZATION_TIME = 2000;    // Reduced to 2 seconds

const int MAX_TIME_WITHOUT_DETECTION = 15000; // Keep this for other timeout purposes

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

// Last valid detected BPM values
int lastDetectedMaternalBpm = 0;
int lastDetectedFetalBpm = 0;

// Calibration variables
int lastPotValue = 0;
unsigned long lastCalibrationChange = 0;
const int CALIBRATION_THRESHOLD = 10; // Min change in potentiometer to trigger update
const int CALIBRATION_DEBOUNCE = 200; // Min time between calibration changes in ms

// --- Lead-off detection ---
bool previousLeadOffState = true; // Start with leads off as default
unsigned long lastGoodSignalTime = 0;
const int LEAD_STABILIZATION_TIME = 2000; // Increased to 2 seconds
const int LEAD_DEBOUNCE_TIME = 1000;      // Increased debounce time to 1 second
unsigned long lastLeadStateChangeTime = 0;
int consecutiveLeadOffReadings = 0;
int consecutiveLeadOnReadings = 0;
const int REQUIRED_CONSECUTIVE_OFF = 10; // Increased required readings for more stability
const int REQUIRED_CONSECUTIVE_ON = 20;  // Require more ON readings before changing state

// Additional variables for lead detection stability
unsigned long validDataStartTime = 0;
bool processingValidData = false;

void setup() {
  Serial.begin(115200);
  Serial.println("Fetal ECG Monitor - CALIBRATION MODE");
  
  // Initialize pins
  pinMode(AD8232_OUTPUT, INPUT);
  pinMode(LED_FETAL, OUTPUT);
  pinMode(LED_MATERNAL, OUTPUT);
  pinMode(LED_STATUS, OUTPUT);
  pinMode(LO_PLUS, INPUT);    // Setup LO+ pin
  pinMode(LO_MINUS, INPUT);   // Setup LO- pin
  pinMode(POTENTIOMETER_PIN, INPUT);
  
  // Initialize buffer with zeros
  for(int i = 0; i < BUFFER_SIZE; i++) {
    ecgBuffer[i] = 0;
  }

  // Set default sensitivity to higher value for better detection
  currentFetalSensitivity = 5; // Default to Level 6 (Very High Sensitivity)
  
  // Initialize calibration with default values from sensitivity level
  calibratedFetalProminence = FETAL_LEVELS[currentFetalSensitivity][0];
  calibratedFetalMinDistance = FETAL_LEVELS[currentFetalSensitivity][1];
  calibratedFetalFilterWeight = FETAL_LEVELS[currentFetalSensitivity][2];
  
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
    digitalWrite(LED_MATERNAL, HIGH);
    digitalWrite(LED_STATUS, HIGH);
    delay(200);
    digitalWrite(LED_FETAL, LOW);
    digitalWrite(LED_MATERNAL, LOW);
    digitalWrite(LED_STATUS, LOW);
    delay(200);
  }
  
  // Read initial potentiometer value
  lastPotValue = analogRead(POTENTIOMETER_PIN);
  
  // Display calibration instructions
  displayCalibrationInstructions();
}

void loop() {
  static unsigned long lastLeadMessageTime = 0;
  
  // Check if leads are connected to the body
  // Note: The AD8232 LO pins are HIGH when leads are disconnected, LOW when connected
  bool loPlus = digitalRead(LO_PLUS);
  bool loMinus = digitalRead(LO_MINUS);
  
  // Implement debouncing for lead detection with hysteresis
  if (loPlus || loMinus) {
    // Potential lead-off detected
    consecutiveLeadOffReadings++;
    consecutiveLeadOnReadings = 0;
  } else {
    // Potential lead-on detected
    consecutiveLeadOnReadings++;
    if (consecutiveLeadOffReadings > 0) consecutiveLeadOffReadings--;
  }
  
  // Determine stable lead state after enough consecutive readings with hysteresis
  bool leadsOff = previousLeadOffState;  // Default to previous state
  
  // Apply different thresholds depending on current state (hysteresis)
  if (previousLeadOffState) {
    // Currently in leads-off state, require more ON readings to change state
    if (consecutiveLeadOnReadings >= REQUIRED_CONSECUTIVE_ON) {
      leadsOff = false;  // Change to leads-on
    }
  } else {
    // Currently in leads-on state, don't change too quickly
    if (consecutiveLeadOffReadings >= REQUIRED_CONSECUTIVE_OFF) {
      leadsOff = true;   // Change to leads-off
    }
  }
  
  // Only process state changes after debounce time
  if (leadsOff != previousLeadOffState && (millis() - lastLeadStateChangeTime > LEAD_DEBOUNCE_TIME)) {
    previousLeadOffState = leadsOff;
    lastLeadStateChangeTime = millis();
    
    if (leadsOff) {
      Serial.println("● Leads disconnected - please check electrode connections");
      processingValidData = false;  // Stop data processing
    } else {
      Serial.println("◌ Leads connected - waiting for signal to stabilize");
      lastGoodSignalTime = millis(); // Reset stabilization timer
    }
  }
  
  // Signal status with LED
  digitalWrite(LED_STATUS, leadsOff ? HIGH : LOW);
  
  // Handle lead-off condition
  if (leadsOff) {
    // Print status message but limit frequency to avoid spamming
    if (millis() - lastLeadMessageTime > 3000) { // Reduced frequency to every 3 seconds
      Serial.print("● Leads off - LO+: ");
      Serial.print(loPlus ? "OFF" : "ON");
      Serial.print(", LO-: ");
      Serial.println(loMinus ? "OFF" : "ON");
      lastLeadMessageTime = millis();
    }
    
    // Flash status LED to indicate lead-off condition
    digitalWrite(LED_STATUS, (millis() / 250) % 2); // Flash twice per second
    
    // Turn off other LEDs
    digitalWrite(LED_FETAL, LOW);
    digitalWrite(LED_MATERNAL, LOW);
    
    // Reset data processing flags
    processingValidData = false;
    
    // Short delay before checking again
    delay(100);
    return; // Exit the loop to check leads again
  }
  
  // If leads are connected but signal is still stabilizing
  if (!leadsOff && (millis() - lastGoodSignalTime < LEAD_STABILIZATION_TIME)) {
    // Still stabilizing
    if (millis() - lastLeadMessageTime > 500) { // Limit message frequency
      Serial.println("◌ Signal stabilizing...");
      lastLeadMessageTime = millis();
    }
    
    digitalWrite(LED_STATUS, (millis() / 125) % 2); // Flash faster during stabilization
    delay(100);
    return;
  }
  
  // If we get here, leads are connected and signal is stable
  // But also check if this is the first time we've reached this point
  if (!processingValidData) {
    processingValidData = true;
    validDataStartTime = millis();
    Serial.println("✓ Leads connected and stable - processing ECG data");
  }
  
  // Read raw ECG value
  int rawEcg = analogRead(AD8232_OUTPUT);
  
  // Additional sanity check on signal quality
  if (rawEcg <= 100) { // Check for abnormally low values
    // Count consecutive bad readings but don't immediately change lead state
    if (millis() - lastLeadMessageTime > 500) {
      Serial.println("! Low signal detected - check electrode contact");
      lastLeadMessageTime = millis();
    }
    delay(50);
    return; // Skip this cycle and check again
  }
  
  // Check potentiometer for calibration adjustment
  checkPotentiometerCalibration();
  
  // Apply signal conditioning before filtering
  rawEcg = constrain(rawEcg, 100, 4000); // Remove extreme outliers
  
  // Apply simple noise reduction
  static int lastRawEcg = 0;
  static int prevRawEcg = 0;
  
  // More aggressive spike filtering - use median of three values
  if (abs(rawEcg - lastRawEcg) > 800) { // Increased sensitivity to spikes
    // Use median of 3 samples to reduce spikes
    int values[3] = {prevRawEcg, lastRawEcg, rawEcg};
    // Simple bubble sort to find median
    for (int i = 0; i < 2; i++) {
      for (int j = 0; j < 2 - i; j++) {
        if (values[j] > values[j + 1]) {
          int temp = values[j];
          values[j] = values[j + 1];
          values[j + 1] = temp;
        }
      }
    }
    rawEcg = values[1]; // median value
  }
  
  prevRawEcg = lastRawEcg;
  lastRawEcg = rawEcg;
  
  // Apply bandpass filters for maternal and fetal signals
  float maternalFiltered = bandpassFilter(rawEcg, false);
  float fetalFiltered = bandpassFilter(rawEcg, true);
  
  // Calculate signal quality metrics
  static float signalVariance = 0;
  static float fetalSignalStrength = 0;
  signalVariance = signalVariance * 0.95 + abs(rawEcg - 2048) * 0.05;
  fetalSignalStrength = fetalSignalStrength * 0.95 + abs(fetalFiltered) * 0.05;
  
  // Auto-adjust sensitivity based on signal quality if not in calibration mode
  if (!usingCalibration) {
    static unsigned long lastSensitivityAdjustTime = 0;
    if (millis() - lastSensitivityAdjustTime > 10000) { // Every 10 seconds
      lastSensitivityAdjustTime = millis();
      
      // If no fetal signal detected with high sensitivity, try increasing sensitivity
      if (fetalBpm == 0 && currentFetalSensitivity < 6) {
        currentFetalSensitivity++;
        if (DEBUG_MODE) {
          Serial.print("Auto-increasing sensitivity to Level ");
          Serial.println(currentFetalSensitivity + 1);
        }
      }
      // If signal is strong and stable, can reduce sensitivity slightly
      else if (fetalSignalStrength > 100 && fetalBpm > 0 && currentFetalSensitivity > 0) {
        currentFetalSensitivity--;
        if (DEBUG_MODE) {
          Serial.print("Auto-decreasing sensitivity to Level ");
          Serial.println(currentFetalSensitivity + 1);
        }
      }
    }
  }
  
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
    lastDetectedMaternalBpm = maternalBpm;
  } else {
    maternalBpm = 0;  // Report actual zero instead of default
    usingDefaultMaternalBpm = true;
  }
  
  if (fetalBpm > 0) {
    lastValidFetalBpm = millis();
    usingDefaultFetalBpm = false;
    lastDetectedFetalBpm = fetalBpm;
  } else {
    fetalBpm = 0;  // Report actual zero instead of default
    usingDefaultFetalBpm = true;
  }
  
  // Visualize heartbeats using LEDs
  if (millis() - lastFetalPeak < 200) {
    digitalWrite(LED_FETAL, HIGH);
  } else {
    digitalWrite(LED_FETAL, LOW);
  }
  
  if (millis() - lastMaternalPeak < 200) {
    digitalWrite(LED_MATERNAL, HIGH);
  } else {
    digitalWrite(LED_MATERNAL, LOW);
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
      
      if (usingCalibration) {
        Serial.print(" | CALIBRATION MODE");
        Serial.print(" | Prominence: ");
        Serial.print(calibratedFetalProminence, 3);
        Serial.print(" | Min Dist: ");
        Serial.print(calibratedFetalMinDistance / SAMPLING_RATE, 2);
        Serial.print("s | Weight: ");
        Serial.print(calibratedFetalFilterWeight, 3);
      } else {
        Serial.print(" | Sensitivity Level: ");
        Serial.print(currentFetalSensitivity + 1); // Display as 1-7 instead of 0-6
      }
      
      if (DEBUG_MODE) {
        Serial.print(" | Raw: ");
        Serial.print(rawEcg);
        Serial.print(" | M-Filt: ");
        Serial.print(maternalFiltered);
        Serial.print(" | F-Filt: ");
        Serial.print(fetalFiltered);
        Serial.print(" | Signal Quality: ");
        Serial.print(signalVariance);
        Serial.print(" | Pot: ");
        Serial.print(lastPotValue);
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
  int delayTime = max(1, (int)(5 - processingTime)); // Aim for 5ms per sample
  delay(delayTime);
  lastSampleTime = millis();
}

/**
 * Check potentiometer value and adjust calibration if needed
 */
void checkPotentiometerCalibration() {
  int currentPotValue = analogRead(POTENTIOMETER_PIN);
  static unsigned long lastPotDisplayTime = 0;
  
  // Always display current pot value at regular intervals when in debug mode
  if (DEBUG_MODE && millis() - lastPotDisplayTime > 2000) {
    lastPotDisplayTime = millis();
    Serial.print("Current Potentiometer Value: ");
    Serial.print(currentPotValue);
    Serial.print(" (0-4095) | Last Value: ");
    Serial.println(lastPotValue);
    
    // Flash the STATUS LED briefly to show pot reading is active
    digitalWrite(LED_STATUS, HIGH);
    delay(10);
    digitalWrite(LED_STATUS, LOW);
  }
  
  // Only process if significant change detected and debounce time passed
  if (abs(currentPotValue - lastPotValue) > CALIBRATION_THRESHOLD &&
      millis() - lastCalibrationChange > CALIBRATION_DEBOUNCE) {
    
    // Update last values
    lastPotValue = currentPotValue;
    lastCalibrationChange = millis();
    
    // Enable calibration mode
    usingCalibration = true;
    
    // Visual feedback - blink both LEDs to confirm calibration change
    digitalWrite(LED_FETAL, HIGH);
    digitalWrite(LED_MATERNAL, HIGH);
    delay(50);
    digitalWrite(LED_FETAL, LOW);
    digitalWrite(LED_MATERNAL, LOW);
    
    // Map potentiometer value (0-4095) to calibration parameters
    // We'll adjust prominence (threshold) as primary calibration parameter
    calibratedFetalProminence = map(currentPotValue, 0, 4095, 
                                   MIN_PROMINENCE_VALUE * 1000, 
                                   MAX_PROMINENCE_VALUE * 1000) / 1000.0;
    
    // Adjust distance and weight based on prominence to maintain overall sensitivity relationship
    // Lower prominence (more sensitive) = shorter min distance (higher max BPM) and higher weight
    float sensitivity = 1.0 - (calibratedFetalProminence - MIN_PROMINENCE_VALUE) / 
                             (MAX_PROMINENCE_VALUE - MIN_PROMINENCE_VALUE);
    
    calibratedFetalMinDistance = SAMPLING_RATE * (MAX_DISTANCE_FACTOR - 
                                 sensitivity * (MAX_DISTANCE_FACTOR - MIN_DISTANCE_FACTOR));
    
    calibratedFetalFilterWeight = MIN_FILTER_WEIGHT + 
                                 sensitivity * (MAX_FILTER_WEIGHT - MIN_FILTER_WEIGHT);
    
    // Always show calibration changes regardless of debug mode
    Serial.println("\n--- CALIBRATION CHANGED ---");
    Serial.print("Pot Value: ");
    Serial.print(currentPotValue);
    Serial.print("/4095 (");
    Serial.print(map(currentPotValue, 0, 4095, 0, 100));
    Serial.println("%)");
    Serial.print("Prominence: ");
    Serial.print(calibratedFetalProminence, 3);
    Serial.print(" | Min Distance: ");
    Serial.print(calibratedFetalMinDistance / SAMPLING_RATE, 2);
    Serial.print("s | Filter Weight: ");
    Serial.print(calibratedFetalFilterWeight, 3);
    Serial.println("\n--------------------------");
  }
}

/**
 * Display calibration instructions
 */
void displayCalibrationInstructions() {
  Serial.println("\n--- FETAL ECG CALIBRATION MODE ---");
  Serial.println("Potentiometer on pin 14 controls detection sensitivity");
  Serial.println("How to calibrate:");
  Serial.println("1. Turn potentiometer fully clockwise for highest threshold (least sensitive)");
  Serial.println("2. Turn potentiometer fully counter-clockwise for lowest threshold (most sensitive)");
  Serial.println("3. If known fetal BPM is higher than detected, decrease threshold by turning counter-clockwise");
  Serial.println("4. If false positives occur, increase threshold by turning clockwise");
  Serial.println("5. Use LED indicators to confirm detection accuracy");
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
      // Decide whether to use calibrated or predefined parameters
      float fetalWeight = usingCalibration ? 
                         calibratedFetalFilterWeight : 
                         FETAL_LEVELS[currentFetalSensitivity][2];
      
      // Fetal filter with sensitivity-adjusted weight
      weight = fetalWeight * (1 - abs(i - BUFFER_SIZE/2.0)/(BUFFER_SIZE/2.0));
      
      // Add additional emphasis on the middle frequencies (fetal heartbeat range)
      if (i > BUFFER_SIZE/4 && i < BUFFER_SIZE*3/4) {
        weight *= 1.5; // Increased from 1.2 for better fetal signal emphasis
      }
    } else {
      // Maternal filter: lower frequency components (doesn't change with sensitivity)
      weight = MATERNAL_WEIGHT * (1 - abs(i - BUFFER_SIZE/2.0)/(BUFFER_SIZE/2.0));
      
      // Add emphasis on maternal signal frequencies
      if (i < BUFFER_SIZE/3) {
        weight *= 1.4; // Emphasize lower frequencies for maternal signal
      }
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
    // Decide whether to use calibrated or predefined parameters
    float fetalProminence, fetalMinDistance;
    
    if (usingCalibration) {
      fetalProminence = calibratedFetalProminence;
      fetalMinDistance = calibratedFetalMinDistance;
    } else {
      fetalProminence = FETAL_LEVELS[currentFetalSensitivity][0];
      fetalMinDistance = FETAL_LEVELS[currentFetalSensitivity][1];
    }
    
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
      
      // More immediate detection for initial peaks
      if (fetalBpm == 0 || lastFetalPeak == 0) {
        fetalConsecutivePeaks += 2; // Give more weight to initial peaks
      } else {
        fetalConsecutivePeaks++;
      }
      
      fetalPeakStrength = filteredEcg - threshold;
      
      if (fetalConsecutivePeaks >= PEAK_PERSISTENCE) {
        fetalPeakDetected = true;
        if (lastFetalPeak != 0) {
          unsigned long interval = currentTime - lastFetalPeak;
          int newBpm = 60000 / interval;
          
          // Acceptable ranges depend on sensitivity level
          int minBpm = 70;
          int maxBpm;
          
          if (usingCalibration) {
            // For calibration mode, adjust max BPM based on min distance
            maxBpm = min(300, (int)(60000 / max(fetalMinDistance, 1.0f)));
          } else {
            // For preset sensitivity, use level-based formula
            maxBpm = 180 + (currentFetalSensitivity * 20); // Higher sensitivity allows higher BPM
          }
          
          if (newBpm >= minBpm && newBpm <= maxBpm) {
            // Add to BPM history
            fetalBpmHistory[bpmHistoryIndex % BPM_HISTORY_SIZE] = newBpm;
            
            // Weighted update based on peak strength and sensitivity
            float weight;
            if (usingCalibration) {
              weight = min(1.0, fetalPeakStrength / (50.0 * fetalProminence));
            } else {
              weight = min(1.0, fetalPeakStrength / (30.0 - currentFetalSensitivity * 5.0));
            }
            
            // More aggressive weighting to new values
            weight = min(0.6, weight + 0.2); // Increase the impact of new readings
            
            // Reset to zero logic can cause issues - avoid going to zero
            if (fetalBpm == 0) {
              fetalBpm = newBpm;
            } else {
              fetalBpm = (int)(fetalBpm * (1.0 - weight) + newBpm * weight);
            }
            
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
    } else if (filteredEcg < threshold * (0.7 - (usingCalibration ? (1.0 - fetalProminence) : (currentFetalSensitivity * 0.1)))) {
      // Reset threshold varies with sensitivity/calibration
      fetalPeakDetected = false;
      if (fetalConsecutivePeaks > 0) fetalConsecutivePeaks--;
    }
    
    // Decay rates vary with sensitivity/calibration
    float decayFactor;
    if (usingCalibration) {
      decayFactor = 0.99 - (0.1 * (1.0 - fetalProminence));
    } else {
      decayFactor = 0.99 - (currentFetalSensitivity * 0.01);
    }
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
      
      // More immediate detection for initial peaks
      if (maternalBpm == 0 || lastMaternalPeak == 0) {
        maternalConsecutivePeaks += 2; // Give more weight to initial peaks
      } else {
        maternalConsecutivePeaks++;
      }
      
      maternalPeakStrength = filteredEcg - threshold;
      
      if (maternalConsecutivePeaks >= PEAK_PERSISTENCE) {
        maternalPeakDetected = true;
        if (lastMaternalPeak != 0) {
          unsigned long interval = currentTime - lastMaternalPeak;
          int newBpm = 60000 / interval;
          
          // Acceptable range for maternal BPM
          if (newBpm >= 40 && newBpm <= 150) {
            // Add to BPM history
            maternalBpmHistory[bpmHistoryIndex % BPM_HISTORY_SIZE] = newBpm;
            bpmHistoryIndex++;
            
            // Weighted update based on peak strength
            float weight = min(0.5, maternalPeakStrength / 100.0);
            weight = min(0.6, weight + 0.2); // More aggressive weighting
            
            if (maternalBpm == 0) {
              maternalBpm = newBpm;
            } else {
              maternalBpm = (int)(maternalBpm * (1.0 - weight) + newBpm * weight);
            }
            
            if (DEBUG_MODE) {
              Serial.print("M-Peak: ");
              Serial.print(maternalPeakStrength);
              Serial.print(", New BPM: ");
              Serial.println(newBpm);
            }
          }
        } else {
          if (DEBUG_MODE) {
            Serial.println("First maternal peak detected");
          }
        }
        lastMaternalPeak = currentTime;
        maternalConsecutivePeaks = 0;
      }
    } else if (filteredEcg < threshold * 0.7) {
      maternalPeakDetected = false;
      if (maternalConsecutivePeaks > 0) maternalConsecutivePeaks--;
    }
    
    // Decay min/max values over time
    maternalMaxValue = maternalMaxValue * 0.99 + filteredEcg * 0.01;
    maternalMinValue = maternalMinValue * 0.99 + filteredEcg * 0.01;
  }
}

/**
 * Send ECG data to Firebase
 */
void sendToFirebase(int rawEcg, float fetalFiltered, float maternalFiltered) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection lost. Attempting to reconnect...");
    WiFi.begin(ssid, password);
    return;
  }

  // Get current timestamp
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  
  char timestamp[30];
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &timeinfo);
  
  // Create JSON string
  String json = "{";
  json += "\"deviceId\":\"esp32\",";
  json += "\"bpm\":" + String(fetalBpm) + ",";
  json += "\"timestamp\":\"" + String(timestamp) + "\",";
  json += "\"rawEcg\":" + String(rawEcg) + ",";
  json += "\"smoothedEcg\":" + String((int)fetalFiltered);
  json += "}";
  
  // Send to Firebase
  HTTPClient http;
  http.begin(FIREBASE_URL + "?auth=" + FIREBASE_AUTH);
  http.addHeader("Content-Type", "application/json");
  
  int httpResponseCode = http.POST(json);
  
  if (httpResponseCode > 0) {
    if (DEBUG_MODE) {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
    }
  } else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  
  http.end();
}