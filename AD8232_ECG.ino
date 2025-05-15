/*
 * AD8232 ECG Sensor Reading for ESP32 (NodeMCU-32S)
 * This sketch reads data from the AD8232 ECG sensor and checks for lead-off detection
 */

#define AD8232_OUTPUT 34    // GPIO34 (ADC1_6) - Analog input pin from AD8232
#define LO_PLUS 16         // GPIO16 - LO+ pin for lead-off detection
#define LO_MINUS 17        // GPIO17 - LO- pin for lead-off detection

void setup() {
  // Initialize serial communication
  Serial.begin(115200);    // ESP32 usually uses 115200 baud rate
  
  // Setup pins
  pinMode(LO_PLUS, INPUT);      // Setup LO+ pin
  pinMode(LO_MINUS, INPUT);     // Setup LO- pin
  // No need to set pinMode for ADC pin on ESP32
  
  // Set ADC characteristics (for better resolution)
  analogReadResolution(12);     // Set ADC resolution to 12 bits (0-4095)
  analogSetAttenuation(ADC_11db);  // Set ADC attenuation for 3.3V range
  
  Serial.println("AD8232 ECG Monitor Test (ESP32)");
}

void loop() {
  // Check if leads are attached
  if((digitalRead(LO_PLUS) == 1) || (digitalRead(LO_MINUS) == 1)) {
    Serial.println("!");
  }
  else {
    // Read the output from AD8232
    int ecgValue = analogRead(AD8232_OUTPUT);
    Serial.println(ecgValue);
  }
  
  // Small delay to prevent overwhelming serial output
  delay(10);
} 