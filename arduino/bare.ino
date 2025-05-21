void setup() {
  // initialize the serial communication:
  Serial.begin(115200);  // Increased baud rate for better performance
  pinMode(16, INPUT); // Setup for leads off detection LO +
  pinMode(17, INPUT); // Setup for leads off detection LO -
  
  // ESP32 ADC configuration
  analogReadResolution(12); // Set ADC resolution to 12 bits
  analogSetAttenuation(ADC_11db); // Set ADC attenuation for 0-3.3V range
}

void loop() {
  
  if((digitalRead(16) == 1)||(digitalRead(17) == 1)){
    Serial.println(0);  // Send 0 when leads are off
  }
  else{
    // send the value of analog input from pin 5:
    Serial.println(analogRead(5));  // Using GPIO5 for ECG signal input
  }
  //Wait for a bit to keep serial data from saturating
  delay(1);
}