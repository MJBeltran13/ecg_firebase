/*
 * ESP32-S ECG Monitor with Battery Management and Firebase
 * 
 * This code implements ECG monitoring, battery monitoring, LED status indicators,
 * and Firebase data sending
 * 
 * Hardware connections:
 * - AD8232: OUTPUT -> Pin 34, LO+ -> Pin 16, LO- -> Pin 17
 * - Battery Voltage Divider -> GPIO14
 * - LED Fetal (Green) -> GPIO25
 * - LED Working (Red) -> GPIO33
 * - LED WiFi (Yellow) -> GPIO32
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <WiFiManager.h>

// WiFi and Firebase Configuration
// const char* WIFI_SSID = "BatStateU-DevOps";  // Replace with your WiFi SSID
// const char* WIFI_PASSWORD = "Dev3l$06";  // Replace with your WiFi password
const String FIREBASE_URL = "https://ecgdata-f042a-default-rtdb.asia-southeast1.firebasedatabase.app/ecg_readings.json";
const String FIREBASE_AUTH = "AIzaSyA0OGrnWnNx0LDPGzDZHdrzajiRGEjr3AM";

// NTP Configuration for timestamps
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 0;

// Firebase sending interval
unsigned long lastDataSend = 0;
const int SEND_INTERVAL = 1000; // Send data every 1 second

// Pin definitions
#define AD8232_OUTPUT 34     // Analog pin connected to AD8232 output
#define LED_FETAL 25        // green LED for fetal heartbeat
#define LED_WORKING 33      // red LED for device operation
#define LED_WIFI 32         // Yellow LED for WiFi status
#define LO_PLUS 16          // LO+ pin for lead-off detection
#define LO_MINUS 17        // LO- pin for lead-off detection
#define BATTERY_PIN 14      // Analog pin for battery voltage measurement

// ECG processing constants
const int SAMPLING_RATE = 200;  // Hz - Typical ECG sampling rate
const int BUFFER_SIZE = 32;     // Buffer size for filtering
float ecgBuffer[BUFFER_SIZE];   // Circular buffer for ECG values
int bufferIndex = 0;            // Current position in buffer

// Calibration constants
const float MIN_PROMINENCE = 0.001;   // Minimum peak prominence (most sensitive)
const float MAX_PROMINENCE = 0.1;    // Maximum peak prominence (least sensitive)
const int MIN_DISTANCE_MS = 150;     // Minimum distance between peaks (400 BPM max)
const int MAX_DISTANCE_MS = 2000;    // Maximum distance between peaks (30 BPM min)

// BPM Range Constants
const int MIN_VALID_BPM = 20;      // Minimum valid BPM for any heart rate
const int MAX_VALID_BPM = 250;     // Maximum valid BPM for any heart rate
const int MIN_MATERNAL_BPM = 30;   // Minimum maternal heart rate
const int MAX_MATERNAL_BPM = 120;  // Maximum maternal heart rate
const int MIN_FETAL_BPM = 90;     // Minimum fetal heart rate
const int MAX_FETAL_BPM = 220;     // Maximum fetal heart rate

// Variables for potentiometer and sensitivity control
int potValue = 5;  // Fixed value instead of reading from potentiometer
float prominence = 0.02;         // Threshold for peak detection (adjusted by pot)
int minPeakDistance = 300;      // Minimum time between peaks (adjusted by pot)
unsigned long lastPotReport = 0;

// BPM detection variables 
float maxValue = 0;            // Maximum signal value for adaptive thresholding
float minValue = 4095;         // Minimum signal value for adaptive thresholding
unsigned long lastPeak = 0;    // Timestamp of last detected peak
bool peakDetected = false;     // Flag for peak detection state machine
int bpm = 0;                   // Current BPM value
const int BPM_HISTORY_SIZE = 5;// Size of BPM history array
int bpmHistory[BPM_HISTORY_SIZE] = {0}; // Array to store recent BPM values
int bpmIndex = 0;              // Current position in BPM history array

// Maternal and fetal BPM tracking
int maternalBpm = 0;           // Maternal heart rate (typically 70-90)
int fetalBpm = 0;              // Fetal heart rate (typically 110-160)
unsigned long lastMaternalPeak = 0;
unsigned long lastFetalPeak = 0;
bool maternalPeakDetected = false;
bool fetalPeakDetected = false;

// Lead-off detection variables
bool leadsConnected = false;
unsigned long lastLeadChange = 0;

// Pan-Tompkins algorithm constants
const int PT_BUFFER_SIZE = 32;      // Buffer for Pan-Tompkins processing
const float PT_HP_CONSTANT = 0.998;  // High-pass filter constant
const float PT_LP_CONSTANT = 0.15;  // Low-pass filter constant
const int INTEGRATION_WIDTH = 30;    // Moving window integration width

// Pan-Tompkins buffers and variables
float pt_hp_data = 0;      // High-pass filtered data
float pt_hp_old = 0;       // Previous high-pass filtered data
float pt_lp_data = 0;      // Low-pass filtered data
float pt_lp_old = 0;       // Previous low-pass filtered data
float pt_der_data = 0;     // Derivative data
float pt_sqr_data = 0;     // Squared data
float pt_mwi_data = 0;     // Moving window integration data
float pt_buffer[PT_BUFFER_SIZE];  // Circular buffer for integration
int pt_buffer_idx = 0;     // Buffer index

// Adaptive threshold variables
float pt_threshold_i1 = 0;  // First threshold
float pt_threshold_i2 = 0;  // Second threshold
float pt_threshold_f1 = 0;  // Filtered threshold 1
float pt_threshold_f2 = 0;  // Filtered threshold 2
float pt_peak = 0;         // Peak level
float pt_npk = 0;          // Noise peak level
int pt_rr_missed = 0;      // Counter for missed beats

// Battery monitoring constants
#define BATTERY_CHECK_INTERVAL 5000  // Check battery every 5 seconds
#define BATTERY_WARNING_THRESHOLD 1.5    // Warning threshold voltage (75%)
#define BATTERY_MAX_VOLTAGE 2.0    // Maximum voltage (100%)
#define VOLTAGE_DIVIDER_RATIO 0.32    // Voltage divider ratio

// Global variables for battery monitoring
unsigned long lastBatteryCheck = 0;
float batteryVoltage = 0.0;
bool ledWorkingState = false;
bool ledWifiState = false;
bool lastFirebaseSendStatus = false;  // Track if last Firebase send was successful

void setup() {
  Serial.begin(115200);
  delay(1000);  // Give more time for initialization
  
  // Connect to WiFi
  setupWiFi();
  
  // Configure ADC for ESP32-S
  analogReadResolution(12);  // Set ADC resolution to 12 bits
  analogSetAttenuation(ADC_11db);  // Set attenuation for 0-3.3V range
  
  // Initialize pins with explicit modes
  pinMode(AD8232_OUTPUT, INPUT);  // ECG analog input
  pinMode(BATTERY_PIN, INPUT);    // Battery voltage input
  pinMode(LED_FETAL, OUTPUT);
  pinMode(LED_WORKING, OUTPUT);
  pinMode(LED_WIFI, OUTPUT);
  pinMode(LO_PLUS, INPUT);    // Lead-off detection
  pinMode(LO_MINUS, INPUT);   // Lead-off detection
  
  // Initialize buffer with midpoint value
  for (int i = 0; i < BUFFER_SIZE; i++) {
    ecgBuffer[i] = 2048;  // Mid-point of 12-bit ADC (0-4095)
  }
  
  // Initial battery reading
  batteryVoltage = getBatteryVoltage();
  
  // Configure time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  // Initial LED test
  testLEDs();
  
  Serial.println("\n==== ESP32-S ECG Monitor with Firebase ====");
  Serial.println("Monitoring ECG signal and sending to Firebase");
  Serial.println("====================================");
}

void loop() {
  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    reconnectWiFi();
  }
  
  // Read sensors
  int rawEcg = analogRead(AD8232_OUTPUT);
  float filteredEcg = 0;
  
  // Check for leads-off condition
  bool leadsOff = checkLeadsOff();
  
  // If leads are connected, process ECG
  if (!leadsOff) {
    // Ensure raw ECG is within valid range
    if (rawEcg >= 0 && rawEcg <= 4095) {
      filteredEcg = filterEcg(rawEcg);
      
      // Detect peaks and calculate BPM
      detectPeaksAndCalculateBpm(filteredEcg);
    } else {
      rawEcg = 2048;  // Set to midpoint if reading is invalid
    }
    
    // Send data to Firebase periodically
    if (millis() - lastDataSend >= SEND_INTERVAL) {
      // Update battery voltage before sending
      batteryVoltage = getBatteryVoltage();
      
      sendToFirebase(rawEcg, filteredEcg);
      lastDataSend = millis();
    }
  } else {
    // If leads are off, set default values
    rawEcg = 2048;
    filteredEcg = 2048;
    fetalBpm = 0;
  }
  
  // Check battery status
  if (millis() - lastBatteryCheck >= BATTERY_CHECK_INTERVAL) {
    batteryVoltage = getBatteryVoltage();
    lastBatteryCheck = millis();
  }
  
  // Update LED states
  updateLeds();
  
  // Display information
  displayInfo(rawEcg, filteredEcg);
  
  // Maintain consistent sampling rate
  static unsigned long lastSampleTime = 0;
  unsigned long processingTime = millis() - lastSampleTime;
  int delayTime = max(1, (int)(5 - processingTime)); // Aim for ~200Hz sampling
  delay(delayTime);
  lastSampleTime = millis();
}

void setupWiFi() {
  // Initialize WiFiManager
  WiFiManager wifiManager;
  
  // Disable saving WiFi credentials
  wifiManager.setConfigPortalTimeout(180);  // 3 minutes timeout for config portal
  wifiManager.setSaveConfigCallback(nullptr);  // Remove config save callback
  wifiManager.setBreakAfterConfig(true);  // Exit config portal after configuration
  
  // Optional: Add custom parameters if needed
  // WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  // wifiManager.addParameter(&custom_mqtt_server);
  
  // Start config portal without trying to connect first
  if (!wifiManager.startConfigPortal("ECG_Monitor_Setup")) {
    Serial.println("Failed to connect or config portal timed out");
    
    // Reset and try again
    ESP.restart();
    delay(5000);
  }
  
  // If you get here, you have connected to the WiFi
  Serial.println("\n✅ Connected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void reconnectWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("Reconnecting to WiFi");
    
    // Use WiFiManager to reconnect
    WiFiManager wifiManager;
    wifiManager.setConfigPortalTimeout(180);  // 3 minutes timeout
    wifiManager.setBreakAfterConfig(true);
    
    if (!wifiManager.startConfigPortal("ECG_Monitor_Reconnect")) {
      Serial.println("\n❌ Failed to reconnect to WiFi");
      
      // Reset and try again
      ESP.restart();
      delay(5000);
    } else {
      Serial.println("\n✅ Reconnected to WiFi");
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());
    }
  }
}

void sendToFirebase(int rawEcg, float smoothedEcg) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ WiFi not connected. Skipping data send.");
    Serial.print("WiFi Status: ");
    Serial.println(WiFi.status());
    lastFirebaseSendStatus = false;
    return;
  }

  // Get current timestamp
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)) {
    Serial.println("❌ Failed to obtain time");
    lastFirebaseSendStatus = false;
    return;
  }
  
  char timestamp[30];
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", &timeinfo);

  // Create JSON using ArduinoJson
  StaticJsonDocument<200> doc;
  doc["deviceId"] = "esp32";
  doc["bpm"] = fetalBpm;
  doc["timestamp"] = timestamp;
  doc["rawEcg"] = rawEcg;
  doc["smoothedEcg"] = smoothedEcg;
  
  String jsonString;
  serializeJson(doc, jsonString);

  // Print JSON for debugging
  Serial.println("\n--- Firebase Debug Info ---");
  Serial.println("Sending JSON:");
  Serial.println(jsonString);

  // Send to Firebase
  HTTPClient http;
  
  // Construct the full URL with auth token
  String url = FIREBASE_URL + "?auth=" + FIREBASE_AUTH;
  Serial.print("URL: ");
  Serial.println(url);
  
  http.begin(url);
  
  // Set headers
  http.addHeader("Content-Type", "application/json");
  
  // Send PUT request
  Serial.println("Sending PUT request...");
  int httpResponseCode = http.PUT(jsonString);
  Serial.print("Response Code: ");
  Serial.println(httpResponseCode);

  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.print("Response Body: ");
    Serial.println(response);
    
    if (httpResponseCode == 200) {
      Serial.println("✅ Data sent to Firebase successfully");
      lastFirebaseSendStatus = true;
    } else {
      Serial.print("⚠️ HTTP Response Code: ");
      Serial.println(httpResponseCode);
      Serial.println("Response: " + response);
      lastFirebaseSendStatus = false;
    }
  } else {
    Serial.print("❌ Error sending data. Error code: ");
    Serial.println(httpResponseCode);
    Serial.println("Error message: " + http.errorToString(httpResponseCode));
    lastFirebaseSendStatus = false;
  }

  http.end();
  Serial.println("------------------------\n");
}

void testLEDs() {
  // Flash all LEDs three times
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_FETAL, HIGH);
    digitalWrite(LED_WORKING, HIGH);
    digitalWrite(LED_WIFI, HIGH);
    delay(200);
    digitalWrite(LED_FETAL, LOW);
    digitalWrite(LED_WORKING, LOW);
    digitalWrite(LED_WIFI, LOW);
    delay(200);
  }
}

float getBatteryVoltage() {
  // Take multiple readings and average them for stability
  const int numReadings = 10;
  int32_t adcTotal = 0;
  
  for(int i = 0; i < numReadings; i++) {
    adcTotal += analogRead(BATTERY_PIN);
    delay(5);  // Small delay between readings
  }
  
  int rawValue = adcTotal / numReadings;
  
  // ESP32-S ADC conversion with improved accuracy
  float voltage = (float)rawValue / 4095.0 * 3.3;
  
  // Apply ESP32-S ADC non-linearity correction
  if (voltage > 0) {
    voltage = voltage * 1.1;  // Compensate for ADC non-linearity
  }
  
  // If using voltage divider, compensate for it
  voltage = voltage / VOLTAGE_DIVIDER_RATIO;
  
  return voltage;
}

void checkBatteryStatus() {
  if (millis() - lastBatteryCheck >= BATTERY_CHECK_INTERVAL) {
    lastBatteryCheck = millis();
    batteryVoltage = getBatteryVoltage();
    
    // Calculate battery percentage
    float batteryPercentage = (batteryVoltage / BATTERY_MAX_VOLTAGE) * 100;
    batteryPercentage = constrain(batteryPercentage, 0, 100);
    
    // Only print battery status in detailed info display
    if (batteryVoltage <= BATTERY_WARNING_THRESHOLD) {
      Serial.println("WARNING: Low Battery!");
    }
  }
}

/**
 * Check if leads are connected properly
 * @return true if leads are disconnected
 */
bool checkLeadsOff() {
  bool loPlus = digitalRead(LO_PLUS);
  bool loMinus = digitalRead(LO_MINUS);
  bool currentLeadsOff = (loPlus || loMinus);
  
  // Update connected status with debouncing
  static int consecutiveReadings = 0;
  if (currentLeadsOff != !leadsConnected) {
    consecutiveReadings++;
    if (consecutiveReadings > 10) {
      leadsConnected = !currentLeadsOff;
      consecutiveReadings = 0;
      lastLeadChange = millis();
      
      if (leadsConnected) {
        Serial.println("✓ Leads connected");
      } else {
        Serial.println("✗ Leads disconnected");
      }
    }
  } else {
    consecutiveReadings = 0;
  }
  
  // Show lead status periodically if disconnected
  static unsigned long lastLeadMessage = 0;
  if (!leadsConnected && millis() - lastLeadMessage > 2000) {
    Serial.print("Leads off - LO+: ");
    Serial.print(loPlus ? "OFF" : "ON");
    Serial.print(", LO-: ");
    Serial.println(loMinus ? "OFF" : "ON");
    lastLeadMessage = millis();
  }
  
  // Update status LEDs
  if (!leadsConnected) {
    digitalWrite(LED_WORKING, HIGH);
    digitalWrite(LED_WIFI, HIGH);
  } else {
    digitalWrite(LED_WORKING, LOW);
    digitalWrite(LED_WIFI, LOW);
  }
  
  return !leadsConnected;
}

/**
 * Read potentiometer and update sensitivity parameters
 */
void updateSensitivityFromPot() {
  // Skip reading potentiometer since we're using fixed value
  
  // Map pot value to ECG sensitivity parameters
  // 1. Prominence/threshold (lower value = more sensitive)
  prominence = map(potValue, 0, 4095, MIN_PROMINENCE * 1000, MAX_PROMINENCE * 1000) / 1000.0;
  
  // 2. Min distance between peaks (lower value = detect faster heart rates)
  minPeakDistance = map(potValue, 0, 4095, MIN_DISTANCE_MS, MAX_DISTANCE_MS);
  
  // Report changes less frequently to avoid flooding serial monitor
  if (millis() - lastPotReport > 1000) {
    lastPotReport = millis();
    
    Serial.println("\n--- SENSITIVITY UPDATED ---");
    Serial.print("Pot Value: ");
    Serial.print(potValue);
    Serial.print("/4095 (");
    Serial.print(map(potValue, 0, 4095, 0, 100));
    Serial.println("%)");
    Serial.print("Prominence: ");
    Serial.print(prominence, 3);
    Serial.print(" | Min Distance: ");
    Serial.print(minPeakDistance);
    Serial.println("ms");
    
    // Visual bar to represent sensitivity level
    Serial.print("Sensitivity: [");
    int barLength = map(4095 - potValue, 0, 4095, 0, 20); // Inverted for sensitivity
    for (int i = 0; i < 20; i++) {
      Serial.print(i < barLength ? "#" : "-");
    }
    Serial.println("]");
    Serial.println("------------------------");
    
    // Flash LEDs to indicate sensitivity change
    digitalWrite(LED_FETAL, HIGH);
    digitalWrite(LED_WORKING, HIGH);
    digitalWrite(LED_WIFI, HIGH);
    delay(30);
    digitalWrite(LED_FETAL, LOW);
    digitalWrite(LED_WORKING, LOW);
    digitalWrite(LED_WIFI, LOW);
  }
}

/**
 * Enhanced ECG filtering function
 */
float filterEcg(int rawEcg) {
  // First apply the existing baseline removal
  ecgBuffer[bufferIndex] = (float)rawEcg;
  bufferIndex = (bufferIndex + 1) % BUFFER_SIZE;
  
  float filtered = 0;
  float totalWeight = 0;
  
  // Stage 1: Weighted moving average (low-pass filter)
  for (int i = 0; i < BUFFER_SIZE; i++) {
    float weight = exp(-pow(i - BUFFER_SIZE/2.0, 2) / (2 * pow(BUFFER_SIZE/3.0, 2)));
    filtered += ecgBuffer[(bufferIndex - i + BUFFER_SIZE) % BUFFER_SIZE] * weight;
    totalWeight += weight;
  }
  
  filtered = filtered / totalWeight;
  
  // Stage 2: Remove baseline wander
  static float lastFiltered = 2048;
  static float baseline = 2048;
  baseline = 0.998 * baseline + 0.002 * filtered;
  float highpassed = filtered - baseline;
  float result = highpassed + 2048;
  result = constrain(result, 0, 4095);
  
  // Apply Pan-Tompkins processing
  bool qrs_detected = panTompkinsQRS(result);
  if(qrs_detected) {
    // Update peak detection flags
    peakDetected = true;
    lastPeak = millis();
  }
  
  return result;
}

/**
 * Pan-Tompkins QRS detection algorithm
 * Returns true if QRS complex is detected
 */
bool panTompkinsQRS(float input_data) {
    // Stage 1: Bandpass filter (combined low-pass and high-pass)
    // High-pass filter
    pt_hp_data = input_data - pt_hp_old + PT_HP_CONSTANT * pt_hp_data;
    pt_hp_old = input_data;
    
    // Low-pass filter
    pt_lp_data = pt_hp_data + 2 * pt_lp_old + PT_LP_CONSTANT * pt_lp_data;
    pt_lp_old = pt_hp_data;
    
    // Stage 2: Derivative
    static float der_data[4] = {0};
    pt_der_data = (2 * pt_lp_data + der_data[0] - der_data[2] - 2 * der_data[3]) / 6.0;
    for(int i = 3; i > 0; i--) der_data[i] = der_data[i-1];
    der_data[0] = pt_lp_data;
    
    // Stage 3: Squaring
    pt_sqr_data = pt_der_data * pt_der_data;
    
    // Stage 4: Moving window integration
    pt_buffer[pt_buffer_idx] = pt_sqr_data;
    float sum = 0;
    for(int i = 0; i < PT_BUFFER_SIZE; i++) {
        sum += pt_buffer[i];
    }
    pt_mwi_data = sum / PT_BUFFER_SIZE;
    pt_buffer_idx = (pt_buffer_idx + 1) % PT_BUFFER_SIZE;
    
    // Stage 5: Adaptive thresholding
    static unsigned long last_qrs = 0;
    static float rr_average1 = 0;
    static float rr_average2 = 0;
    static int rr_count = 0;
    bool qrs_detected = false;
    
    unsigned long current_time = millis();
    
    // Update thresholds
    pt_threshold_i1 = pt_npk + 0.15 * (pt_peak - pt_npk);
    pt_threshold_i2 = 0.3 * pt_threshold_i1;
    
    // Check for QRS complex
    if(pt_mwi_data > pt_threshold_i1) {
        if(current_time - last_qrs > 100) {
            pt_peak = 0.9 * pt_peak + 0.1 * pt_mwi_data;
            qrs_detected = true;
            
            // Update RR intervals
            if(last_qrs > 0) {
                unsigned long rr_interval = current_time - last_qrs;
                if(rr_count < 8) {
                    rr_average1 = ((rr_count * rr_average1) + rr_interval) / (rr_count + 1);
                    rr_average2 = rr_average1;
                    rr_count++;
                } else {
                    if(rr_interval >= 0.85 * rr_average2 && rr_interval <= 1.25 * rr_average2) {
                        rr_average2 = 0.9 * rr_average2 + 0.1 * rr_interval;
                    }
                }
            }
            last_qrs = current_time;
            pt_rr_missed = 0;
        }
    } else if(pt_mwi_data > pt_threshold_i2) {
        pt_npk = 0.9 * pt_npk + 0.1 * pt_mwi_data;
    }
    
    // Search back for missed QRS
    if(current_time - last_qrs > 1.5 * rr_average2) {
        pt_threshold_i1 *= 0.6;
        pt_threshold_i2 *= 0.6;
        pt_rr_missed++;
        
        if(pt_rr_missed > 3) {
            // Reset thresholds if too many beats missed
            pt_threshold_i1 = pt_npk + 0.15 * (pt_peak - pt_npk);
            pt_threshold_i2 = 0.3 * pt_threshold_i1;
            pt_rr_missed = 0;
        }
    }
    
    return qrs_detected;
}

/**
 * Detect peaks in filtered ECG and calculate BPM
 */
void detectPeaksAndCalculateBpm(float filteredEcg) {
    static float baseline = 2048;
    static float lastValue = 0;
    unsigned long currentTime = millis();
    
    // Apply Pan-Tompkins QRS detection
    bool qrs_detected = panTompkinsQRS(filteredEcg);
    
    if (qrs_detected && currentTime - lastPeak >= minPeakDistance) {
        if (lastPeak > 0) {
            unsigned long interval = currentTime - lastPeak;
            int newBpm = 60000 / interval;
            
            // Only accept reasonable values using configurable ranges
            if (newBpm >= MIN_VALID_BPM && newBpm <= MAX_VALID_BPM) {
                // Add to BPM history
                bpmHistory[bpmIndex] = newBpm;
                bpmIndex = (bpmIndex + 1) % BPM_HISTORY_SIZE;
                
                // Calculate average BPM
                int total = 0, count = 0;
                for (int i = 0; i < BPM_HISTORY_SIZE; i++) {
                    if (bpmHistory[i] > 0) {
                        total += bpmHistory[i];
                        count++;
                    }
                }
                
                if (count > 0) {
                    bpm = total / count;
                    
                    // Classify as maternal or fetal based on ranges
                    if (bpm >= MIN_MATERNAL_BPM && bpm <= MAX_MATERNAL_BPM) {
                        maternalBpm = bpm;
                        lastMaternalPeak = currentTime;
                    } else if (bpm >= MIN_FETAL_BPM && bpm <= MAX_FETAL_BPM) {
                        fetalBpm = bpm;
                        lastFetalPeak = currentTime;
                    }
                }
            }
        }
        lastPeak = currentTime;
    }
    
    // Store current value for next iteration
    lastValue = filteredEcg;
}

/**
 * Update LEDs based on device status
 */
void updateLeds() {
  unsigned long currentMillis = millis();
  
  // Update LED_WORKING (Red) based on battery voltage
  if (batteryVoltage <= BATTERY_WARNING_THRESHOLD) {  // 1.5V threshold
    // Blink when battery is low
    ledWorkingState = (currentMillis / 1000) % 2;  // Blink once per second
  } else {
    // Off when battery is good
    ledWorkingState = false;
  }
  digitalWrite(LED_WORKING, ledWorkingState);
  
  // Update LED_WIFI (Yellow) based on WiFi status
  if (WiFi.status() != WL_CONNECTED) {
    // Blink when disconnected
    ledWifiState = (currentMillis / 500) % 2;  // Blink twice per second
  } else {
    // Off when connected
    ledWifiState = false;
  }
  digitalWrite(LED_WIFI, ledWifiState);
  
  // Update LED_FETAL (Green) based on raw ECG reading
  int rawEcg = analogRead(AD8232_OUTPUT);
  if (rawEcg > 0 && rawEcg < 4095) {  // Check if there's a valid ECG reading
    digitalWrite(LED_FETAL, HIGH);
  } else {
    digitalWrite(LED_FETAL, LOW);
  }
}

/**
 * Display ECG data and device status
 */
void displayInfo(int rawEcg, float filteredEcg) {
  // Output data in Serial Plotter format
  Serial.print(rawEcg);
  Serial.print(",");
  Serial.print(filteredEcg);
  Serial.print(",");
  Serial.print(batteryVoltage);
  Serial.print(",");
  Serial.print(fetalBpm);
  Serial.print(",");
  Serial.print(lastFirebaseSendStatus ? "1" : "0");  // Add Firebase status to the plot (1 = sent, 0 = not sent)
  Serial.println();
  
  // Print detailed information every second
  static unsigned long lastDetailedPrint = 0;
  if (millis() - lastDetailedPrint >= 1000) {
    lastDetailedPrint = millis();
    
    Serial.println("\n--- Device Status ---");
    Serial.print("Fetal BPM: ");
    if (fetalBpm > 0 && millis() - lastFetalPeak < 5000) {
      Serial.print(fetalBpm);
      Serial.println(" 👶");
    } else {
      Serial.println("--");
    }
    
    Serial.print("Battery Voltage: ");
    Serial.print(batteryVoltage, 2);
    Serial.print("V (");
    Serial.print((batteryVoltage / BATTERY_MAX_VOLTAGE) * 100, 1);
    Serial.println("%)");
    
    Serial.print("Signal Quality: ");
    if (pt_peak > 0) {
      float signalQuality = (pt_peak - pt_npk) / pt_peak * 100;
      Serial.print(signalQuality, 1);
      Serial.println("%");
    } else {
      Serial.println("--");
    }
    
    Serial.print("WiFi Status: ");
    Serial.println(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");

    Serial.print("Firebase Status: ");
    Serial.println(lastFirebaseSendStatus ? "Data Sent ✅" : "Not Sent ❌");
    
    Serial.println("-------------------");
  }
  
  // Small delay to control data rate
  delay(1);
} 