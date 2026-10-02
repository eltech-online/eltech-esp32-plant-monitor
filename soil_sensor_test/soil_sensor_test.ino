// ElTech-Online ESP32 Plant Monitor — soil sensor test
//
// The smallest possible sketch for the capacitive soil moisture sensor: it
// reads the sensor once a second and prints the voltage to Serial Monitor.
// Nothing else is needed — no display, no WiFi, no extra libraries.
//
// Use it to:
//   - check the sensor is wired correctly before building the full project
//   - see for yourself how the voltage changes between dry and wet
//   - find your own sensor's "dry" and "wet" numbers
//
// Wiring (3 wires):
//   sensor VCC  -> ESP32-C3 3V3
//   sensor GND  -> ESP32-C3 GND
//   sensor AOUT -> ESP32-C3 GPIO 3
//
// How to use:
//   1. Upload this sketch and open Tools > Serial Monitor at 115200 baud.
//   2. Hold the sensor in the air. Write down the number: that's "dry".
//   3. Stand its flat end in a glass of water (keep the electronics at the
//      cable end out of the water). Write down the number: that's "wet".
//   The dry number should be clearly HIGHER than the wet number.

// The pin the sensor's signal wire is connected to. It must be an
// analog-capable pin: on the ESP32-C3 that's GPIO 0-4.
#define SOIL_PIN 3

// setup() runs once, when the board is powered on or reset.
void setup() {
  // Start the connection to the computer, at 115200 bits per second. Serial
  // Monitor must be set to the same speed or the text comes out garbled.
  Serial.begin(115200);

  // Tell the ESP32 to measure voltages up to about 2.5 V on this pin (its
  // widest range). Without this it would still work, but naming it makes clear
  // what range we're using.
  analogSetPinAttenuation(SOIL_PIN, ADC_11db);

  Serial.println("Soil sensor test - readings in millivolts (mV)");
}

// loop() runs over and over, forever.
void loop() {
  // Measure the voltage on the pin. The answer is in millivolts:
  // 1000 mV = 1 volt.
  int millivolts = analogReadMilliVolts(SOIL_PIN);

  // The same measurement as the raw number the ESP32's converter produces:
  // 0 (no voltage) to 4095 (the top of its range). Many tutorials use this
  // raw number; millivolts are easier to understand and to check with a
  // multimeter.
  int raw = analogRead(SOIL_PIN);

  Serial.print("Sensor: ");
  Serial.print(millivolts);
  Serial.print(" mV   (raw ");
  Serial.print(raw);
  Serial.println(")");

  delay(1000);   // wait 1 second (1000 milliseconds), then loop() runs again
}
