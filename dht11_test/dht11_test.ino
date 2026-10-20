// ElTech-Online ESP32 Plant Monitor — DHT11 test
//
// The smallest sketch for the DHT11 temperature/humidity module: it reads the
// sensor every 2 seconds and prints the temperature and humidity to Serial
// Monitor. No display, no WiFi.
//
// Use it to:
//   - check the DHT11 is wired correctly before building the full project
//   - see how the readings change when you breathe on the sensor
//
// Library needed (Arduino IDE Library Manager): DHT sensor library (by Adafruit).
// If it asks to install Adafruit Unified Sensor too, click "Install all".
//
// Wiring (3 wires):
//   DHT11 VCC  -> ESP32-C3 3V3
//   DHT11 GND  -> ESP32-C3 GND
//   DHT11 DATA -> ESP32-C3 GPIO 10
//
// How to use:
//   1. Upload this sketch and open Tools > Serial Monitor at 115200 baud.
//   2. You should see a room temperature (about 15-30 C) and humidity
//      (about 30-70 %). Breathe gently on the sensor: the humidity goes up.
//   "Failed to read" every time means a wiring problem: check DATA is on GPIO 10.

#include <DHT.h>   // the library that does the DHT11's pulse timing for us

// The pin the sensor's DATA wire is connected to. Any free digital pin works
// except GPIO 2, 8 and 9, which the ESP32-C3 also uses to decide how to start up.
#define DHT_PIN  10
#define DHT_TYPE DHT11   // the library also handles the DHT22, so say which one

// This line creates the "dht" object: it stands for the real sensor, and we
// ask it for readings with a dot, like dht.readTemperature().
DHT dht(DHT_PIN, DHT_TYPE);

// setup() runs once, when the board is powered on or reset.
void setup() {
  // Start the connection to the computer, at 115200 bits per second. Serial
  // Monitor must be set to the same speed or the text comes out garbled.
  Serial.begin(115200);
  dht.begin();
  Serial.println("DHT11 test - temperature and humidity every 2 seconds");
}

// loop() runs over and over, forever.
void loop() {
  // The DHT11 can only take a new measurement about once a second, so wait
  // 2 seconds between readings (2000 milliseconds).
  delay(2000);

  float humidity = dht.readHumidity();          // in %
  float temperature = dht.readTemperature();    // in degrees C

  // If the sensor didn't answer, the library hands back NAN ("not a number").
  // isnan() checks for that. A single miss now and then is normal.
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Failed to read from the DHT11 - check the wiring");
    return;   // skip the rest of loop() this time round
  }

  Serial.print("Temperature: ");
  Serial.print(temperature, 1);   // 1 = one decimal place
  Serial.print(" C   Humidity: ");
  Serial.print(humidity, 0);      // 0 = whole number
  Serial.println(" %");
}
