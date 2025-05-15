/*
 * ESP32-S ECG Monitor with Battery Management and Firebase - RTOS Version
 * 
 * This code implements ECG monitoring, battery monitoring, LED status indicators,
 * and Firebase data sending using FreeRTOS tasks for better resource management
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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

// Task handles
TaskHandle_t ecgTaskHandle = NULL;
TaskHandle_t wifiTaskHandle = NULL;
TaskHandle_t batteryTaskHandle = NULL;
TaskHandle_t ledTaskHandle = NULL;
TaskHandle_t displayTaskHandle = NULL;

// Queue handles
QueueHandle_t ecgDataQueue;
QueueHandle_t bpmQueue;
QueueHandle_t batteryQueue;

// Mutex for shared resources
SemaphoreHandle_t wifiMutex;
SemaphoreHandle_t serialMutex;

// WiFi and Firebase Configuration
const char* WIFI_SSID = "BatStateU-DevOps";  // Replace with your WiFi SSID
const char* WIFI_PASSWORD = "Dev3l$06";  // Replace with your WiFi password
const String FIREBASE_URL = "https://ecgdata-f042a-default-rtdb.asia-southeast1.firebasedatabase.app/ecg_readings.json";
const String FIREBASE_AUTH = "AIzaSyA0OGrnWnNx0LDPGzDZHdrzajiRGEjr3AM";

// NTP Configuration for timestamps
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 0;

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
const float MIN_PROMINENCE = 0.001;   // Minimum peak prominence
const float MAX_PROMINENCE = 0.1;    // Maximum peak prominence
const int MIN_DISTANCE_MS = 150;     // Minimum distance between peaks
const int MAX_DISTANCE_MS = 2000;    // Maximum distance between peaks

// BPM Range Constants
const int MIN_VALID_BPM = 20;      // Minimum valid BPM
const int MAX_VALID_BPM = 250;     // Maximum valid BPM
const int MIN_MATERNAL_BPM = 30;   // Minimum maternal heart rate
const int MAX_MATERNAL_BPM = 120;  // Maximum maternal heart rate
const int MIN_FETAL_BPM = 90;     // Minimum fetal heart rate
const int MAX_FETAL_BPM = 220;     // Maximum fetal heart rate

// Global variables
float prominence = 0.02;
int minPeakDistance = 300;
int fetalBpm = 0;
float batteryVoltage = 0.0;
bool leadsConnected = false;
bool wifiConnected = false;

// Battery monitoring constants
#define BATTERY_CHECK_INTERVAL 5000
#define BATTERY_WARNING_THRESHOLD 1.5
#define BATTERY_MAX_VOLTAGE 2.0
#define VOLTAGE_DIVIDER_RATIO 0.32

// Structs for data passing between tasks
struct ECGData {
    int rawEcg;
    float filteredEcg;
    unsigned long timestamp;
};

struct BPMData {
    int bpm;
    bool isFetal;
    unsigned long timestamp;
};

// Function declarations
void setupWiFi();
float filterEcg(int rawEcg);
bool panTompkinsQRS(float input_data);
float getBatteryVoltage();
bool checkLeadsOff();

// Task functions
void ecgTask(void *parameter);
void wifiTask(void *parameter);
void batteryTask(void *parameter);
void ledTask(void *parameter);
void displayTask(void *parameter);

void setup() {
    Serial.begin(115200);
    
    // Initialize pins
    pinMode(AD8232_OUTPUT, INPUT);
    pinMode(BATTERY_PIN, INPUT);
    pinMode(LED_FETAL, OUTPUT);
    pinMode(LED_WORKING, OUTPUT);
    pinMode(LED_WIFI, OUTPUT);
    pinMode(LO_PLUS, INPUT);
    pinMode(LO_MINUS, INPUT);
    
    // Configure ADC
    analogSetWidth(12);
    analogSetPinAttenuation(BATTERY_PIN, ADC_11db);
    analogSetPinAttenuation(AD8232_OUTPUT, ADC_11db);
    
    // Create queues
    ecgDataQueue = xQueueCreate(10, sizeof(ECGData));
    bpmQueue = xQueueCreate(5, sizeof(BPMData));
    batteryQueue = xQueueCreate(2, sizeof(float));
    
    // Create mutex
    wifiMutex = xSemaphoreCreateMutex();
    serialMutex = xSemaphoreCreateMutex();
    
    // Create tasks
    xTaskCreatePinnedToCore(
        ecgTask,
        "ECG Task",
        8192,
        NULL,
        3, // High priority
        &ecgTaskHandle,
        1  // Core 1
    );
    
    xTaskCreatePinnedToCore(
        wifiTask,
        "WiFi Task",
        8192,
        NULL,
        2, // Medium priority
        &wifiTaskHandle,
        0  // Core 0
    );
    
    xTaskCreatePinnedToCore(
        batteryTask,
        "Battery Task",
        2048,
        NULL,
        1, // Low priority
        &batteryTaskHandle,
        0  // Core 0
    );
    
    xTaskCreatePinnedToCore(
        ledTask,
        "LED Task",
        2048,
        NULL,
        1, // Low priority
        &ledTaskHandle,
        0  // Core 0
    );
    
    xTaskCreatePinnedToCore(
        displayTask,
        "Display Task",
        4096,
        NULL,
        1, // Low priority
        &displayTaskHandle,
        0  // Core 0
    );
    
    // Initial LED test
    testLEDs();
    
    if (xSemaphoreTake(serialMutex, portMAX_DELAY)) {
        Serial.println("\n==== ESP32-S ECG Monitor with Firebase (RTOS) ====");
        Serial.println("System initialized and tasks created");
        Serial.println("=============================================");
        xSemaphoreGive(serialMutex);
    }
}

void loop() {
    // Empty loop - everything is handled by tasks
    vTaskDelay(portMAX_DELAY);
}

// ECG Task - Highest priority, runs on Core 1
void ecgTask(void *parameter) {
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(5); // 200Hz sampling rate
    
    // Initialize the xLastWakeTime variable with the current time
    xLastWakeTime = xTaskGetTickCount();
    
    while(1) {
        // Check leads connection
        leadsConnected = !checkLeadsOff();
        
        if (leadsConnected) {
            // Read and process ECG
            int rawEcg = analogRead(AD8232_OUTPUT);
            float filteredEcg = filterEcg(rawEcg);
            
            // Create ECG data structure
            ECGData ecgData = {
                .rawEcg = rawEcg,
                .filteredEcg = filteredEcg,
                .timestamp = millis()
            };
            
            // Send to queue
            xQueueSend(ecgDataQueue, &ecgData, 0);
            
            // Process QRS detection
            if (panTompkinsQRS(filteredEcg)) {
                // Calculate BPM and classify as maternal/fetal
                unsigned long currentTime = millis();
                static unsigned long lastPeak = 0;
                
                if (lastPeak > 0) {
                    unsigned long interval = currentTime - lastPeak;
                    int newBpm = 60000 / interval;
                    
                    if (newBpm >= MIN_VALID_BPM && newBpm <= MAX_VALID_BPM) {
                        BPMData bpmData = {
                            .bpm = newBpm,
                            .isFetal = (newBpm >= MIN_FETAL_BPM && newBpm <= MAX_FETAL_BPM),
                            .timestamp = currentTime
                        };
                        
                        xQueueSend(bpmQueue, &bpmData, 0);
                    }
                }
                lastPeak = currentTime;
            }
        }
        
        // Wait for the next cycle
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

// WiFi Task - Medium priority, runs on Core 0
void wifiTask(void *parameter) {
    const TickType_t xDelay = pdMS_TO_TICKS(1000); // Check every second
    
    while(1) {
        if (xSemaphoreTake(wifiMutex, portMAX_DELAY)) {
            if (WiFi.status() != WL_CONNECTED) {
                setupWiFi();
            }
            wifiConnected = (WiFi.status() == WL_CONNECTED);
            xSemaphoreGive(wifiMutex);
        }
        
        if (wifiConnected) {
            // Check if there's data to send
            ECGData ecgData;
            if (xQueueReceive(ecgDataQueue, &ecgData, 0) == pdTRUE) {
                // Get current time
                struct tm timeinfo;
                if (getLocalTime(&timeinfo)) {
                    char timestamp[30];
                    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", &timeinfo);
                    
                    // Create JSON
                    String json = "{";
                    json += "\"deviceId\":\"esp32\",";
                    json += "\"bpm\":" + String(fetalBpm) + ",";
                    json += "\"timestamp\":\"" + String(timestamp) + "\",";
                    json += "\"rawEcg\":" + String(ecgData.rawEcg) + ",";
                    json += "\"smoothedEcg\":" + String(ecgData.filteredEcg);
                    json += "}";
                    
                    // Send to Firebase
                    HTTPClient http;
                    http.begin(FIREBASE_URL + "?auth=" + FIREBASE_AUTH);
                    http.addHeader("Content-Type", "application/json");
                    
                    int httpResponseCode = http.PUT(json);
                    
                    if (httpResponseCode > 0) {
                        if (xSemaphoreTake(serialMutex, portMAX_DELAY)) {
                            Serial.println("✅ Data sent to Firebase");
                            xSemaphoreGive(serialMutex);
                        }
                    }
                    
                    http.end();
                }
            }
        }
        
        vTaskDelay(xDelay);
    }
}

// Battery Task - Low priority, runs on Core 0
void batteryTask(void *parameter) {
    const TickType_t xDelay = pdMS_TO_TICKS(BATTERY_CHECK_INTERVAL);
    
    while(1) {
        float voltage = getBatteryVoltage();
        xQueueOverwrite(batteryQueue, &voltage); // Only keep latest reading
        batteryVoltage = voltage; // Update global variable
        
        vTaskDelay(xDelay);
    }
}

// LED Task - Low priority, runs on Core 0
void ledTask(void *parameter) {
    const TickType_t xDelay = pdMS_TO_TICKS(50); // Update LEDs every 50ms
    
    while(1) {
        // Update LED_WORKING (Red) based on battery voltage
        if (batteryVoltage <= BATTERY_WARNING_THRESHOLD) {  // 1.5V threshold
            digitalWrite(LED_WORKING, (xTaskGetTickCount() / 500) % 2); // Blink at 1Hz
        } else {
            digitalWrite(LED_WORKING, LOW); // Off when battery is good
        }
        
        // Update LED_WIFI (Yellow) based on WiFi status
        if (!wifiConnected) {
            digitalWrite(LED_WIFI, (xTaskGetTickCount() / 500) % 2); // Blink at 1Hz when disconnected
        } else {
            digitalWrite(LED_WIFI, LOW); // Off when connected
        }
        
        // Update LED_FETAL (Green) based on ECG reading
        ECGData ecgData;
        if (xQueuePeek(ecgDataQueue, &ecgData, 0) == pdTRUE) {
            if (millis() - ecgData.timestamp < 100) { // If reading is recent (within last 100ms)
                digitalWrite(LED_FETAL, HIGH);
            } else {
                digitalWrite(LED_FETAL, LOW);
            }
        } else {
            digitalWrite(LED_FETAL, LOW);
        }
        
        vTaskDelay(xDelay);
    }
}

// Display Task - Lowest priority, runs on Core 0
void displayTask(void *parameter) {
    const TickType_t xDelay = pdMS_TO_TICKS(1000); // Update display every second
    
    while(1) {
        if (xSemaphoreTake(serialMutex, portMAX_DELAY)) {
            Serial.println("\n--- Device Status ---");
            
            // Display BPM
            Serial.print("Fetal BPM: ");
            BPMData bpmData;
            if (xQueuePeek(bpmQueue, &bpmData, 0) == pdTRUE && bpmData.isFetal) {
                if (millis() - bpmData.timestamp < 5000) {
                    Serial.print(bpmData.bpm);
                    Serial.println(" 👶");
                } else {
                    Serial.println("--");
                }
            } else {
                Serial.println("--");
            }
            
            // Display Battery
            Serial.print("Battery: ");
            Serial.print(batteryVoltage, 2);
            Serial.print("V (");
            Serial.print((batteryVoltage / BATTERY_MAX_VOLTAGE) * 100, 1);
            Serial.println("%)");
            
            // Display WiFi Status
            Serial.print("WiFi: ");
            Serial.println(wifiConnected ? "Connected" : "Disconnected");
            
            Serial.println("-------------------");
            
            xSemaphoreGive(serialMutex);
        }
        
        vTaskDelay(xDelay);
    }
}

// Utility Functions
void testLEDs() {
    for (int i = 0; i < 3; i++) {
        digitalWrite(LED_FETAL, HIGH);
        digitalWrite(LED_WORKING, HIGH);
        digitalWrite(LED_WIFI, HIGH);
        vTaskDelay(pdMS_TO_TICKS(200));
        digitalWrite(LED_FETAL, LOW);
        digitalWrite(LED_WORKING, LOW);
        digitalWrite(LED_WIFI, LOW);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void setupWiFi() {
    if (xSemaphoreTake(serialMutex, portMAX_DELAY)) {
        Serial.print("Connecting to WiFi");
        xSemaphoreGive(serialMutex);
    }
    
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        vTaskDelay(pdMS_TO_TICKS(500));
        if (xSemaphoreTake(serialMutex, portMAX_DELAY)) {
            Serial.print(".");
            xSemaphoreGive(serialMutex);
        }
        attempts++;
    }
    
    if (xSemaphoreTake(serialMutex, portMAX_DELAY)) {
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\n✅ Connected to WiFi");
            Serial.print("IP: ");
            Serial.println(WiFi.localIP());
        } else {
            Serial.println("\n❌ Failed to connect to WiFi");
        }
        xSemaphoreGive(serialMutex);
    }
    
    // Configure time
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

float getBatteryVoltage() {
    const int numReadings = 10;
    int32_t adcTotal = 0;
    
    for(int i = 0; i < numReadings; i++) {
        adcTotal += analogRead(BATTERY_PIN);
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    
    int rawValue = adcTotal / numReadings;
    float voltage = (float)rawValue / 4095.0 * 3.3;
    voltage = voltage * 1.1 / VOLTAGE_DIVIDER_RATIO;
    
    return voltage;
}

bool checkLeadsOff() {
    return (digitalRead(LO_PLUS) || digitalRead(LO_MINUS));
}

// Note: The filterEcg and panTompkinsQRS functions remain the same as in the original code
// They are omitted here for brevity but should be included in the final implementation 