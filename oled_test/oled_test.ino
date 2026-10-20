// ElTech-Online ESP32 Plant Monitor — OLED display test
//
// The smallest sketch for the 1.3" SH1106 OLED: it lists every part answering
// on the I2C bus, then draws a frame, some text and a counter on the screen.
// No sensors, no WiFi.
//
// Use it to:
//   - check the OLED is wired correctly before building the full project
//   - see which I2C addresses are in use (the OLED is usually 0x3C, the light
//     sensor 0x23), which is handy if a part isn't found
//
// Libraries needed (Arduino IDE Library Manager): Adafruit SH110X and
// Adafruit GFX Library. If it asks to install Adafruit BusIO too, click "Install all".
//
// Wiring (4 wires):
//   OLED VDD -> ESP32-C3 3V3
//   OLED GND -> ESP32-C3 GND
//   OLED SCK -> ESP32-C3 GPIO 9   (SCK is this display's name for SCL)
//   OLED SDA -> ESP32-C3 GPIO 8
//
// How to use:
//   1. Upload this sketch and open Tools > Serial Monitor at 115200 baud.
//   2. Serial Monitor lists the I2C addresses found; the screen shows a frame,
//      "OLED test OK" and a number counting up once a second.
//   Blank screen but 0x3D in the list? Change OLED_ADDR below to 0x3D.

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

#define I2C_SDA 8
#define I2C_SCL 9
#define OLED_ADDR 0x3C          // the usual address; some displays use 0x3D
#define SCREEN_WIDTH 128        // the OLED is 128 pixels wide...
#define SCREEN_HEIGHT 64        // ...and 64 pixels tall

// The "display" object stands for the real screen. -1 = no reset pin.
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
bool oledOK = false;
int counter = 0;

// Knocks on every possible I2C address (1-126) and prints the ones that answer.
void scanI2C() {
  Serial.println("I2C devices found:");
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {   // 0 means "something answered"
      Serial.print("  0x");
      if (addr < 16) Serial.print("0");
      Serial.println(addr, HEX);
      found++;
    }
  }
  if (found == 0) Serial.println("  none - check VDD, GND, SDA (GPIO 8) and SCK (GPIO 9)");
}

// setup() runs once, when the board is powered on or reset.
void setup() {
  // Serial Monitor must be set to the same speed (115200) or the text comes out garbled.
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);
  delay(500);   // give Serial Monitor a moment to connect

  Serial.println("OLED (SH1106) test");
  scanI2C();

  oledOK = display.begin(OLED_ADDR, true);
  if (!oledOK) {
    Serial.println("OLED NOT FOUND at the address in OLED_ADDR.");
    return;
  }
  Serial.println("OLED OK - look at the screen");
  // Text must be given a colour, or nothing is drawn. On this one-colour
  // display, WHITE means "pixel on".
  display.setTextColor(SH110X_WHITE);
}

// loop() runs over and over, forever.
void loop() {
  if (!oledOK) {
    delay(1000);
    return;
  }
  // Draw the whole picture in memory first, then send it with display.display().
  display.clearDisplay();
  display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SH110X_WHITE);   // a frame round the edge
  display.setTextSize(1);
  display.setCursor(28, 8);
  display.print("OLED test OK");
  display.setTextSize(3);       // 3 times bigger
  display.setCursor(40, 28);
  display.print(counter);
  display.display();

  counter = counter + 1;
  if (counter > 99) counter = 0;   // two digits fit; start again after 99
  delay(1000);
}
