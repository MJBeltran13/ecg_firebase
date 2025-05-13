/*
 * Potentiometer + ECG BPM Test
 * 
 * This sketch allows testing the potentiometer on pin 14 while also
 * detecting BPM from the AD8232 ECG sensor.
 * 
 * Hardware connections:
 * - AD8232: OUTPUT -> Pin 35, LO+ -> Pin 33, LO- -> Pin 32
 * - Potentiometer: Middle pin -> ESP32 pin 14
 * - LEDs: Fetal -> Pin 25, Working -> Pin 26, WiFi -> Pin 27
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Pin definitions
#define AD8232_OUTPUT 35    // Analog pin connected to AD8232 output
#define LED_FETAL 25        // LED for fetal heartbeat
#define LED_WORKING 26      // Green LED for device operation
#define LED_WIFI 27         // Yellow LED for WiFi status
#define LO_PLUS 33          // LO+ pin for lead-off detection
#define LO_MINUS 32         // LO- pin for lead-off detection
#define BATTERY_PIN 34      // Analog pin for battery voltage measurement

// Battery monitoring constants
#define BATTERY_CHECK_INTERVAL 5000  // Check battery every 5 seconds
#define BATTERY_LOW_THRESHOLD 2.7    // Low battery threshold voltage
#define BATTERY_CRITICAL_THRESHOLD 2.4 // Critical battery threshold voltage
#define VOLTAGE_DIVIDER_RATIO 0.32    // Voltage divider ratio if used

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
int potValue = 5;            // Fixed potentiometer value
float prominence = 0.02;     // Threshold for peak detection
int minPeakDistance = 300;   // Minimum time between peaks

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

// Firebase configuration
#define FIREBASE_URL "https://ecgdata-f042a-default-rtdb.asia-southeast1.firebasedatabase.app/ecg_readings.json"
#define FIREBASE_AUTH "AIzaSyA0OGrnWnNx0LDPGzDZHdrzajiRGEjr3AM"
#define WIFI_SSID "BatStateU-DevOps"  // Replace with your WiFi SSID
#define WIFI_PASSWORD "Dev3l$06"  // Replace with your WiFi password

// NTP time setup
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 0;

// Device identification
String deviceId = "ESP32_" + String((uint32_t)ESP.getEfuseMac(), HEX);
unsigned long readingCounter = 0;

// Add these variables after other global variables
unsigned long lastBatteryCheck = 0;
float batteryVoltage = 0.0;

// Add these LED state variables after other global variables
bool ledWorkingState = false;
bool ledWifiState = false;
unsigned long lastLedUpdate = 0;

// Add this function before setup()
float getBatteryVoltage() {
  int rawValue = analogRead(BATTERY_PIN);
  Serial.print("Raw ADC Value: ");
  Serial.println(rawValue);
  float voltage = (rawValue * 3.3 / 4095.0) * VOLTAGE_DIVIDER_RATIO;
  return voltage;
}

void checkBatteryStatus() {
  if (millis() - lastBatteryCheck >= BATTERY_CHECK_INTERVAL) {
    lastBatteryCheck = millis();
    batteryVoltage = getBatteryVoltage();
    
    // Print battery status
    Serial.print("Battery Voltage: ");
    Serial.print(batteryVoltage, 2);
    Serial.println("V");
    
    // Update LED patterns based on battery status
    if (batteryVoltage <= BATTERY_CRITICAL_THRESHOLD) {
      // Critical battery - rapid blinking of working LED
      digitalWrite(LED_WORKING, (millis() / 250) % 2);
    } else if (batteryVoltage <= BATTERY_LOW_THRESHOLD) {
      // Low battery - slow blinking of working LED
      digitalWrite(LED_WORKING, (millis() / 1000) % 2);
    }
    // Normal battery level is handled in the main loop
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  
  // Initialize WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ Connected to Wi-Fi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  
  // Sync time for timestamps
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("❌ Failed to get time");
  }
  
  // Initialize pins
  pinMode(AD8232_OUTPUT, INPUT);
  pinMode(LED_FETAL, OUTPUT);
  pinMode(LED_WORKING, OUTPUT);
  pinMode(LED_WIFI, OUTPUT);
  pinMode(LO_PLUS, INPUT);
  pinMode(LO_MINUS, INPUT);
  pinMode(BATTERY_PIN, INPUT);
  
  // Initialize buffer
  for (int i = 0; i < BUFFER_SIZE; i++) {
    ecgBuffer[i] = 0;
  }
  
  // Initial LED test - flash all LEDs
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
  
  // Print header
  Serial.println("\n==== ECG BPM Monitoring ====");
  Serial.println("====================================");
  
  // Initialize sensitivity settings once at startup
  updateSensitivityFromPot();
}

void loop() {
  // Check battery status first
  checkBatteryStatus();
  
  // Check for leads-off condition
  bool leadsOff = checkLeadsOff();
  
  // Update all LEDs
  updateLeds();
  
  // Only process ECG if leads are connected and battery is not critical
  if (!leadsOff && batteryVoltage > BATTERY_CRITICAL_THRESHOLD) {
    // Read and filter ECG signal
    int rawEcg = analogRead(AD8232_OUTPUT);
    float filteredEcg = filterEcg(rawEcg);
    
    // Detect peaks and calculate BPM
    detectPeaksAndCalculateBpm(filteredEcg);
    
    // Display information periodically
    displayInfo(rawEcg, filteredEcg);
  }
  
  // Maintain consistent sampling rate
  static unsigned long lastSampleTime = 0;
  unsigned long processingTime = millis() - lastSampleTime;
  int delayTime = max(1, (int)(5 - processingTime)); // Aim for ~200Hz sampling
  delay(delayTime);
  lastSampleTime = millis();
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
  
  return !leadsConnected;
}

/**
 * Set fixed sensitivity parameters from potentiometer value
 */
void updateSensitivityFromPot() {
  // Fixed pot value is already set at initialization (potValue = 5)
  
  // Map pot value to ECG sensitivity parameters
  // 1. Prominence/threshold (lower value = more sensitive)
  prominence = map(potValue, 0, 4095, MIN_PROMINENCE * 1000, MAX_PROMINENCE * 1000) / 1000.0;
  
  // 2. Min distance between peaks (lower value = detect faster heart rates)
  minPeakDistance = map(potValue, 0, 4095, MIN_DISTANCE_MS, MAX_DISTANCE_MS);
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
 * Update LEDs based on heartbeat and status
 */
void updateLeds() {
  unsigned long currentMillis = millis();
  
  // Update LED_FETAL based on fetal heartbeat
  if (millis() - lastFetalPeak < 150 && fetalBpm > 0) {
    digitalWrite(LED_FETAL, HIGH);
  } else {
    digitalWrite(LED_FETAL, LOW);
  }
  
  // Update LED_WORKING based on device state priority:
  // 1. Battery Critical (fast blink)
  // 2. Battery Low (slow blink)
  // 3. Lead-off (off)
  // 4. Normal operation (solid on)
  if (batteryVoltage <= BATTERY_CRITICAL_THRESHOLD) {
    // Critical battery - rapid blinking (4 times per second)
    ledWorkingState = (currentMillis / 250) % 2;
  } else if (batteryVoltage <= BATTERY_LOW_THRESHOLD) {
    // Low battery - slow blinking (once per second)
    ledWorkingState = (currentMillis / 1000) % 2;
  } else if (!leadsConnected) {
    // Leads off - LED off
    ledWorkingState = false;
  } else {
    // Normal operation - solid on
    ledWorkingState = true;
  }
  digitalWrite(LED_WORKING, ledWorkingState);
  
  // Update LED_WIFI based on WiFi status
  if (WiFi.status() == WL_CONNECTED) {
    ledWifiState = true;
  } else {
    // Blink when disconnected (twice per second)
    ledWifiState = (currentMillis / 500) % 2;
  }
  digitalWrite(LED_WIFI, ledWifiState);
}

/**
 * Display ECG data and BPM values in Serial Plotter format
 */
void displayInfo(int rawEcg, float filteredEcg) {
  // Output data in Serial Plotter format (comma-separated values)
  Serial.print(rawEcg);
  Serial.print(",");
  Serial.print(filteredEcg);
  Serial.print(",");
  
  // Output fixed value where potentiometer reading used to be (for consistency)
  Serial.print(potValue);
  Serial.print(",");
  
  // Output fetal BPM (show 0 if no recent detection)
  if (fetalBpm > 0 && millis() - lastFetalPeak < 5000) {
    Serial.print(fetalBpm);
  } else {
    Serial.print("0");
  }
  Serial.println();
  
  // Print detailed information to Serial Monitor every second
  static unsigned long lastDetailedPrint = 0;
  if (millis() - lastDetailedPrint >= 1000) {
    lastDetailedPrint = millis();
    
    Serial.println("\n--- Device Status ---");
    
    // Battery Status
    Serial.print("Battery: ");
    Serial.print(batteryVoltage, 2);
    Serial.print("V (");
    if (batteryVoltage <= BATTERY_CRITICAL_THRESHOLD) {
      Serial.println("CRITICAL)");
    } else if (batteryVoltage <= BATTERY_LOW_THRESHOLD) {
      Serial.println("LOW)");
    } else {
      Serial.println("GOOD)");
    }
    
    // WiFi Status
    Serial.print("WiFi: ");
    Serial.println(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
    
    // ECG Status
    Serial.print("Fetal BPM: ");
    if (fetalBpm > 0 && millis() - lastFetalPeak < 5000) {
      Serial.print(fetalBpm);
      Serial.println(" ❤");
    } else {
      Serial.println("--");
    }
    
    Serial.println("-------------------");
  }
  
  // Send data to Firebase every second
  static unsigned long lastFirebaseUpdate = 0;
  if (millis() - lastFirebaseUpdate >= 1000) {
    lastFirebaseUpdate = millis();
    sendToFirebase(rawEcg, filteredEcg, filteredEcg);  // Using same filtered value for both fetal and maternal for now
  }
  
  // Small delay to control data rate
  delay(1);
}

void reconnectWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Reconnecting to WiFi");
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nReconnected to WiFi");
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("\nFailed to reconnect to WiFi");
    }
  }
}

String getISOTimestamp() {
  // Get current time from NTP server
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  char timestamp[30];
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", timeinfo);
  return String(timestamp);
}

bool sendToFirebase(int rawEcg, float fetalFiltered, float maternalFiltered) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ Wi-Fi disconnected. Trying to reconnect...");
    reconnectWiFi();
    return false;
  }

  // Get timestamp
  String timestamp = getISOTimestamp();

  // Construct JSON
  String json = "{";
  json += "\"deviceId\":\"esp32\",";
  json += "\"bpm\":" + String(fetalBpm) + ",";
  json += "\"timestamp\":\"" + String(timestamp) + "\",";
  json += "\"rawEcg\":" + String(rawEcg) + ",";
  json += "\"smoothedEcg\":" + String(fetalFiltered);
  json += "}";

  HTTPClient http;
  // Convert C-style string literals to String objects before concatenation
  String url = String(FIREBASE_URL) + "?auth=" + String(FIREBASE_AUTH);
  http.begin(url); // Use the String object
  http.addHeader("Content-Type", "application/json");

  int httpResponseCode = http.PUT(json);

  if (httpResponseCode > 0) {
    Serial.println("✅ Data sent: " + json);
    Serial.println("Response: " + http.getString());
    http.end();
    return true;
  } else {
    Serial.print("❌ Error sending data: ");
    Serial.println(httpResponseCode);
    http.end();
    return false;
  }
} 