/*
 * Potentiometer Test Sketch
 * 
 * This sketch reads a potentiometer connected to pin 14 of an ESP32
 * and displays the values on the Serial Monitor.
 * 
 * Hardware connections:
 * - Connect potentiometer middle pin to ESP32 pin 14
 * - Connect one outer pin of potentiometer to 3.3V
 * - Connect other outer pin of potentiometer to GND
 * - Optionally connect an LED (with a 220 ohm resistor) to pin 25 for visual feedback
 * 
 */

// Pin definitions
#define POTENTIOMETER_PIN 14  // Analog pin connected to potentiometer
#define LED_PIN 25            // Optional LED for visual feedback

// Variables for potentiometer reading
int currentPotValue = 0;      // Current potentiometer value
int lastPotValue = 0;         // Previous potentiometer value
unsigned long lastPrintTime = 0;  // Time of last serial print
unsigned long lastLedUpdate = 0;  // Time of last LED update

// Constants for determining significant changes
const int THRESHOLD = 10;     // Minimum change to be considered significant
const int PRINT_INTERVAL = 200; // Print interval in milliseconds

void setup() {
  // Initialize Serial communication
  Serial.begin(115200);
  delay(500);  // Allow serial to initialize
  
  // Initialize the potentiometer pin as an input
  pinMode(POTENTIOMETER_PIN, INPUT);
  
  // Initialize the LED pin as an output
  pinMode(LED_PIN, OUTPUT);
  
  // Print header
  Serial.println();
  Serial.println("=================================");
  Serial.println("    Potentiometer Test Sketch    ");
  Serial.println("=================================");
  Serial.println("Reading potentiometer on pin 14");
  Serial.println("Turn the potentiometer to see values change");
  Serial.println();
  
  // Print column headers
  Serial.println("Raw Value | Percentage | Voltage (V)");
  Serial.println("---------|-----------|-----------");
}

void loop() {
  // Read the potentiometer value (ESP32 has 12-bit ADC: 0-4095)
  currentPotValue = analogRead(POTENTIOMETER_PIN);
  
  // Calculate percentage and voltage
  int percentage = map(currentPotValue, 0, 4095, 0, 100);
  float voltage = currentPotValue * (3.3 / 4095.0);
  
  // Check for significant change or time interval
  bool significantChange = abs(currentPotValue - lastPotValue) > THRESHOLD;
  bool timeToUpdate = millis() - lastPrintTime >= PRINT_INTERVAL;
  
  // Print values if there's a significant change or periodically
  if (significantChange || timeToUpdate) {
    // Update timing
    lastPrintTime = millis();
    lastPotValue = currentPotValue;
    
    // Print values in columns
    Serial.print(currentPotValue);
    Serial.print("\t| ");
    Serial.print(percentage);
    Serial.print("%\t| ");
    Serial.print(voltage, 2);
    Serial.println("V");
    
    // Visual representation of potentiometer position
    printBar(percentage);
  }
  
  // Update LED brightness based on potentiometer value
  // (only update periodically to reduce flickering)
  if (millis() - lastLedUpdate >= 50) {
    lastLedUpdate = millis();
    
    // Map the pot value to LED brightness (ESP32 PWM range is 0-255)
    int brightness = map(currentPotValue, 0, 4095, 0, 255);
    analogWrite(LED_PIN, brightness);
  }
  
  // Small delay to reduce readings
  delay(10);
}

// Function to print a visual bar representing the percentage
void printBar(int percentage) {
  // Only print the bar every 5 seconds or on significant change (>5%)
  static int lastPercentagePrinted = -10;
  static unsigned long lastBarPrintTime = 0;
  
  if (abs(percentage - lastPercentagePrinted) > 5 || millis() - lastBarPrintTime > 5000) {
    lastPercentagePrinted = percentage;
    lastBarPrintTime = millis();
    
    // Create a visual bar representation
    Serial.print("[");
    int barLength = map(percentage, 0, 100, 0, 20);
    
    for (int i = 0; i < 20; i++) {
      if (i < barLength) {
        Serial.print("#");
      } else {
        Serial.print("-");
      }
    }
    
    Serial.println("]");
    Serial.println(); // Extra line for readability
  }
} 