#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>

// --- Wi-Fi Configuration ---
const char *ssid = "WIFI";
const char *password = "Password";

// --- Firebase Configuration ---
const char *firebaseUrl = "https://ecgdata-f042a-default-rtdb.asia-southeast1.firebasedatabase.app/ecg_readings.json";
const char *firebaseAuth = "AIzaSyA0OGrnWnNx0LDPGzDZHdrzajiRGEjr3AM";
// NTP Server for time synchronization
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0; // adjust based on your timezone
const int   daylightOffset_sec = 0; // adjust based on daylight saving

// --- Pin Definitions ---
#define AD8232_OUTPUT 35 // AD8232 sensor output to ESP32 ADC pin
#define LO_PLUS 33       // LO+ pin (lead-off detection)
#define LO_MINUS 32      // LO– pin (lead-off detection)
#define LED_BATTERY 25   // Red LED for battery status
#define LED_WORKING 26   // Green LED for device operation
#define LED_WIFI 27      // Yellow LED for WiFi status
#define BATTERY_PIN 34   // Analog pin for battery voltage measurement

// Battery voltage thresholds for 8V battery
#define BATTERY_CRITICAL 0.3 // Critical threshold

// Voltage divider ratio (R1 = 5k, R2 = 20k)
// Adjusted ratio to match actual measurements
#define VOLTAGE_DIVIDER_RATIO 1.11

// --- Filtering and BPM Calculation ---
const int filterWindow = 20; // Increased window size for better smoothing
int ecgBuffer[filterWindow]; // Array to store recent ECG values
int ecgIndex = 0;
long ecgSum = 0;

int bpmArray[10]; // Array to store the most recent 10 BPM values
int bpmIndex = 0;
long previousTime = 0;
bool pulseDetected = false;

// Function to get formatted timestamp
String getFormattedTime() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return "2023-06-01T12:30:45"; // Return a default timestamp as fallback
  }
  char timeStringBuff[30];
  strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%dT%H:%M:%S", &timeinfo);
  return String(timeStringBuff);
}

void setup()
{
    Serial.begin(115200);
    Serial.println("Starting ESP32...");

    // Setup LED pins
    pinMode(LED_BATTERY, OUTPUT);
    pinMode(LED_WIFI, OUTPUT);
    pinMode(LED_WORKING, OUTPUT);

    // Initialize LEDs
    digitalWrite(LED_BATTERY, LOW);
    digitalWrite(LED_WIFI, LOW);
    digitalWrite(LED_WORKING, LOW);

    // Setup battery monitoring
    pinMode(BATTERY_PIN, INPUT);

    // Connect to Wi-Fi
    WiFi.begin(ssid, password);
    Serial.print("Connecting to Wi-Fi");
    unsigned long startAttemptTime = millis();
    const unsigned long wifiTimeout = 30000; // 30 seconds timeout

    // Wait for connection with a timeout
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < wifiTimeout)
    {
        delay(500);
        Serial.print(".");
    }
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("\nFailed to connect to Wi-Fi!");
    }
    else
    {
        Serial.println("\nConnected to Wi-Fi!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        
        // Initialize and get the time
        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    }

    // Setup sensor pins
    pinMode(AD8232_OUTPUT, INPUT);
    pinMode(LO_PLUS, INPUT);
    pinMode(LO_MINUS, INPUT);

    // Initialize buffers
    for (int i = 0; i < filterWindow; i++)
    {
        ecgBuffer[i] = 0;
    }
    for (int i = 0; i < 10; i++)
    {
        bpmArray[i] = -1;
    }

    Serial.println("Waiting for heartbeat...");
}

void loop()
{
    // Check for lead-off detection first
    if (digitalRead(LO_PLUS) == 1 || digitalRead(LO_MINUS) == 1)
    {
        resetBPMArray();
        digitalWrite(LED_WORKING, LOW); // Turn off green LED when leads are off
        Serial.println("Lead-off detected");
        delay(500);
        return;
    }
    else
    {
        digitalWrite(LED_WORKING, HIGH); // Turn on green LED when leads are connected
    }

    // Update WiFi LED status with different patterns for different states
    static unsigned long lastWifiBlinkTime = 0;
    static bool wifiLedState = false;
    unsigned long currentWifiTime = millis();

    if (WiFi.status() == WL_CONNECTED)
    {
        digitalWrite(LED_WIFI, HIGH); // Solid on when connected
    }
    else
    {
        // Blink (500ms) when not connected
        if (currentWifiTime - lastWifiBlinkTime > 500)
        {
            lastWifiBlinkTime = currentWifiTime;
            wifiLedState = !wifiLedState;
            digitalWrite(LED_WIFI, wifiLedState);
        }
    }

    // Check battery status every second
    static unsigned long lastBatteryCheck = 0;
    if (millis() - lastBatteryCheck > 1000)
    {
        lastBatteryCheck = millis();
        float batteryVoltage = getBatteryVoltage();
        updateBatteryLED(batteryVoltage);

        // Print battery status to serial
        Serial.print("Battery Status: ");
        Serial.print(batteryVoltage, 2); // Print with 2 decimal places
        Serial.print("V - ");
        if (batteryVoltage < BATTERY_CRITICAL)
        {
            Serial.println("CRITICAL - Battery needs replacement!");
        }
        else
        {
            Serial.println("Good");
        }
    }

    // Read raw ECG value and normalize around 0
    int ecgValue = analogRead(AD8232_OUTPUT);
    int normalizedEcg = ecgValue - 512;

    // Apply moving average filter
    ecgSum -= ecgBuffer[ecgIndex];
    ecgBuffer[ecgIndex] = normalizedEcg;
    ecgSum += normalizedEcg;
    ecgIndex = (ecgIndex + 1) % filterWindow;
    int smoothedEcg = ecgSum / filterWindow;

    // Adaptive baseline and threshold calculation
    static int baseline = 0;
    baseline = (baseline * 9 + smoothedEcg) / 10;
    int peakThreshold = baseline + 30;

    // Debug log for raw and smoothed ECG data
    Serial.print("Raw ECG: ");
    Serial.print(ecgValue);
    Serial.print(" | Smoothed ECG: ");
    Serial.println(smoothedEcg);

    // Heartbeat detection and BPM calculation
    unsigned long currentBeatTime = millis();
    if (smoothedEcg > peakThreshold && !pulseDetected)
    {
        pulseDetected = true;
        long timeBetweenBeats = currentBeatTime - previousTime;
        previousTime = currentBeatTime;

        // Consider valid heartbeats between 600 and 1200 ms (approx. 60–100 BPM)
        if (timeBetweenBeats > 600 && timeBetweenBeats < 1200)
        {
            int bpm = 60000 / timeBetweenBeats;
            addBpmToArray(bpm);
            int averageBPM = getAverageBPM();
            if (averageBPM >= 60 && averageBPM <= 100)
            {
                Serial.print("BPM: ");
                Serial.println(averageBPM);
                Serial.print("ECG: ");
                Serial.print(smoothedEcg);
                Serial.print(" | Time Between Beats: ");
                Serial.println(timeBetweenBeats);

                // Send the data if Wi-Fi is connected
                if (WiFi.status() == WL_CONNECTED)
                {
                    HTTPClient http;
                    // Add auth parameter to the URL
                    String fullUrl = String(firebaseUrl) + "?auth=" + String(firebaseAuth);
                    http.begin(fullUrl);
                    http.addHeader("Content-Type", "application/json");

                    // Generate timestamp for data
                    String timestamp = getFormattedTime();
                    
                    // Create JSON payload matching the exact format required
                    String payload = "{\"deviceId\": \"esp32\", \"bpm\": " + String(averageBPM) + 
                                    ", \"timestamp\": \"" + timestamp + 
                                    "\", \"rawEcg\": " + String(ecgValue) + 
                                    "\", \"smoothedEcg\": " + String(smoothedEcg) + "}";
                                    
                    int httpResponseCode = http.PUT(payload);
                    if (httpResponseCode > 0)
                    {
                        String response = http.getString();
                        Serial.print("Firebase Response: ");
                        Serial.println(response);
                    }
                    else
                    {
                        Serial.print("Error sending data to Firebase, HTTP code: ");
                        Serial.println(httpResponseCode);
                    }
                    http.end();
                }
            }
        }
    }

    if (smoothedEcg < peakThreshold)
    {
        pulseDetected = false;
    }

    delay(10);
}

void addBpmToArray(int bpm)
{
    bpmArray[bpmIndex] = bpm;
    bpmIndex = (bpmIndex + 1) % 10;
}

int getAverageBPM()
{
    int sum = 0, count = 0;
    for (int i = 0; i < 10; i++)
    {
        if (bpmArray[i] != -1)
        {
            sum += bpmArray[i];
            count++;
        }
    }
    return count > 0 ? sum / count : 0;
}

void resetBPMArray()
{
    for (int i = 0; i < 10; i++)
    {
        bpmArray[i] = -1;
    }
    bpmIndex = 0;
}

float getBatteryVoltage()
{
    // Read the analog value (0-4095 for ESP32)
    int rawValue = analogRead(BATTERY_PIN);

    // Convert to actual voltage
    // ESP32 ADC reference voltage is 3.3V
    float voltage = (rawValue * 3.3 / 4095.0) * VOLTAGE_DIVIDER_RATIO;

    return voltage;
}

void updateBatteryLED(float voltage)
{
    static unsigned long lastBlinkTime = 0;
    static bool ledState = false;
    unsigned long currentTime = millis();

    if (voltage < BATTERY_CRITICAL)
    {
        // Battery is critical - LED blinking every 500ms
        if (currentTime - lastBlinkTime > 500)
        {
            lastBlinkTime = currentTime;
            ledState = !ledState;
            digitalWrite(LED_BATTERY, ledState);
        }
    }
    else
    {
        // Battery is good - LED off
        digitalWrite(LED_BATTERY, LOW);
    }
}