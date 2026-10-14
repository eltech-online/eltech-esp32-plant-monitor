#pragma once

// The dashboard webpage, served as-is by handleRoot() — no server-side values
// are inserted into it. Instead, its own JavaScript (bottom of this file) polls
// the /data endpoint every 2 seconds and updates the readings, the calibration
// values and the power-on self-test results in the browser, so the page never
// needs a full reload. The calibration buttons send a POST to /calibrate, and
// the light correction box sends one to /lightfactor.
//
// R"HTML(...)HTML" is a C++ raw string literal: everything between the markers
// is taken literally, including quotes and newlines, so the HTML/CSS/JS below
// can be pasted in unescaped. On an ESP32 a `const` string like this stays
// in flash memory automatically (it's never copied into RAM), so PROGMEM changes
// nothing here — it's kept so the code also works on boards like the Arduino
// Uno, where it does matter.
const char PAGE_TEMPLATE[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ElTech-Online Plant Monitor</title>
  <style>
    body { font-family: -apple-system, Arial, sans-serif; background:#0f172a; color:#e2e8f0;
           margin:0; padding:24px; text-align:center; }
    h1 { font-size:20px; font-weight:600; margin:0 0 2px; }
    .sub { color:#94a3b8; font-size:13px; margin-bottom:24px; }
    .cards { display:flex; gap:12px; justify-content:center; flex-wrap:wrap; }
    .card { background:#1e293b; border-radius:12px; padding:20px 24px; min-width:110px; }
    .card .label { color:#94a3b8; font-size:12px; text-transform:uppercase; letter-spacing:0.05em; }
    .card .value { font-size:32px; font-weight:700; margin-top:6px; }
    .card .state { margin-top:8px; }
    .panel { margin-top:24px; background:#1e293b; border-radius:12px; padding:16px 20px;
             text-align:left; max-width:420px; margin-left:auto; margin-right:auto; }
    .panel .head { display:flex; justify-content:space-between; align-items:center; margin-bottom:8px; }
    .panel h2 { font-size:14px; margin:0; color:#e2e8f0; }
    .row { display:flex; justify-content:space-between; align-items:center; gap:12px;
           padding:7px 0; border-top:1px solid #334155; font-size:13px; }
    .row .name { color:#e2e8f0; }
    .row .detail { color:#94a3b8; font-size:12px; margin-left:6px; }
    .badge { font-size:11px; font-weight:700; letter-spacing:0.04em; padding:3px 8px;
             border-radius:999px; white-space:nowrap; }
    .ok   { background:#14532d; color:#86efac; }
    .fail { background:#7f1d1d; color:#fca5a5; }
    .warn { background:#713f12; color:#fde68a; }
    .hint { color:#cbd5e1; font-size:13px; line-height:1.6; margin:10px 0 0; padding-left:20px; }
    .buttons { display:flex; gap:8px; flex-wrap:wrap; margin-top:12px; }
    button { font:inherit; font-size:13px; font-weight:600; color:#0f172a; background:#7dd3fc;
             border:0; border-radius:8px; padding:9px 14px; }
    button.quiet { background:#334155; color:#e2e8f0; }
    #calmsg, #lkmsg { color:#94a3b8; font-size:12px; margin-top:10px; min-height:15px; }
    .panel a { color:#7dd3fc; }
    .footer { margin-top:24px; color:#64748b; font-size:11px; }
    .footer a { color:#94a3b8; }
  </style>
</head>
<body>
  <h1>ElTech-Online</h1>
  <div class="sub">ESP32 Plant Monitor &mdash; live readings, updated every 2 seconds</div>
  <div class="cards">
    <div class="card"><div class="label">Soil moisture</div><div class="value" id="s">--</div>
      <div class="state"><span class="badge" id="st">--</span></div></div>
    <div class="card"><div class="label">Air temperature</div><div class="value" id="t">--</div></div>
    <div class="card"><div class="label">Air humidity</div><div class="value" id="h">--</div></div>
    <div class="card"><div class="label">Light</div><div class="value" id="l">--</div></div>
  </div>
  <div class="panel">
    <div class="head"><h2>Soil sensor calibration</h2><span class="badge" id="calstate">--</span></div>
    <div class="row"><span class="name">Sensor reading now</span><span id="mv">--</span></div>
    <div class="row"><span class="name">Dry point (0 %)</span><span id="dry">--</span></div>
    <div class="row"><span class="name">Wet point (100 %)</span><span id="wet">--</span></div>
    <ol class="hint">
      <li>Hold the sensor in the air, clean and dry, and press <b>Set dry</b>.</li>
      <li>Stand its flat end in a glass of water and press <b>Set wet</b>. Keep the
          electronics at the cable end out of the water.</li>
    </ol>
    <div class="buttons">
      <button onclick="calibrate('dry')">Set dry</button>
      <button onclick="calibrate('wet')">Set wet</button>
      <button class="quiet" onclick="calibrate('reset')">Reset</button>
    </div>
    <div id="calmsg"></div>
  </div>
  <div class="panel">
    <div class="head"><h2>Light correction</h2><span id="lk">--</span></div>
    <ol class="hint">
      <li>The white dome blocks some light, so the reading is multiplied by this
          number. 1.5 matches a bare sensor (it read 180 lux where the dome read 120).</li>
      <li>To set your own: measure the same spot with a phone light-meter app or a
          bare sensor, and enter that reading &divide; this one. Don't remove the dome:
          it is usually sealed on.</li>
    </ol>
    <div class="buttons">
      <input id="lkin" type="number" step="0.05" min="0.5" max="5" value="1.5"
             style="font:inherit;width:80px;padding:6px;border-radius:6px;border:1px solid #334155;background:#0f172a;color:#e2e8f0">
      <button onclick="setLight(el('lkin').value)">Save</button>
      <button class="quiet" onclick="setLight('reset')">Reset to 1.5</button>
    </div>
    <div id="lkmsg"></div>
  </div>
  <div class="panel">
    <div class="head"><h2>Self-test (at power-on)</h2><span class="badge" id="result">--</span></div>
    <div id="parts"></div>
  </div>
  <div class="panel">
    <div class="head"><h2>Build your own</h2></div>
    <ol class="hint">
      <li>Get the full source, wiring diagram and setup guide on <a href="https://github.com/eltech-online/eltech-esp32-plant-monitor" target="_blank">GitHub</a></li>
      <li>Flash it onto an ESP32 with the Arduino IDE (board package + 3 libraries, all listed in the README)</li>
      <li>Power it on, connect to the WiFi network it creates, and you'll see this exact page</li>
    </ol>
  </div>
  <div class="footer">ESP32 + soil moisture sensor + BH1750 light sensor + DHT11 + OLED &middot; <a href="https://github.com/eltech-online/eltech-esp32-plant-monitor" target="_blank">github.com/eltech-online/eltech-esp32-plant-monitor</a></div>
  <script>
    const el = (id) => document.getElementById(id);
    async function refresh() {
      try {
        const r = await fetch('/data');
        const d = await r.json();
        el('s').textContent = d.soil;
        el('t').textContent = d.temp;
        el('h').textContent = d.hum;
        el('l').textContent = d.light;
        // DRY and WET are amber (the plant needs attention), OK is green.
        el('st').textContent = d.status;
        el('st').className = 'badge ' + (d.status === 'OK' ? 'ok' : 'warn');
        el('mv').textContent = d.soil_mv + ' mV';
        el('dry').textContent = d.cal.dry + ' mV';
        el('wet').textContent = d.cal.wet + ' mV';
        const cal = el('calstate');
        if (!d.cal.valid)      { cal.textContent = 'NEEDED';   cal.className = 'badge fail'; }
        else if (d.cal.saved)  { cal.textContent = 'SAVED';    cal.className = 'badge ok'; }
        else                   { cal.textContent = 'DEFAULTS'; cal.className = 'badge warn'; }
        if (d.lightk !== undefined) {
          el('lk').textContent = 'x' + d.lightk.toFixed(2);
          if (!lkLoaded) { el('lkin').value = d.lightk.toFixed(2); lkLoaded = true; }  // don't overwrite while typing
        }
        if (d.selftest) showSelfTest(d.selftest);
      } catch (e) { /* device rebooting or momentarily busy — try again next tick */ }
    }
    // Saves the light correction factor on the board (or "reset" = 1.5).
    let lkLoaded = false;
    async function setLight(f) {
      try {
        const r = await fetch('/lightfactor?f=' + encodeURIComponent(f), { method: 'POST' });
        const d = await r.json();
        el('lkmsg').textContent = d.msg;
        lkLoaded = false;   // show the saved value in the box again
      } catch (e) { el('lkmsg').textContent = 'Could not reach the board. Try again.'; }
      refresh();
    }
    // Asks the board to save its current reading as the dry or wet point.
    async function calibrate(point) {
      try {
        const r = await fetch('/calibrate?point=' + point, { method: 'POST' });
        const d = await r.json();
        el('calmsg').textContent = d.msg;
      } catch (e) { el('calmsg').textContent = 'Could not reach the board. Try again.'; }
      refresh();
    }
    // Draws one row per part: name, the first reading it took, and an OK/fail badge.
    function showSelfTest(st) {
      const result = el('result');
      result.textContent = st.result;
      result.className = 'badge ' + (st.result === 'PASS' ? 'ok' : 'fail');
      const parts = el('parts');
      parts.textContent = '';
      for (const [name, status, detail] of st.parts) {
        const row = document.createElement('div');
        row.className = 'row';
        const label = document.createElement('span');
        label.innerHTML = '<span class="name"></span><span class="detail"></span>';
        label.children[0].textContent = name;
        label.children[1].textContent = detail;
        const badge = document.createElement('span');
        badge.className = 'badge ' + (status === 'OK' ? 'ok' : 'fail');
        badge.textContent = status;
        row.append(label, badge);
        parts.append(row);
      }
    }
    refresh();
    setInterval(refresh, 2000);
  </script>
</body>
</html>
)HTML";
