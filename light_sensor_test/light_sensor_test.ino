// ElTech-Online ESP32 Plant Monitor — light sensor (BH1750) test
//
// The smallest sketch for the BH1750 light sensor: it finds the sensor on the
// I2C bus and prints the light level once a second. No display, no WiFi, and
// no library: the sensor only needs a couple of one-byte commands.
//
// Use it to:
//   - check the light sensor is wired correctly before building the full project
//   - see how the light level changes: cover the sensor, then shine a torch on it
//   - work out your own dome correction (see "Light correction" in the README)
//
// Wiring (4 wires; leave the ADDR wire unconnected):
//   sensor VCC -> ESP32-C3 3V3
//   sensor GND -> ESP32-C3 GND
//   sensor SDA -> ESP32-C3 GPIO 8
//   sensor SCL -> ESP32-C3 GPIO 9
//
// How to use:
//   1. Upload this sketch and open Tools > Serial Monitor at 115200 baud.
//   2. It prints which address the sensor answered at, then a reading every
//      second. A room is roughly 100-500 lux; daylight by a window is thousands.

#include <Wire.h>   // the ESP32's built-in I2C library

// I2C pins: the ESP32-C3 SuperMini's labeled SDA and SCL pins.
#define I2C_SDA 8
#define I2C_SCL 9

// The BH1750 answers at address 0x23, or at 0x5C if its ADDR wire is
// connected to 3.3V. This sketch tries both.
#define LIGHT_ADDR      0x23
#define LIGHT_ADDR_ALT  0x5C

// The white dome blocks some light. The full project multiplies by this
// factor to correct for it (bare sensor 180 lux, dome 120 lux: 180 / 120 = 1.5).
// This sketch prints the reading both without and with it.
#define DOME_FACTOR 1.5

uint8_t lightAddr = 0;   // the address that answered (0 = none found yet)

// Switches the sensor on and starts it measuring continuously.
// Returns true if a sensor answered at that address.
bool startLightSensor(uint8_t addr) {
  Wire.beginTransmission(addr);
  Wire.write(0x01);                                // command 0x01: power on
  if (Wire.endTransmission() != 0) return false;   // 0 means "it answered"
  Wire.beginTransmission(addr);
  Wire.write(0x10);                                // command 0x10: keep measuring, high resolution
  return Wire.endTransmission() == 0;
}

// setup() runs once, when the board is powered on or reset.
void setup() {
  // Serial Monitor must be set to the same speed (115200) or the text comes out garbled.
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);
  delay(500);   // give Serial Monitor a moment to connect

  Serial.println("Light sensor (BH1750) test");
  if (startLightSensor(LIGHT_ADDR)) {
    lightAddr = LIGHT_ADDR;
  } else if (startLightSensor(LIGHT_ADDR_ALT)) {
    lightAddr = LIGHT_ADDR_ALT;
  }

  if (lightAddr == 0) {
    Serial.println("Light sensor NOT FOUND at 0x23 or 0x5C.");
    Serial.println("Check VCC, GND, and that SDA is on GPIO 8 and SCL on GPIO 9.");
  } else {
    Serial.print("Light sensor found at address 0x");
    Serial.println(lightAddr, HEX);
  }
  delay(180);   // the first measurement takes up to 180 ms
}

// loop() runs over and over, forever.
void loop() {
  if (lightAddr == 0) {   // nothing found: there's nothing to read
    delay(1000);
    return;
  }

  // Ask the sensor for 2 bytes: its latest measurement.
  if (Wire.requestFrom(lightAddr, (uint8_t)2) != 2) {
    Serial.println("No answer from the light sensor - check the wiring");
    delay(1000);
    return;
  }
  // The two bytes make one number from 0 to 65535. The first byte is the
  // "big" half, so shift it 8 places left and join the second byte on.
  int raw = (Wire.read() << 8) | Wire.read();
  float lux = raw / 1.2;   // the datasheet says: divide by 1.2 to get lux

  Serial.print("Raw: ");
  Serial.print(raw);
  Serial.print("   Light: ");
  Serial.print(lux, 0);
  Serial.print(" lx   With dome correction: ");
  Serial.print(lux * DOME_FACTOR, 0);
  Serial.println(" lx");

  delay(1000);   // wait 1 second, then loop() runs again
}
