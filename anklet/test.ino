#include <Arduino_BMI270_BMM150.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

#define BUTTON_SET D4
#define BUTTON_UP D2
#define BUTTON_DOWN D3

// Increment settings
#define WEIGHT_INCREMENT 0.5  // Weight changes by 0.5 kg per press
#define TIME_INCREMENT 1    // Time changes by 1 minute per press

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// State variables for button handling
float currentWeight = 60;  // Changed to float for decimal weight increments
int currentMinutes = 5;  // Default minutes
bool settingWeight = true;
bool settingTime = false;
bool exerciseStarted = false;

// Button debounce variables
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

int jumpCount = 0;

bool isJumping = false;

float speed = 0.0;

float totalSpeed = 0.0;

int speedReadings = 0;

unsigned long startTime;

unsigned long durationMillis;

bool timerSet = false;

int runningCount = 0;

float heightSum = 0;

int heightReadings = 0;

float userWeight = 0.0; // User input weight

const float heightThreshold = 0.5; // Increased threshold for better distinction

const float runningHeightThreshold = 0.7; // Added threshold for clearer running detection


const float jumpThreshold = 2.2;    // Increased threshold to reduce false jump detections

const float freeFallThreshold = 0.3; // Near zero gravity in Y-axis (free fall)

const float landingThreshold = 0.8;  // Detect landing impact

const float speedConversionFactor = 3.6; // Convert m/s to km/h

const int speedDetectionWindow = 5; // Number of readings for speed calculation

const unsigned long jumpDebounceTime = 300; // Minimum time between jumps (ms)

const float runningOscillationThreshold = 0.2; // Threshold for running motion consistency


float speedBuffer[speedDetectionWindow] = {0};

int speedIndex = 0;

unsigned long lastJumpTime = 0;


void updateDisplay() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0,0);
    
    if (!exerciseStarted) {
        if (settingWeight) {
            display.println("Set Weight (kg):");
            display.setTextSize(2);
            display.setCursor(40,20);
            display.println(currentWeight);
        } else if (settingTime) {
            display.println("Set Time (min):");
            display.setTextSize(2);
            display.setCursor(40,20);
            display.println(currentMinutes);
        }
        display.setTextSize(1);
        display.setCursor(0,50);
        display.println("SET:Next UP/DOWN:Adjust");
    } else {
        display.println("Exercise Stats:");
        display.print("Jumps: ");
        display.println(jumpCount);
        display.print("Speed: ");
        display.print(speed);
        display.println(" km/h");
        
        // Calculate remaining time
        unsigned long remainingTime = (durationMillis - (millis() - startTime)) / 1000;
        display.print("Time left: ");
        display.print(remainingTime / 60);
        display.print(":");
        display.println(remainingTime % 60);
    }
    display.display();
}

void handleButtons() {
    if (millis() - lastDebounceTime < debounceDelay) {
        return;
    }
    
    if (digitalRead(BUTTON_SET) == LOW) {
        lastDebounceTime = millis();
        if (settingWeight) {
            settingWeight = false;
            settingTime = true;
        } else if (settingTime) {
            settingTime = false;
            exerciseStarted = true;
            userWeight = currentWeight;
            durationMillis = currentMinutes * 60000;
            startTime = millis();
            timerSet = true;
        }
    }
    
    if (digitalRead(BUTTON_UP) == LOW) {
        lastDebounceTime = millis();
        if (settingWeight) {
            currentWeight += WEIGHT_INCREMENT;
        } else if (settingTime) {
            currentMinutes += TIME_INCREMENT;
        }
    }
    
    if (digitalRead(BUTTON_DOWN) == LOW) {
        lastDebounceTime = millis();
        if (settingWeight && currentWeight > 1) {
            currentWeight -= WEIGHT_INCREMENT;
        } else if (settingTime && currentMinutes > 1) {
            currentMinutes -= TIME_INCREMENT;
        }
    }
}

void setup() {
    Serial.begin(115200);
    
    // Initialize buttons
    pinMode(BUTTON_SET, INPUT_PULLUP);
    pinMode(BUTTON_UP, INPUT_PULLUP);
    pinMode(BUTTON_DOWN, INPUT_PULLUP);
    
    // Initialize OLED
    if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println("SSD1306 allocation failed");
        while(1);
    }
    display.clearDisplay();
    display.display();

    if (!IMU.begin()) {
        Serial.println("Failed to initialize IMU!");
        while (1);
    }
}

void loop() {
    if (!exerciseStarted) {
        handleButtons();
        updateDisplay();
        delay(50);
    } else {
        if (timerSet && (millis() - startTime < durationMillis)) {
            float ax, ay, az;
            
            if (IMU.accelerationAvailable()) {
                IMU.readAcceleration(ax, ay, az);

                float verticalAcceleration = abs(ay); // Up/Down movement
                float forwardAcceleration = abs(ax);  // Forward/Backward movement
                float height = abs(az); // Z-axis height approximation

                heightSum += height;
                heightReadings++;

                // **Detect Jump with Debouncing**
                if (verticalAcceleration > jumpThreshold && (millis() - lastJumpTime > jumpDebounceTime)) {
                    isJumping = true;
                    lastJumpTime = millis();
                }

                if (isJumping && verticalAcceleration < freeFallThreshold) {
                    Serial.println("In the air...");
                }

                if (isJumping && verticalAcceleration < landingThreshold && (millis() - lastJumpTime > 200)) {
                    jumpCount++;
                    isJumping = false;
                    Serial.print("Jump detected! Total Jumps: ");
                    Serial.println(jumpCount);
                }

                // **Running Detection Improvement**
                if (!isJumping && forwardAcceleration > runningOscillationThreshold && height > runningHeightThreshold) {
                    runningCount++;
                }

                // **Running Speed Calculation**
                speedBuffer[speedIndex] = forwardAcceleration * speedConversionFactor; // Convert acceleration to speed approximation
                speedIndex = (speedIndex + 1) % speedDetectionWindow;
                
                float avgSpeed = 0;
                for (int i = 0; i < speedDetectionWindow; i++) {
                    avgSpeed += speedBuffer[i];
                }
                avgSpeed /= speedDetectionWindow;

                speed = avgSpeed;
                totalSpeed += speed;
                speedReadings++;
                
                Serial.print("Running Speed: ");
                Serial.print(speed);
                Serial.println(" km/h");
                
                updateDisplay();  // Update display with current stats
            }
            delay(50);
        } else if (timerSet) {
            // Exercise completed
            display.clearDisplay();
            display.setTextSize(1);
            display.setTextColor(SSD1306_WHITE);
            display.setCursor(0,0);
            display.println("Exercise Complete!");
            display.print("Total Jumps: ");
            display.println(jumpCount);
            display.print("Avg Speed: ");
            display.print(totalSpeed / (speedReadings > 0 ? speedReadings : 1));
            display.println(" km/h");
            display.print("Calories: ");
            display.print((MET * 3.5 * userWeight / 200) * (durationMillis / 60000));
            display.display();
            
            delay(5000);  // Show results for 5 seconds
            
            // Reset for next session
            exerciseStarted = false;
            settingWeight = true;
            timerSet = false;
        }
    }
}