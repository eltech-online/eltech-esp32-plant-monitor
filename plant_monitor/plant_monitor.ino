// ElTech-Online ESP32 Plant Monitor — soil moisture + air temperature/humidity,
// with an OLED readout and a WiFi Access Point + web dashboard
//
// Three sensors, three different ways of talking to a microcontroller:
//   - Capacitive soil moisture sensor -> an ANALOG voltage, read with the ADC
//   - DHT11 temperature/humidity      -> a ONE-WIRE DIGITAL signal (timed pulses)
//   - SH1106 OLED display             -> the I2C bus (two shared wires)
//
// The board starts its own WiFi network (an Access Point, no router needed) and
// serves a live-updating webpage with the readings to anything that connects to
// it — a phone, laptop, whatever. The same page has the buttons that calibrate
// the soil sensor.
//
// Libraries needed (Arduino IDE Library Manager):
//   DHT sensor library (by Adafruit)
//   Adafruit SH110X
//   Adafruit GFX Library
// (Library Manager also offers Adafruit BusIO and Adafruit Unified Sensor as
// dependencies — click "Install all". Tested versions are listed in the README.)
// (WiFi and WebServer are built into the ESP32 board package — no separate install)
// Board package: esp32 by Espressif Systems
//
// How to use:
//   1. Flash this sketch.
//   2. The OLED and Serial Monitor show this board's WiFi network name and
//      password. Connect your phone/laptop to that network.
//   3. Open a browser to http://192.168.4.1 (also shown on the OLED and Serial).
//   4. The page updates every 2 seconds on its own — no need to refresh.
//   5. Calibrate the soil sensor once, from the page: "Set dry" with the sensor
//      in the air, "Set wet" with it standing in a glass of water.
// Full source, wiring diagram and setup guide: github.com/eltech-online/eltech-esp32-plant-monitor
//
// ---------------------------------------------------------------------------
// New to Arduino code? How to read this file
// ---------------------------------------------------------------------------
// Lines starting with // are comments: notes for people, ignored by the board.
// The file is in this order, and you can read it top to bottom:
//   1. Settings      - pin numbers and limits you can safely change
//   2. Soil sensor   - reading the voltage and turning it into a percentage
//   3. Web server    - what the board sends to your phone's browser
//   4. WiFi          - starting the board's own WiFi network
//   5. setup()       - runs ONCE when the board is powered on
//   6. loop()        - then runs over and over, forever
//   7. OLED screen   - drawing the readings on the display
//   8. Self-test     - checking every part works at power-on
// A good first experiment: change DRY_BELOW_PCT below, upload, and watch the
// "DRY" warning appear at a different moisture level.

// #include pulls in a library: ready-made code for one job, so we don't have
// to write (for example) the OLED's low-level commands ourselves.
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>  // saves the WiFi password and the calibration in flash
#include <Wire.h>
#include <DHT.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include "logo_bitmap.h"    // shop logo bitmap for the OLED splash screen
#include "page_template.h"  // the web dashboard's HTML page (used by handleRoot() below)

// ---- WiFi Access Point settings ----
// Leave both empty ("") and every board gets its OWN network name, made from its
// unique hardware (MAC) address, e.g. "ElTech-PM-A3F2", and its OWN random
// 8-character password. The password is created on first boot and saved in
// flash, so it stays the same after every reboot and re-flash. Both are shown on
// the OLED and in Serial Monitor.
// Or type your own in: the name can be up to 32 characters (only the first 21
// fit on the OLED) and the password must be 8-63 characters.
const char* AP_SSID     = "";
const char* AP_PASSWORD = "";
const bool  AP_OPEN_NETWORK = false;  // true = no password at all (anyone nearby can join)

// #define gives a number a name. Everywhere the code says SCREEN_WIDTH, the
// compiler reads 128. Names make the code readable, and a value only has to be
// changed in one place.
#define SCREEN_WIDTH 128    // the OLED is 128 pixels wide...
#define SCREEN_HEIGHT 64    // ...and 64 pixels tall
#define OLED_ADDR    0x3C   // the OLED's I2C address. Common default; try 0x3D if blank

// GPIO 8/9 are the ESP32-C3 SuperMini's labeled I2C pins. I2C needs two wires:
// SDA carries the data, SCL carries the clock that keeps both ends in step.
#define I2C_SDA 8
#define I2C_SCL 9

// The soil sensor's signal wire. It must be an analog-capable pin: on the
// ESP32-C3 that's GPIO 0-4. (GPIO 5 can also read analog, but not while WiFi is on.)
#define SOIL_PIN 3

// The DHT11's data wire. Any free digital pin works except GPIO 2, 8 and 9,
// which the ESP32-C3 also uses to decide how to start up.
#define DHT_PIN  10
#define DHT_TYPE DHT11

// ---- Soil sensor calibration ----
// The sensor gives a HIGHER voltage when dry and a LOWER one when wet. The exact
// numbers differ a little from sensor to sensor, so the two end points are
// measured once ("Set dry" / "Set wet" on the web page) and saved in flash.
// These defaults are only used until you've done that.
// ("const int" = a whole number that never changes. mV = millivolts,
// thousandths of a volt: 2300 mV is 2.3 V.)
const int DEFAULT_DRY_MV = 2300;   // sensor in the air  -> shown as 0 %
const int DEFAULT_WET_MV = 1000;   // sensor in water    -> shown as 100 %
// Dry and wet must be at least this far apart, or the calibration is refused
// as a mistake (e.g. both buttons pressed with the sensor in the same place).
const int MIN_CAL_SPAN_MV = 300;

// What the moisture percentage means for the plant. Change to suit yours:
// a cactus is happy far drier than a fern.
const int DRY_BELOW_PCT = 30;   // below this: "DRY" — time to water
const int WET_ABOVE_PCT = 70;   // above this: "WET" — hold off watering

// Self-test ranges. A reading outside these almost always means a faulty or
// badly wired sensor, so the self-test fails it.
const float TEMP_MIN_C  = 0.0, TEMP_MAX_C  = 50.0;   // the DHT11's own working range
const float HUM_MIN_PCT = 5.0, HUM_MAX_PCT = 95.0;
const int   SOIL_MIN_MV = 300, SOIL_MAX_MV = 3000;

// These three lines create the "objects" the libraries give us. Each one stands
// for a real thing, and we tell it what to do with a dot: dht.readHumidity(),
// display.print("hello"), server.send(...).
DHT dht(DHT_PIN, DHT_TYPE);                                        // the DHT11 sensor
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);  // the OLED screen
WebServer server(80);                                              // the web server (port 80 = normal web port)

// Variables declared up here, outside any function, are "global": every
// function below can read and change them. "bool" is a true/false value.
// These two remember whether each part answered, so the rest of the code can
// skip a missing part instead of crashing.
bool oledOK = false, dhtOK = false;

// Set by runSelfTest(): true only if the sensor answered AND its first reading
// was inside the ranges above.
bool dhtPassed = false, soilPassed = false;
bool selfTestPassed = false;

// Each part's self-test result as text ("OK", "NOT FOUND", "BAD READING"),
// plus the first reading it took — kept so the web dashboard can show them too.
String dhtStatus = "NOT FOUND", soilStatus = "BAD READING";
String dhtDetail, soilDetail;

// The network name/password actually in use (from the settings above, or
// generated), and whether the Access Point started.
String apSsid, apPassword, apUrl;
bool wifiOK = false;
String wifiError;

// The calibration in use, and whether it came from flash or from the defaults.
int dryMv = DEFAULT_DRY_MV, wetMv = DEFAULT_WET_MV;
bool calSaved = false;

// The latest readings. "float" is a number with a decimal point, "int" is a
// whole number. NAN means "not a number" — a float that has no value yet.
float g_temperature = NAN, g_humidity = NAN;
int g_soilMv = 0;        // soil sensor voltage, in millivolts
int dhtFailCount = 0;    // how many DHT11 readings in a row have failed

// These lines just announce functions that are written further down the file,
// so the code above them is allowed to use them.
void centerText(const String& text, int y, int textSize);
bool runSelfTest();
void showSelfTestFailure();
void drawDataScreen();

// ---------------------------------------------------------------------------
// Soil sensor
// ---------------------------------------------------------------------------

// Reads the soil sensor's voltage in millivolts.
//
// analogReadMilliVolts() uses the ESP32's ADC (analog-to-digital converter) to
// measure the voltage on a pin. A single reading jumps around a little, so
// this takes 16 readings, adds them up, and returns the average — a simple way
// to get a steadier number.
int readSoilMillivolts() {
  const int samples = 16;
  long total = 0;
  for (int i = 0; i < samples; i++) {         // repeat 16 times
    total += analogReadMilliVolts(SOIL_PIN);  // add this reading to the total
    delay(2);                                 // wait 2 ms between readings
  }
  return total / samples;                     // total / 16 = the average
}

// True if the dry and wet points are far enough apart to be believable.
bool calibrationValid() {
  return dryMv - wetMv >= MIN_CAL_SPAN_MV;
}

// Turns a voltage into 0-100 %: the dry point is 0 %, the wet point is 100 %,
// and anything in between is scaled in a straight line.
//
// Worked example with dry = 2300 mV, wet = 1000 mV and a reading of 1650 mV:
//   how far from dry are we?   2300 - 1650 =  650 mV
//   how far is dry from wet?   2300 - 1000 = 1300 mV
//   650 / 1300 = 0.5  ->  50 %
int soilPercent(int mv) {
  if (!calibrationValid()) return 0;
  float pct = 100.0 * (dryMv - mv) / (dryMv - wetMv);
  // Round to a whole number, and keep it between 0 and 100 even if the soil is
  // a little drier or wetter than the calibration points.
  return constrain((int)lround(pct), 0, 100);
}

// Turns the percentage into a word for the screen.
String soilStatusText(int pct) {
  if (!calibrationValid()) return "CAL?";
  if (pct < DRY_BELOW_PCT) return "DRY";
  if (pct > WET_ABOVE_PCT) return "WET";
  return "OK";
}

// Reads the saved dry/wet points back from flash memory at power-on.
//
// Preferences is the ESP32's small "notebook" that survives the power being
// turned off. Values are stored under a name (a "key"), here "dry_mv" and
// "wet_mv". The -1 is what getInt() hands back if nothing was ever saved.
void loadCalibration() {
  Preferences prefs;
  prefs.begin("plant", true);   // open the notebook called "plant" (true = read only)
  int savedDry = prefs.getInt("dry_mv", -1);
  int savedWet = prefs.getInt("wet_mv", -1);
  prefs.end();
  calSaved = savedDry > 0 || savedWet > 0;
  if (savedDry > 0) dryMv = savedDry;
  if (savedWet > 0) wetMv = savedWet;
}

// Saves the current reading as the dry or the wet point ("dry" / "wet"), or
// goes back to the defaults ("reset"). Returns a message saying what happened.
String calibrate(const String& point) {
  Preferences prefs;
  if (point == "reset") {
    prefs.begin("plant", false);
    prefs.remove("dry_mv");
    prefs.remove("wet_mv");
    prefs.end();
    dryMv = DEFAULT_DRY_MV;
    wetMv = DEFAULT_WET_MV;
    calSaved = false;
    return "Calibration reset to the defaults.";
  }
  if (point != "dry" && point != "wet") return "Unknown calibration point.";

  // Take a reading right now. Whatever the sensor is touching at this moment
  // (air, or water) becomes the new dry or wet point.
  int mv = readSoilMillivolts();
  if (mv < SOIL_MIN_MV || mv > SOIL_MAX_MV) {
    return "Reading of " + String(mv) + " mV looks wrong. Check the sensor wiring.";
  }
  prefs.begin("plant", false);
  if (point == "dry") { dryMv = mv; prefs.putInt("dry_mv", mv); }
  else                { wetMv = mv; prefs.putInt("wet_mv", mv); }
  prefs.end();
  calSaved = true;

  String msg = (point == "dry" ? "Dry" : "Wet");
  msg += " point saved: " + String(mv) + " mV.";
  if (!calibrationValid()) {
    msg += " Dry and wet are too close together. Now set the other point.";
  }
  return msg;
}

// ---------------------------------------------------------------------------
// Web server
// ---------------------------------------------------------------------------

// A browser asks the board for an address ("/" or "/data"), and the matching
// function below sends the answer. setup() connects each address to its
// function with server.on(...).

// "/" -> the dashboard page itself (the HTML lives in page_template.h).
void handleRoot() {
  // No-cache headers: stops your browser reusing an old cached page after you
  // reflash the sketch, since the ESP32 always serves from the same IP.
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
  server.send(200, "text/html", PAGE_TEMPLATE);
}

// One self-test row as a JSON array: ["name","status","detail"].
String selfTestRow(const String& name, const String& status, const String& detail) {
  return "[\"" + name + "\",\"" + status + "\",\"" + detail + "\"]";
}

// "/data" -> just the current readings, as JSON. JSON is a simple text format
// that programs can read easily. The page's JavaScript asks for it every 2
// seconds. What this function sends looks like:
//   {"soil":"62 %","status":"OK","soil_mv":1494,"temp":"21.4 °C","hum":"48 %", ...}
// In the code below, \" puts a real quote mark inside the text, and += adds
// more text onto the end of what's already there.
void handleData() {
  int pct = soilPercent(g_soilMv);

  // First work out the text to show for each reading.
  String soilText = "--";
  if (calibrationValid()) soilText = String(pct) + " %";

  String tempText = "n/a", humText = "n/a";
  if (dhtOK) {
    tempText = String(g_temperature, 1) + " °C";   // 1 = one decimal place
    humText  = String(g_humidity, 0) + " %";       // 0 = whole number
  }

  // Then build the JSON text piece by piece.
  String json = "{";
  json += "\"soil\":\"" + soilText + "\",";
  json += "\"status\":\"" + soilStatusText(pct) + "\",";
  json += "\"soil_mv\":" + String(g_soilMv) + ",";
  json += "\"temp\":\"" + tempText + "\",";
  json += "\"hum\":\""  + humText + "\",";

  json += "\"cal\":{\"dry\":" + String(dryMv) + ",\"wet\":" + String(wetMv);
  json += ",\"saved\":" + String(calSaved ? "true" : "false");
  json += ",\"valid\":" + String(calibrationValid() ? "true" : "false") + "},";

  // The power-on self-test results, so you can see them without Serial Monitor.
  json += "\"selftest\":{\"result\":\"" + String(selfTestPassed ? "PASS" : "FAIL") + "\",\"parts\":[";
  json += selfTestRow("OLED (SH1106)", oledOK ? "OK" : "NOT FOUND", "") + ",";
  json += selfTestRow("DHT11", dhtStatus, dhtDetail) + ",";
  json += selfTestRow("Soil sensor", soilStatus, soilDetail) + ",";
  json += selfTestRow("WiFi AP", wifiOK ? "OK" : "FAILED", wifiOK ? "" : wifiError);
  json += "]}}";
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
  server.send(200, "application/json", json);
}

// The page's calibration buttons send a POST to /calibrate?point=dry (or wet,
// or reset). POST rather than GET because it changes something on the board.
void handleCalibrate() {
  String msg = calibrate(server.arg("point"));
  Serial.println("Calibration: " + msg);
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
  server.send(200, "application/json", "{\"msg\":\"" + msg + "\"}");
}

// ---------------------------------------------------------------------------
// WiFi Access Point
// ---------------------------------------------------------------------------

// "ElTech-PM-" + the last 4 hex digits of this board's MAC address. Every ESP32
// has a different MAC, so two kits in the same room never clash.
String makeUniqueSsid() {
  uint8_t mac[6];
  WiFi.softAPmacAddress(mac);
  char name[20];
  snprintf(name, sizeof(name), "ElTech-PM-%02X%02X", mac[4], mac[5]);
  return String(name);
}

// Loads this board's saved password, or creates and saves a random one on first
// boot. Uses lowercase letters and digits only, minus look-alikes (i, l, o, 0, 1),
// so it's easy to read off the OLED and type on a phone. To get a new one, set
// Tools > "Erase All Flash Before Sketch Upload" to Enabled and upload once
// (that also clears the soil sensor calibration).
String loadOrCreatePassword() {
  Preferences prefs;
  prefs.begin("plant", false);
  String password = prefs.getString("ap_password", "");
  if (password.length() < 8) {
    const char alphabet[] = "abcdefghjkmnpqrstuvwxyz23456789";
    password = "";
    for (int i = 0; i < 8; i++) {
      password += alphabet[esp_random() % (sizeof(alphabet) - 1)];
    }
    prefs.putString("ap_password", password);
  }
  prefs.end();
  return password;
}

// Starts the Access Point and returns true if it worked. On failure, wifiError
// says why — instead of silently printing a URL for a network that doesn't exist.
bool startAccessPoint() {
  // Turning WiFi on first also powers up the radio, which esp_random() uses as a
  // source of true randomness for the generated password.
  WiFi.mode(WIFI_AP);

  apSsid = strlen(AP_SSID) ? String(AP_SSID) : makeUniqueSsid();
  if (AP_OPEN_NETWORK) {
    apPassword = "";
  } else {
    apPassword = strlen(AP_PASSWORD) ? String(AP_PASSWORD) : loadOrCreatePassword();
  }

  if (apSsid.length() > 32) {
    wifiError = "name over 32 chars";   // error texts fit one OLED line (21 chars)
    return false;
  }
  if (!AP_OPEN_NETWORK && (apPassword.length() < 8 || apPassword.length() > 63)) {
    wifiError = "pass not 8-63 chars";
    return false;
  }
  if (!WiFi.softAP(apSsid.c_str(), AP_OPEN_NETWORK ? NULL : apPassword.c_str())) {
    wifiError = "softAP() failed";
    return false;
  }
  // The ESP32-C3 SuperMini's tiny antenna can't handle full transmit power:
  // the signal gets so distorted that phones can't see or join the network.
  // Lowering it to 8.5 dBm fixes this and is still plenty for a room.
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  return true;
}

// ---------------------------------------------------------------------------
// setup() runs once at power-on, loop() then runs forever
// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);

  oledOK = display.begin(OLED_ADDR, true);
  dht.begin();
  // Measure up to about 2.5 V on the soil pin (the ESP32-C3's widest range).
  analogSetPinAttenuation(SOIL_PIN, ADC_11db);
  loadCalibration();

  // Required — without this the driver's default text color doesn't exactly
  // match any of its defined color constants, so every text pixel write
  // silently no-ops while drawLine/drawBitmap (which pass SH110X_WHITE
  // explicitly themselves) still render fine.
  if (oledOK) {
    display.setTextColor(SH110X_WHITE);
    // Clip text that's too wide instead of wrapping it onto the next line, so a
    // long custom WiFi name/password can't spill over the readings below it.
    display.setTextWrap(false);
  }

  Serial.println("========================================");
  Serial.println("           ElTech-Online");
  Serial.println("   ESP32 Plant Monitor (WiFi AP mode)");
  Serial.println("========================================");

  // Start the Access Point first, so the self-test can report whether it worked.
  wifiOK = startAccessPoint();
  IPAddress ip = WiFi.softAPIP();
  apUrl = "http://" + ip.toString();

  // Self-test: check this in Serial Monitor to confirm everything is wired right.
  selfTestPassed = runSelfTest();

  Serial.println("--- WiFi Access Point ---");
  if (wifiOK) {
    Serial.print("SSID:     "); Serial.println(apSsid);
    Serial.print("Password: "); Serial.println(AP_OPEN_NETWORK ? "(open network)" : apPassword.c_str());
    Serial.print("URL:      http://"); Serial.println(ip);
  } else {
    Serial.print("FAILED to start: "); Serial.println(wifiError);
  }

  Serial.println("--- Soil calibration ---");
  Serial.printf("Dry point: %d mV, wet point: %d mV (%s)\n", dryMv, wetMv,
                calSaved ? "saved" : "defaults, not calibrated yet");
  Serial.println("Type d = set dry, w = set wet, r = reset (or use the web page)");

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/calibrate", HTTP_POST, handleCalibrate);
  server.begin();

  if (oledOK) {
    display.clearDisplay();
    display.drawBitmap((SCREEN_WIDTH - LOGO_WIDTH) / 2, 0, logo_bmp, LOGO_WIDTH, LOGO_HEIGHT, SH110X_WHITE);
    centerText("ElTech-Online", 36, 1);
    centerText("Plant Monitor", 48, 1);
    display.display();
    delay(2000);

    if (!selfTestPassed) showSelfTestFailure();

    // Show the WiFi connection details full-screen once at boot. They also stay
    // visible on the live data screen afterwards (see drawDataScreen()).
    if (wifiOK) {
      display.clearDisplay();
      centerText("Connect to WiFi:", 0, 1);
      centerText(apSsid, 12, 1);
      centerText(AP_OPEN_NETWORK ? String("(open network)") : "Pass: " + apPassword, 24, 1);
      centerText("then open:", 36, 1);
      centerText(apUrl, 48, 1);
      display.display();
      delay(6000);
    }
  }
}

void loop() {
  // Answer any browser that's waiting. This has to be called very often, which
  // is why there is no long delay() anywhere in loop().
  server.handleClient();

  // Calibration from Serial Monitor: type d, w or r and press Enter.
  // Serial.available() says how many typed characters are waiting to be read.
  while (Serial.available()) {
    char c = Serial.read();   // take the next character
    if (c == 'd') Serial.println("Calibration: " + calibrate("dry"));
    if (c == 'w') Serial.println("Calibration: " + calibrate("wet"));
    if (c == 'r') Serial.println("Calibration: " + calibrate("reset"));
  }

  // Read the sensors every 2 seconds WITHOUT stopping the web server.
  // millis() is the number of milliseconds since the board started. Instead of
  // pausing with delay(2000), we note the time of the last reading and only do
  // the next one once 2000 ms have gone by. ("static" makes lastRead keep its
  // value between one run of loop() and the next.)
  static unsigned long lastRead = 0;
  if (millis() - lastRead >= 2000) {
    lastRead = millis();

    g_soilMv = readSoilMillivolts();

    // The DHT11 sends its data as a train of precisely timed pulses, and now and
    // then one gets missed (the library then returns "not a number", NaN). So a
    // single failed read keeps the last good values; only several in a row mean
    // the sensor has really gone.
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    bool goodReading = !isnan(h) && !isnan(t);   // isnan() = "is this not a number?"
    if (goodReading) {
      g_humidity = h;
      g_temperature = t;
      dhtFailCount = 0;
      dhtOK = true;
    } else {
      dhtFailCount = dhtFailCount + 1;
      if (dhtFailCount >= 5) dhtOK = false;      // 5 misses in a row: show "n/a"
    }

    int pct = soilPercent(g_soilMv);
    Serial.println("----------------------------------------");
    Serial.print("  Soil moisture : ");
    if (calibrationValid()) Serial.printf("%3d %%  %s\n", pct, soilStatusText(pct).c_str());
    else Serial.println("needs calibrating");
    Serial.printf("  Soil sensor   : %4d mV\n", g_soilMv);
    Serial.print("  Temperature   : ");
    if (dhtOK) Serial.printf("%5.1f C\n", g_temperature); else Serial.println("  n/a");
    Serial.print("  Humidity      : ");
    if (dhtOK) Serial.printf("%5.0f %%\n", g_humidity); else Serial.println("  n/a");

    if (oledOK) drawDataScreen();
  }
}

// The live data screen:
//
//   y=0   WiFi: ElTech-PM-A3F2      <- network name
//   y=9   Pass: abcd2345            <- password
//   y=20      62%                   <- big headline soil moisture (text size 3)
//   y=46  Soil: OK       22C 48%    <- status, then air temperature + humidity
//   y=56  http://192.168.4.1        <- the address to open in your browser
//
// Positions are in pixels: x counts across from the left edge (0-127), y counts
// DOWN from the top edge (0-63). Nothing appears on the screen until
// display.display() at the end sends the finished picture in one go.
void drawDataScreen() {
  display.clearDisplay();

  if (wifiOK) {
    String nameLine = "WiFi: " + apSsid;
    centerText(nameLine.length() <= 21 ? nameLine : apSsid, 0, 1);
    String passLine = AP_OPEN_NETWORK ? String("Open network") : "Pass: " + apPassword;
    centerText(passLine.length() <= 21 ? passLine : apPassword, 9, 1);
  } else {
    centerText("WiFi FAILED", 0, 1);
    centerText(wifiError, 9, 1);
  }
  display.drawLine(0, 17, SCREEN_WIDTH, 17, SH110X_WHITE);

  int pct = soilPercent(g_soilMv);
  if (calibrationValid()) {
    centerText(String(pct) + "%", 20, 3);
  } else {
    // No usable calibration yet: show the raw voltage instead of a made-up %.
    centerText(String(g_soilMv) + "mV", 24, 2);
  }

  display.drawLine(0, 43, SCREEN_WIDTH, 43, SH110X_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 46);
  display.print("Soil: ");
  display.print(soilStatusText(pct));

  String air = "air n/a";
  if (dhtOK) air = String(g_temperature, 0) + "C " + String(g_humidity, 0) + "%";
  // Push this text up against the right-hand edge: each character is 6 pixels
  // wide, so start it (number of characters x 6) pixels in from the right.
  display.setCursor(SCREEN_WIDTH - air.length() * 6, 46);
  display.print(air);

  if (wifiOK) centerText(apUrl, 56, 1);

  display.display();
}

// Draws `text` horizontally centered on the display at the given y, for the given text size.
// Uses a fixed 6px-per-character advance (the default GFX font's width at size 1) rather than
// getTextBounds() — that call returned inconsistent widths across Adafruit_GFX library versions
// and pushed text off-screen.
void centerText(const String& text, int y, int textSize) {
  display.setTextSize(textSize);
  int textWidthPx = text.length() * 6 * textSize;
  int x = (SCREEN_WIDTH - textWidthPx) / 2;
  if (x < 0) x = 0;
  display.setCursor(x, y);
  display.print(text);
}

bool inRange(float value, float minValue, float maxValue) {
  return !isnan(value) && value >= minValue && value <= maxValue;
}

// Checks each part is connected AND gives a believable first reading, prints
// the result to Serial, and returns true only if everything passed.
bool runSelfTest() {
  Serial.println("--- Self-test ---");
  Serial.print("OLED (SH1106): "); Serial.println(oledOK ? "OK" : "NOT FOUND");

  // The DHT11 needs about a second after power-on before it answers, so try a
  // few times before giving up on it.
  Serial.print("DHT11:         ");
  float t = NAN, h = NAN;
  for (int attempt = 1; attempt <= 3; attempt++) {
    delay(1200);
    h = dht.readHumidity(true);   // true = take a fresh reading now
    t = dht.readTemperature();
    if (!isnan(t) && !isnan(h)) break;   // got a reading: stop trying
  }
  if (isnan(t) || isnan(h)) {
    Serial.println("NOT FOUND");
  } else {
    dhtOK = true;
    g_temperature = t;
    g_humidity = h;
    dhtPassed = inRange(t, TEMP_MIN_C, TEMP_MAX_C) && inRange(h, HUM_MIN_PCT, HUM_MAX_PCT);
    dhtStatus = dhtPassed ? "OK" : "BAD READING";
    dhtDetail = String(t, 1) + " °C, " + String(h, 0) + " %";
    Serial.printf("%s (%.1f C, %.0f %%)\n", dhtStatus.c_str(), t, h);
  }

  // An analog sensor can't be "found" the way an I2C one can — there is only a
  // voltage on the pin. So the test is whether that voltage is one a working,
  // connected sensor could give.
  Serial.print("Soil sensor:   ");
  g_soilMv = readSoilMillivolts();
  soilPassed = g_soilMv >= SOIL_MIN_MV && g_soilMv <= SOIL_MAX_MV;
  soilStatus = soilPassed ? "OK" : "BAD READING";
  soilDetail = String(g_soilMv) + " mV";
  Serial.printf("%s (%d mV)\n", soilStatus.c_str(), g_soilMv);

  Serial.print("WiFi AP:       ");
  if (wifiOK) { Serial.println("OK"); } else { Serial.print("FAILED ("); Serial.print(wifiError); Serial.println(")"); }

  bool passed = oledOK && dhtPassed && soilPassed && wifiOK;
  Serial.print("RESULT:        "); Serial.println(passed ? "PASS" : "FAIL");
  return passed;
}

// Shown on the OLED only when the self-test fails, so a problem is visible even
// without a computer attached. (If the OLED itself failed, only Serial shows it.)
void showSelfTestFailure() {
  display.clearDisplay();
  centerText("SELF-TEST FAILED", 0, 1);
  display.drawLine(0, 9, SCREEN_WIDTH, 9, SH110X_WHITE);
  int y = 14;
  if (!dhtPassed) {
    display.setCursor(0, y); y += 10;
    display.print(dhtOK ? "DHT11: bad reading" : "DHT11: not found");
  }
  if (!soilPassed) {
    display.setCursor(0, y); y += 10;
    display.print("Soil: bad reading");
  }
  if (!wifiOK) {
    display.setCursor(0, y); y += 10;
    display.print("WiFi: failed");
  }
  display.setCursor(0, 54);
  display.print("See Serial Monitor");
  display.display();
  delay(5000);
}
