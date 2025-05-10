/*
 * Potentiometer + ECG BPM Test with FreeRTOS
 * 
 * This sketch allows testing the potentiometer on pin 14 while also
 * detecting BPM from the AD8232 ECG sensor. It uses FreeRTOS to manage
 * different tasks concurrently.
 * 
 * Hardware connections:
 * - AD8232: OUTPUT -> Pin 35, LO+ -> Pin 33, LO- -> Pin 32
 * - Potentiometer: Middle pin -> ESP32 pin 14
 * - LEDs: Fetal -> Pin 25, Working -> Pin 26, WiFi -> Pin 27
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
// ESP32 Arduino core has built-in FreeRTOS

// RTOS settings
#define STACK_SIZE 4096                // Stack size for tasks
#define ECG_TASK_PRIORITY 3            // Highest priority for ECG sampling
#define LED_TASK_PRIORITY 2            // Medium priority for LED updates
#define BATTERY_TASK_PRIORITY 1        // Lower priority for battery monitoring
#define FIREBASE_TASK_PRIORITY 1       // Lower priority for Firebase uploads
#define POTENTIOMETER_TASK_PRIORITY 2  // Medium priority for potentiometer reading

// RTOS task handles
TaskHandle_t ecgTaskHandle = NULL;
TaskHandle_t ledTaskHandle = NULL;
TaskHandle_t batteryTaskHandle = NULL;
TaskHandle_t firebaseTaskHandle = NULL;
TaskHandle_t potentiometerTaskHandle = NULL;

// Data protection
SemaphoreHandle_t ecgMutex;
SemaphoreHandle_t bpmMutex;
SemaphoreHandle_t batteryMutex;

// RTOS queues for communication between tasks
QueueHandle_t ecgDataQueue;
QueueHandle_t bpmDataQueue;

// Pin definitions
#define AD8232_OUTPUT 35    // Analog pin connected to AD8232 output
#define POTENTIOMETER_PIN 14 // Potentiometer for calibration
#define LED_FETAL 25        // LED for fetal heartbeat
#define LED_WORKING 26      // Green LED for device operation
#define LED_WIFI 27         // Yellow LED for WiFi status
#define LO_PLUS 33          // LO+ pin for lead-off detection
#define LO_MINUS 32         // LO- pin for lead-off detection
#define BATTERY_PIN 34      // Analog pin for battery voltage measurement

// Battery monitoring constants
#define BATTERY_CHECK_INTERVAL 5000  // Check battery every 5 seconds
#define BATTERY_LOW_THRESHOLD 3.3    // Low battery threshold voltage
#define BATTERY_CRITICAL_THRESHOLD 3.0 // Critical battery threshold voltage
#define VOLTAGE_DIVIDER_RATIO 2.0    // Voltage divider ratio if used

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
float prominence = 0.02;     // Threshold for peak detection (adjusted by pot)
int minPeakDistance = 300;   // Minimum time between peaks (adjusted by pot)
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

// Firebase configuration
#define FIREBASE_URL "https://your-firebase-url.firebaseio.com"  // Replace with your Firebase URL
#define FIREBASE_AUTH ""  // Replace with your Firebase auth token if needed
#define WIFI_SSID "your-wifi-ssid"  // Replace with your WiFi SSID
#define WIFI_PASSWORD "your-wifi-password"  // Replace with your WiFi password

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

// Struct to pass ECG data between tasks
typedef struct {
  int rawEcg;
  float filteredEcg;
  unsigned long timestamp;
} EcgData_t;

// Struct for BPM data
typedef struct {
  int fetalBpm;
  int maternalBpm;
  unsigned long timestamp;
} BpmData_t;

// Add this function before setup()
float getBatteryVoltage() {
  // Read raw value and convert to voltage
  int rawValue = analogRead(BATTERY_PIN);
  // ESP32 ADC reference voltage is 3.3V and resolution is 12-bit (0-4095)
  float voltage = (rawValue * 3.3 / 4095.0) * VOLTAGE_DIVIDER_RATIO;
  return voltage;
}

void checkBatteryStatus() {
  if (millis() - lastBatteryCheck >= BATTERY_CHECK_INTERVAL) {
    lastBatteryCheck = millis();
    
    if (xSemaphoreTake(batteryMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      batteryVoltage = getBatteryVoltage();
      xSemaphoreGive(batteryMutex);
    }
    
    // Print battery status
    Serial.print("Battery Voltage: ");
    Serial.print(batteryVoltage, 2);
    Serial.println("V");
  }
}

/**
 * Task for ECG signal acquisition and processing
 */
void ecgTask(void *parameter) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(5); // 200Hz sampling
  EcgData_t ecgData;
  
  while (1) {
    // Check for leads-off condition
    bool loPlus = digitalRead(LO_PLUS);
    bool loMinus = digitalRead(LO_MINUS);
    bool currentLeadsOff = (loPlus || loMinus);
    
    // Update leads connected status with debouncing
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
    
    // Only process ECG if leads are connected
    if (leadsConnected) {
      // Read and filter ECG signal
      int rawEcg = analogRead(AD8232_OUTPUT);
      
      // Take mutex for buffer access
      if (xSemaphoreTake(ecgMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        float filteredEcg = filterEcg(rawEcg);
        
        // Prepare data structure
        ecgData.rawEcg = rawEcg;
        ecgData.filteredEcg = filteredEcg;
        ecgData.timestamp = millis();
        
        // Send to processing queue
        xQueueSend(ecgDataQueue, &ecgData, 0);
        
        // Detect peaks and calculate BPM
        detectPeaksAndCalculateBpm(filteredEcg);
        
        xSemaphoreGive(ecgMutex);
      }
      
      // Prepare BPM data if needed
      if (xSemaphoreTake(bpmMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        if ((fetalBpm > 0 && millis() - lastFetalPeak < 5000) || 
            (maternalBpm > 0 && millis() - lastMaternalPeak < 5000)) {
          
          BpmData_t bpmData;
          bpmData.fetalBpm = fetalBpm;
          bpmData.maternalBpm = maternalBpm;
          bpmData.timestamp = millis();
          
          // Send to BPM queue (overwrite old data if queue is full)
          xQueueOverwrite(bpmDataQueue, &bpmData);
        }
        xSemaphoreGive(bpmMutex);
      }
    }
    
    // Wait for the next cycle using vTaskDelayUntil to ensure consistent timing
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

/**
 * Task for updating LEDs
 */
void ledTask(void *parameter) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(50); // 20Hz update rate
  
  while (1) {
    unsigned long currentMillis = millis();
    
    // Take BPM mutex to access heartbeat data
    if (xSemaphoreTake(bpmMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      // Update LED_FETAL based on fetal heartbeat
      if (millis() - lastFetalPeak < 150 && fetalBpm > 0) {
        digitalWrite(LED_FETAL, HIGH);
      } else {
        digitalWrite(LED_FETAL, LOW);
      }
      xSemaphoreGive(bpmMutex);
    }
    
    // Take battery mutex to access battery status
    if (xSemaphoreTake(batteryMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      // Update LED_WORKING based on device state priority
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
      xSemaphoreGive(batteryMutex);
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
    
    // Wait for the next cycle
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

/**
 * Task for monitoring battery status
 */
void batteryTask(void *parameter) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(5000); // Check every 5 seconds
  
  while (1) {
    if (xSemaphoreTake(batteryMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      batteryVoltage = getBatteryVoltage();
      
      // Print battery status
      Serial.print("Battery Voltage: ");
      Serial.print(batteryVoltage, 2);
      Serial.print("V (");
      if (batteryVoltage <= BATTERY_CRITICAL_THRESHOLD) {
        Serial.println("CRITICAL)");
      } else if (batteryVoltage <= BATTERY_LOW_THRESHOLD) {
        Serial.println("LOW)");
      } else {
        Serial.println("GOOD)");
      }
      
      xSemaphoreGive(batteryMutex);
    }
    
    // Wait for the next cycle
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

/**
 * Task for reading potentiometer and updating sensitivity
 */
void potentiometerTask(void *parameter) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(100); // 10Hz update rate
  
  while (1) {
    // Read potentiometer (0-4095)
    int newPotValue = analogRead(POTENTIOMETER_PIN);
    
    // Only update if significant change detected
    if (abs(newPotValue - potValue) > 10) {
      potValue = newPotValue;
      
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
      }
    }
    
    // Wait for the next cycle
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

/**
 * Task for sending data to Firebase
 */
void firebaseTask(void *parameter) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(1000); // Send every 1 second
  
  while (1) {
    // Try to receive the latest BPM data
    BpmData_t bpmData;
    if (xQueuePeek(bpmDataQueue, &bpmData, 0) == pdTRUE) {
      // Try to receive the latest ECG data
      EcgData_t ecgData;
      if (xQueueReceive(ecgDataQueue, &ecgData, 0) == pdTRUE) {
        // Only send if we have a valid fetal BPM
        if (bpmData.fetalBpm > 0 && millis() - bpmData.timestamp < 5000) {
          sendToFirebase(ecgData.rawEcg, ecgData.filteredEcg, ecgData.filteredEcg);
        }
      }
    }
    
    // Wait for the next cycle
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

/**
 * Task for displaying information on Serial
 */
void displayTask(void *parameter) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(1000); // Update every second
  
  while (1) {
    // Print detailed information to Serial Monitor
    Serial.println("\n--- Device Status ---");
    
    // Battery Status
    if (xSemaphoreTake(batteryMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
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
      xSemaphoreGive(batteryMutex);
    }
    
    // WiFi Status
    Serial.print("WiFi: ");
    Serial.println(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
    
    // ECG Status
    if (xSemaphoreTake(bpmMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      Serial.print("Fetal BPM: ");
      if (fetalBpm > 0 && millis() - lastFetalPeak < 5000) {
        Serial.print(fetalBpm);
        Serial.println(" ❤");
      } else {
        Serial.println("--");
      }
      xSemaphoreGive(bpmMutex);
    }
    
    Serial.print("Potentiometer: ");
    Serial.print(potValue);
    Serial.print("/4095 (");
    Serial.print(map(potValue, 0, 4095, 0, 100));
    Serial.println("%)");
    
    Serial.print("Signal Quality: ");
    if (pt_peak > 0) {
      float signalQuality = (pt_peak - pt_npk) / pt_peak * 100;
      Serial.print(signalQuality, 1);
      Serial.println("%");
    } else {
      Serial.println("--");
    }
    
    Serial.println("-------------------");
    
    // Wait for the next cycle
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  
  Serial.println("\n==== ECG BPM Test with FreeRTOS ====");
  Serial.println("Initializing device...");
  
  // Create mutex for protecting shared data
  ecgMutex = xSemaphoreCreateMutex();
  bpmMutex = xSemaphoreCreateMutex();
  batteryMutex = xSemaphoreCreateMutex();
  
  // Create queues for data exchange between tasks
  ecgDataQueue = xQueueCreate(10, sizeof(EcgData_t));
  bpmDataQueue = xQueueCreate(1, sizeof(BpmData_t)); // Only need latest BPM value
  
  // Initialize pins
  pinMode(AD8232_OUTPUT, INPUT);
  pinMode(POTENTIOMETER_PIN, INPUT);
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
  
  // Initialize WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  
  // Wait for WiFi with timeout to avoid blocking setup
  int wifiTimeout = 0;
  while (WiFi.status() != WL_CONNECTED && wifiTimeout < 20) {
    delay(500);
    Serial.print(".");
    wifiTimeout++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to WiFi");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFailed to connect to WiFi. Will retry in background.");
  }
  
  // Initial battery reading
  batteryVoltage = getBatteryVoltage();
  Serial.print("Initial battery level: ");
  Serial.print(batteryVoltage);
  Serial.println("V");
  
  // Create tasks
  xTaskCreatePinnedToCore(
    ecgTask,               // Function that implements the task
    "ECG_Task",            // Name of the task
    STACK_SIZE,            // Stack size in words
    NULL,                  // Parameter passed to the task
    ECG_TASK_PRIORITY,     // Priority at which the task is created
    &ecgTaskHandle,        // Task handle
    1                      // Core where the task should run (Core 1 - Application Core)
  );
  
  xTaskCreatePinnedToCore(
    ledTask,               // Function that implements the task
    "LED_Task",            // Name of the task
    2048,                  // Stack size in words
    NULL,                  // Parameter passed to the task
    LED_TASK_PRIORITY,     // Priority at which the task is created
    &ledTaskHandle,        // Task handle
    1                      // Core where the task should run (Core 1)
  );
  
  xTaskCreatePinnedToCore(
    batteryTask,           // Function that implements the task
    "Battery_Task",        // Name of the task
    2048,                  // Stack size in words
    NULL,                  // Parameter passed to the task
    BATTERY_TASK_PRIORITY, // Priority at which the task is created
    &batteryTaskHandle,    // Task handle
    0                      // Core where the task should run (Core 0 - Protocol Core)
  );
  
  xTaskCreatePinnedToCore(
    potentiometerTask,     // Function that implements the task
    "Potentiometer_Task",  // Name of the task
    2048,                  // Stack size in words
    NULL,                  // Parameter passed to the task
    POTENTIOMETER_TASK_PRIORITY, // Priority at which the task is created
    &potentiometerTaskHandle, // Task handle
    0                      // Core where the task should run (Core 0)
  );
  
  xTaskCreatePinnedToCore(
    firebaseTask,          // Function that implements the task
    "Firebase_Task",       // Name of the task
    STACK_SIZE,            // Stack size in words - larger for HTTP operations
    NULL,                  // Parameter passed to the task
    FIREBASE_TASK_PRIORITY, // Priority at which the task is created
    &firebaseTaskHandle,   // Task handle
    0                      // Core where the task should run (Core 0)
  );
  
  xTaskCreatePinnedToCore(
    displayTask,           // Function that implements the task
    "Display_Task",        // Name of the task
    2048,                  // Stack size in words
    NULL,                  // Parameter passed to the task
    1,                     // Priority at which the task is created
    NULL,                  // Task handle (not needed to store)
    0                      // Core where the task should run (Core 0)
  );
  
  Serial.println("All tasks created successfully");
  Serial.println("====================================");
}

void loop() {
  // Nothing to do here - all functionality is handled by RTOS tasks
  vTaskDelay(pdMS_TO_TICKS(1000)); // Prevent watchdog timer issues
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
 * Read potentiometer and update sensitivity parameters
 */
void updateSensitivityFromPot() {
  // Read potentiometer (0-4095)
  int newPotValue = analogRead(POTENTIOMETER_PIN);
  
  // Only update if significant change detected
  if (abs(newPotValue - potValue) > 10) {
    potValue = newPotValue;
    
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
      delay(30);
      digitalWrite(LED_FETAL, LOW);
      digitalWrite(LED_WORKING, LOW);
    }
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
  
  // Output potentiometer value (0-4095)
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
    
    Serial.print("Potentiometer: ");
    Serial.print(potValue);
    Serial.print("/4095 (");
    Serial.print(map(potValue, 0, 4095, 0, 100));
    Serial.println("%)");
    
    Serial.print("Signal Quality: ");
    if (pt_peak > 0) {
      float signalQuality = (pt_peak - pt_npk) / pt_peak * 100;
      Serial.print(signalQuality, 1);
      Serial.println("%");
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
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return "1970-01-01T00:00:00Z";
  }
  
  char timeStringBuff[50];
  strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
  return String(timeStringBuff);
}

bool sendToFirebase(int rawEcg, float fetalFiltered, float maternalFiltered) {
  // Don't send if fetalBpm is 0
  if (fetalBpm == 0) {
    return false;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ Wi-Fi disconnected. Trying to reconnect...");
    reconnectWiFi();
    return false;
  }

  String timestamp = getISOTimestamp();

  // Construct JSON
  String json = "{";
  json += "\"deviceId\":\"esp32\",";
  json += "\"bpm\":" + String(fetalBpm) + ",";
  json += "\"timestamp\":\"" + String(timestamp) + "\",";
  json += "\"rawEcg\":" + String(rawEcg) + ",";
  json += "\"smoothedEcg\":" + String(fetalFiltered);
  json += "}";

  String firebasePath = String(FIREBASE_URL) + "/readings.json";
  if (String(FIREBASE_AUTH).length() > 0) {
    firebasePath += "?auth=" + String(FIREBASE_AUTH);
  }

  HTTPClient http;
  http.begin(firebasePath);
  http.addHeader("Content-Type", "application/json");

  int httpResponseCode = http.PUT(json);

  if (httpResponseCode >= 200 && httpResponseCode < 300) {
    Serial.println("✅ Data sent to Firebase successfully!");
    Serial.println("BPM: " + String(fetalBpm));
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