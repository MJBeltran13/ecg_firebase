/*
 * AD8232 ECG Sensor Reading
 * This sketch reads data from the AD8232 ECG sensor and checks for lead-off detection
 */

#define AD8232_OUTPUT 34     // Output pin from AD8232
#define LO_PLUS 16         // LO+ pin for lead-off detection
#define LO_MINUS 17        // LO- pin for lead-off detection

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  
  // Setup pins
  pinMode(AD8232_OUTPUT, INPUT); // Setup AD8232 output pin
  pinMode(LO_PLUS, INPUT);      // Setup LO+ pin
  pinMode(LO_MINUS, INPUT);     // Setup LO- pin
  
  Serial.println("AD8232 ECG Monitor Test");
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