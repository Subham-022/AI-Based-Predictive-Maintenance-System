#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// wifi
const char* ssid     = "Subham022";
const char* password = "12345678";

WebServer server(80);

// pins
#define IN1 25
#define IN2 26
#define ENA 14
#define DHTPIN 4
#define DHTTYPE DHT22
#define BUZZER 15
#define LED_RED 16
#define LED_GREEN 2
#define ACS712_PIN 34

// i2c pins for mpu6050
#define SDA_PIN 21
#define SCL_PIN 22

DHT dht(DHTPIN, DHTTYPE);
Adafruit_MPU6050 mpu;
bool mpuReady = false;

// sensor readings
float temperature = 0;
float humidity = 0;
float current = 0;

// imu stuff
float ax=0, ay=0, az=0;
float gx=0, gy=0, gz=0;
float accMag = 0;
float accBase = 9.81;
float vibration = 0;

// health
float healthScore = 100.0;
String status_text = "Nominal";

// motor runtime counter ( in seconds)
unsigned long motorSeconds = 0;
unsigned long lastSecTick = 0;

// flags
bool motorRunning = true;
bool buzzerMuted = false;
unsigned long muteUntil = 0;

void motorForward() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(ENA, HIGH);
}

void motorStop() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(ENA, LOW);
}

// health calculation
void calculateHealth() {
  // temp stress
  float tStress = 0;
  if (temperature >= 55) tStress = 100;
  else if (temperature >= 45) tStress = 50 + (temperature - 45) * 5;
  else if (temperature >= 35) tStress = (temperature - 35) * 5;

  // current stress
  float cStress = 0;
  if (current >= 2.0) cStress = 100;
  else if (current >= 1.5) cStress = 50 + (current - 1.5) * 100;
  else if (current >= 0.5) cStress = (current - 0.5) * 50;

  // vibration stress
  float vStress = 0;
  if (vibration >= 6.0) vStress = 100;
  else if (vibration >= 3.0) vStress = 50 + (vibration - 3.0) * (50.0/3.0);
  else if (vibration >= 1.0) vStress = (vibration - 1.0) * 25;

  float stress = (tStress * 0.5) + (cStress * 0.25) + (vStress * 0.25);
  healthScore = 100 - stress;
  if (healthScore < 0) healthScore = 0;
  if (healthScore > 100) healthScore = 100;

  if (healthScore >= 80) status_text = "Nominal";
  else if (healthScore >= 60) status_text = "Monitor Closely";
  else if (healthScore >= 40) status_text = "Maintenance Needed";
  else status_text = "CRITICAL - Stop Motor";
}

// json for dashboard
String getJSON() {
  String s = "{";
  s += "\"temperature\":" + String(temperature, 1) + ",";
  s += "\"humidity\":" + String(humidity, 1) + ",";
  s += "\"current\":" + String(current, 2) + ",";
  s += "\"vibration\":" + String(vibration, 2) + ",";
  s += "\"health\":" + String(healthScore, 1) + ",";
  s += "\"runtime\":" + String(motorSeconds) + ",";
  s += "\"status\":\"" + status_text + "\",";
  s += "\"motor\":\"" + String(motorRunning ? "running" : "stopped") + "\",";
  s += "\"buzzerMuted\":" + String(buzzerMuted ? "true" : "false") + ",";
  s += "\"mpu\":" + String(mpuReady ? "true" : "false") + ",";
  s += "\"base\":" + String(accBase, 2);
  s += "}";
  return s;
}

// handlers
void handleData() { server.send(200, "application/json", getJSON()); }

void handleStart() {
  motorRunning = true;
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleStop() {
  motorRunning = false;
  motorStop();
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleSilence() {
  muteUntil = millis() + 30000UL;
  buzzerMuted = true;
  server.send(200, "application/json", "{\"ok\":true}");
}

// tare the mpu baseline
void handleCalibrate() {
  if (!mpuReady) {
    server.send(200, "application/json", "{\"ok\":false}");
    return;
  }
  float sum = 0;
  int n = 40;
  for (int i = 0; i < n; i++) {
    sensors_event_t a, g, t;
    mpu.getEvent(&a, &g, &t);
    float mag = sqrt(a.acceleration.x*a.acceleration.x
                   + a.acceleration.y*a.acceleration.y
                   + a.acceleration.z*a.acceleration.z);
    sum += mag;
    delay(50);
  }
  accBase = sum / n;
  server.send(200, "application/json", "{\"ok\":true,\"base\":" + String(accBase, 2) + "}");
}

void handleResetRuntime() {
  motorSeconds = 0;
  server.send(200, "application/json", "{\"ok\":true}");
}

// dashboard page
String getHTML() {
  String html = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Motor Health Dashboard</title>
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    font-family: "Segoe UI", Tahoma, Arial, sans-serif;
    background: #f1f5f9;
    color: #1f2937;
    padding: 24px 20px;
    line-height: 1.45;
  }
  .wrap { max-width: 1000px; margin: 0 auto; }

  .head {
    background: linear-gradient(135deg, #1e3a8a, #3b82f6);
    color: #fff;
    border-radius: 12px;
    padding: 24px 28px;
    margin-bottom: 18px;
    display: flex;
    align-items: center;
    justify-content: space-between;
    flex-wrap: wrap;
    gap: 10px;
    box-shadow: 0 4px 12px rgba(30, 58, 138, 0.15);
  }
  .head h1 { font-size: 21px; font-weight: 700; }
  .head .sub { font-size: 13px; color: #dbeafe; margin-top: 5px; }
  .live {
    display: inline-flex; align-items: center; gap: 6px;
    background: rgba(255,255,255,0.12);
    padding: 5px 12px; border-radius: 20px;
    font-size: 12px; font-weight: 600;
  }
  .live i {
    width: 8px; height: 8px; background: #4ade80;
    border-radius: 50%; display: inline-block;
    animation: blink 1.2s infinite;
  }
  @keyframes blink { 0%,100%{opacity:1} 50%{opacity:.25} }

  .alert {
    background: #fee2e2; color: #7f1d1d;
    border: 1px solid #fecaca; border-left: 4px solid #dc2626;
    border-radius: 6px; padding: 10px 14px;
    font-size: 13px; font-weight: 600;
    margin-bottom: 16px;
    display: none;
  }
  .alert.show { display: block; }

  .card {
    background: #fff;
    border: 1px solid #e2e8f0;
    border-radius: 10px;
    padding: 20px 22px;
    margin-bottom: 16px;
    box-shadow: 0 1px 3px rgba(0,0,0,0.04);
  }

  /* health area */
  .health {
    display: grid;
    grid-template-columns: 210px 1fr;
    gap: 30px;
    align-items: center;
    padding: 28px;
  }
  .ring { position: relative; width: 190px; height: 190px; }
  .ring svg { transform: rotate(-90deg); filter: drop-shadow(0 2px 4px rgba(0,0,0,0.08)); }
  .ring .track { fill: none; stroke: #eef2f7; stroke-width: 12; }
  .ring .fill  { fill: none; stroke-width: 12; stroke-linecap: round; }
  .ring .lbl {
    position: absolute; inset: 0;
    display: flex; flex-direction: column;
    align-items: center; justify-content: center;
  }
  .ring .pct { font-size: 32px; font-weight: bold; color: #1e3a8a; }
  .ring .tag { font-size: 10px; color: #6b7280; text-transform: uppercase; letter-spacing: 1.2px; margin-top: 5px; }

  .hinfo h2 { font-size: 15px; color: #374151; margin-bottom: 10px; }
  .badge {
    display: inline-block;
    padding: 6px 14px;
    border-radius: 5px;
    font-size: 13px;
    font-weight: 600;
    background: #dcfce7;
    color: #166534;
  }
  .runtime {
    margin-top: 14px;
    font-size: 13px;
    color: #555;
    display: inline-flex;
    align-items: center;
    gap: 10px;
    background: #f8fafc;
    border: 1px solid #e2e8f0;
    padding: 8px 12px;
    border-radius: 6px;
  }
  .runtime b { color: #2563eb; font-size: 15px; }

  /* controls */
  .ctrl {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 14px;
  }
  .ctrl-item {
    display: flex; align-items: center; justify-content: space-between;
    gap: 10px; padding: 12px 14px;
    background: #f8fafc; border: 1px solid #e2e8f0; border-radius: 6px;
  }
  .ctrl-label { font-size: 11px; color: #6b7280; text-transform: uppercase; letter-spacing: 1px; }
  .ctrl-state { font-size: 14px; font-weight: 600; color: #111827;
                display: inline-flex; align-items: center; gap: 7px; }
  .sdot { width: 9px; height: 9px; border-radius: 50%; display: inline-block; }

  .btn {
    border: none;
    padding: 8px 15px;
    border-radius: 6px;
    font-size: 12px;
    font-weight: 600;
    cursor: pointer;
    font-family: inherit;
    transition: background 0.2s, transform 0.1s;
  }
  .btn:hover { filter: brightness(0.95); }
  .btn:active { transform: translateY(1px); }
  .btn-start { background: #16a34a; color: #fff; }
  .btn-stop  { background: #dc2626; color: #fff; }
  .btn-mute  { background: #f59e0b; color: #fff; }
  .btn-cal   { background: #2563eb; color: #fff; }
  .btn-reset { background: #64748b; color: #fff; padding: 5px 11px; font-size: 11px; border-radius: 4px; }
  .btn:disabled { background: #cbd5e1 !important; cursor: not-allowed; }
  .btns { display: flex; gap: 6px; }

  /* sensor cards */
  .grid { display: grid; grid-template-columns: repeat(4, 1fr); gap: 14px; margin-bottom: 16px; }
  .scard {
    background: #fff;
    border: 1px solid #e2e8f0;
    border-radius: 10px;
    padding: 16px 18px;
    border-top: 3px solid #94a3b8;
    box-shadow: 0 1px 3px rgba(0,0,0,0.04);
  }
  .scard.sc-t { border-top-color: #ef4444; }
  .scard.sc-h { border-top-color: #2563eb; }
  .scard.sc-c { border-top-color: #10b981; }
  .scard.sc-v { border-top-color: #f59e0b; }
  .scard h3 { font-size: 11px; color: #6b7280; text-transform: uppercase; letter-spacing: 1.2px; margin-bottom: 10px; }
  .scard .val { font-size: 24px; font-weight: 600; color: #111827; }
  .scard .u { font-size: 12px; color: #6b7280; margin-left: 3px; }
  .bar { margin-top: 12px; height: 6px; background: #eef2f7; border-radius: 3px; overflow: hidden; }
  .bar > div { height: 100%; width: 0%; transition: width 0.5s ease, background 0.3s ease; border-radius: 3px; }

  /* chart */
  .chart-title { font-size: 13px; color: #374151; margin-bottom: 10px; font-weight: 600; }
  .legend { display: flex; gap: 14px; font-size: 11px;
            color: #6b7280; margin-bottom: 8px; flex-wrap: wrap; }
  .legend span { display: inline-flex; align-items: center; gap: 5px; }
  .legend i { display: inline-block; width: 10px; height: 10px; border-radius: 2px; }
  .chart { width: 100%; height: 140px; }

  .info {
    display: flex; justify-content: space-between; flex-wrap: wrap;
    background: #fff; border: 1px solid #e2e8f0; border-radius: 10px;
    padding: 13px 20px; font-size: 12px; color: #555; gap: 8px;
    box-shadow: 0 1px 3px rgba(0,0,0,0.04);
  }
  .info b { color: #111827; }
  footer { text-align: center; font-size: 11px; color: #9ca3af; margin-top: 14px; }

  @media (max-width: 760px) {
    .grid { grid-template-columns: 1fr 1fr; }
    .ctrl { grid-template-columns: 1fr; }
  }
  @media (max-width: 520px) {
    .health { grid-template-columns: 1fr; text-align: center; justify-items: center; }
  }
</style>
</head>
<body>
<div class="wrap">

  <div class="head">
    <div>
      <h1>AI Based Predictive Maintenance System Dashboard</h1>
      <div class="sub">Motor Health Monitor</div>
    </div>
    <div class="live"><i></i> LIVE</div>
  </div>

  <div id="alertBox" class="alert">Warning: system condition has degraded. Check motor.</div>

  <!-- health -->
  <div class="card health">
    <div class="ring">
      <svg viewBox="0 0 190 190" width="190" height="190">
        <circle class="track" cx="95" cy="95" r="78"/>
        <circle id="hFill" class="fill"
          cx="95" cy="95" r="78"
          stroke="#22c55e" stroke-dasharray="490" stroke-dashoffset="0"/>
      </svg>
      <div class="lbl">
        <div class="pct"><span id="hPct">--</span>%</div>
        <div class="tag">health score</div>
      </div>
    </div>
    <div class="hinfo">
      <h2>Current System Condition</h2>
      <span id="sBadge" class="badge">--</span>
      <div class="runtime">
        Motor runtime:
        <b><span id="rt">0s</span></b>
        <button class="btn btn-reset" onclick="resetRuntime()">Reset</button>
      </div>
    </div>
  </div>

  <!-- controls -->
  <div class="card">
    <div class="ctrl">
      <div class="ctrl-item">
        <div>
          <div class="ctrl-label">Motor</div>
          <div class="ctrl-state">
            <span id="mDot" class="sdot" style="background:#16a34a;"></span>
            <span id="mState">--</span>
          </div>
        </div>
        <div class="btns">
          <button id="btnStart" class="btn btn-start" onclick="send('start')">Start</button>
          <button id="btnStop"  class="btn btn-stop"  onclick="send('stop')">Stop</button>
        </div>
      </div>

      <div class="ctrl-item">
        <div>
          <div class="ctrl-label">Buzzer</div>
          <div class="ctrl-state">
            <span id="bDot" class="sdot" style="background:#9ca3af;"></span>
            <span id="bState">--</span>
          </div>
        </div>
        <div class="btns">
          <button id="btnMute" class="btn btn-mute" onclick="send('silence')">Silence 30s</button>
          <button id="btnCal"  class="btn btn-cal"  onclick="calibrate()">Tare</button>
        </div>
      </div>
    </div>
  </div>

  <!-- sensors -->
  <div class="grid">
    <div class="scard sc-t">
      <h3>Temperature</h3>
      <div class="val"><span id="tV">--</span><span class="u">&deg;C</span></div>
      <div class="bar"><div id="tB"></div></div>
    </div>
    <div class="scard sc-h">
      <h3>Humidity</h3>
      <div class="val"><span id="hV">--</span><span class="u">%</span></div>
      <div class="bar"><div id="hB"></div></div>
    </div>
    <div class="scard sc-c">
      <h3>Current</h3>
      <div class="val"><span id="cV">--</span><span class="u">A</span></div>
      <div class="bar"><div id="cB"></div></div>
    </div>
    <div class="scard sc-v">
      <h3>Vibration</h3>
      <div class="val"><span id="vV">--</span><span class="u">m/s&sup2;</span></div>
      <div class="bar"><div id="vB"></div></div>
    </div>
  </div>

  <!-- chart -->
  <div class="card">
    <div class="chart-title">Live Trend (last 30 readings)</div>
    <div class="legend">
      <span><i style="background:#ef4444"></i> Temperature (&deg;C)</span>
      <span><i style="background:#2563eb"></i> Current &times;10 (A)</span>
      <span><i style="background:#f59e0b"></i> Vibration (m/s&sup2;)</span>
    </div>
    <svg class="chart" viewBox="0 0 300 120" preserveAspectRatio="none">
      <line x1="0" y1="30" x2="300" y2="30" stroke="#eef2f7" stroke-width="1"/>
      <line x1="0" y1="60" x2="300" y2="60" stroke="#eef2f7" stroke-width="1"/>
      <line x1="0" y1="90" x2="300" y2="90" stroke="#eef2f7" stroke-width="1"/>
      <polyline id="tLine" fill="none" stroke="#ef4444" stroke-width="2" points=""/>
      <polyline id="cLine" fill="none" stroke="#2563eb" stroke-width="2" points=""/>
      <polyline id="vLine" fill="none" stroke="#f59e0b" stroke-width="2" points=""/>
    </svg>
  </div>

  <div class="info">
    <span>Sensors: <b>DHT22 + ACS712 + MPU6050</b></span>
    <span>Controller: <b>ESP32</b></span>
    <span>Last update: <b id="ts">--</b></span>
  </div>

  <footer>Auto-refresh every 1.5 seconds</footer>
</div>

<script>
  function setV(id, val) {
    var e = document.getElementById(id);
    if (e.textContent !== String(val)) {
      e.textContent = val;
    }
  }
  function clamp(v, lo, hi) { return v < lo ? lo : v > hi ? hi : v; }

  function fmtTime(sec) {
    sec = parseInt(sec);
    var h = Math.floor(sec / 3600);
    var m = Math.floor((sec % 3600) / 60);
    var s = sec % 60;
    if (h > 0) return h + "h " + m + "m " + s + "s";
    if (m > 0) return m + "m " + s + "s";
    return s + "s";
  }

  var tHist = [], cHist = [], vHist = [];
  var MAX = 30;

  function drawChart() {
    var W = 300, H = 120;
    function tY(v) { return H - clamp(v,0,60)/60*H; }
    function cY(v) { return H - clamp(v*10,0,60)/60*H; }
    function vY(v) { return H - clamp(v,0,10)/10*H; }
    function toPts(arr, fn) {
      if (!arr.length) return "";
      var step = W / (MAX - 1);
      var out = [];
      for (var i = 0; i < arr.length; i++) {
        out.push((i*step).toFixed(1) + "," + fn(arr[i]).toFixed(1));
      }
      return out.join(" ");
    }
    document.getElementById("tLine").setAttribute("points", toPts(tHist, tY));
    document.getElementById("cLine").setAttribute("points", toPts(cHist, cY));
    document.getElementById("vLine").setAttribute("points", toPts(vHist, vY));
  }

  function send(cmd) {
    fetch("/" + cmd).then(function(){ refresh(); }).catch(function(){});
  }

  function resetRuntime() {
    fetch("/resetrt").then(function(){ refresh(); }).catch(function(){});
  }

  function calibrate() {
    var b = document.getElementById("btnCal");
    b.disabled = true;
    b.textContent = "Taring...";
    fetch("/calibrate").then(function(r){ return r.json(); })
      .then(function(){
        b.textContent = "Tare";
        b.disabled = false;
        refresh();
      })
      .catch(function(){
        b.textContent = "Tare";
        b.disabled = false;
      });
  }

  function refresh() {
    fetch("/data", { cache: "no-store" })
      .then(function(r){ return r.json(); })
      .then(function(d){
        setV("tV", d.temperature.toFixed(1));
        setV("hV", d.humidity.toFixed(1));
        setV("cV", d.current.toFixed(2));
        setV("vV", d.vibration.toFixed(2));
        setV("hPct", Math.round(d.health));
        setV("rt", fmtTime(d.runtime));

        var hs = d.health;
        var bg, fg;
        if (hs >= 80)      { bg="#dcfce7"; fg="#166534"; }
        else if (hs >= 60) { bg="#fef3c7"; fg="#78350f"; }
        else if (hs >= 40) { bg="#ffedd5"; fg="#7c2d12"; }
        else               { bg="#fee2e2"; fg="#7f1d1d"; }

        var badge = document.getElementById("sBadge");
        badge.textContent = d.status;
        badge.style.background = bg;
        badge.style.color = fg;

        var alertBox = document.getElementById("alertBox");
        if (hs < 40) {
          alertBox.classList.add("show");
          alertBox.textContent = "CRITICAL: " + d.status + " - please stop the motor.";
        } else {
          alertBox.classList.remove("show");
        }

        // ring fill - circumference = 2 * pi * 78 = 490
        var circ = 2 * Math.PI * 78;
        var fill = document.getElementById("hFill");
        fill.setAttribute("stroke-dashoffset", (circ * (1 - hs/100)).toFixed(1));
        var ringCol;
        if (hs >= 80) ringCol = "#22c55e";
        else if (hs >= 60) ringCol = "#f59e0b";
        else if (hs >= 40) ringCol = "#f97316";
        else ringCol = "#ef4444";
        fill.setAttribute("stroke", ringCol);

        // bars
        var tB = document.getElementById("tB");
        tB.style.width = clamp((d.temperature - 25)/30 * 100, 0, 100) + "%";
        tB.style.background = d.temperature > 45 ? "#ef4444" : d.temperature > 35 ? "#f59e0b" : "#22c55e";

        var hB = document.getElementById("hB");
        hB.style.width = clamp(d.humidity, 0, 100) + "%";
        hB.style.background = d.humidity > 80 ? "#f59e0b" : "#2563eb";

        var cB = document.getElementById("cB");
        cB.style.width = clamp(d.current/2 * 100, 0, 100) + "%";
        cB.style.background = d.current > 1.5 ? "#ef4444" : d.current > 0.5 ? "#f59e0b" : "#22c55e";

        var vB = document.getElementById("vB");
        vB.style.width = clamp(d.vibration/6 * 100, 0, 100) + "%";
        vB.style.background = d.vibration > 3 ? "#ef4444" : d.vibration > 1 ? "#f59e0b" : "#22c55e";

        var running = d.motor === "running";
        document.getElementById("mState").textContent = running ? "Running" : "Stopped";
        document.getElementById("mDot").style.background = running ? "#16a34a" : "#dc2626";
        document.getElementById("btnStart").disabled = running;
        document.getElementById("btnStop").disabled  = !running;

        var bActive = (hs < 40) || (d.temperature > 55) || (d.current > 2.0) || (d.vibration > 6);
        var muted = d.buzzerMuted;
        var bText, bCol;
        if (!bActive)      { bText = "Silent"; bCol = "#9ca3af"; }
        else if (muted)    { bText = "Muted";  bCol = "#f59e0b"; }
        else               { bText = "ALARM";  bCol = "#dc2626"; }
        document.getElementById("bState").textContent = bText;
        document.getElementById("bDot").style.background = bCol;
        document.getElementById("btnMute").disabled = !bActive || muted;

        tHist.push(d.temperature); cHist.push(d.current); vHist.push(d.vibration);
        if (tHist.length > MAX) tHist.shift();
        if (cHist.length > MAX) cHist.shift();
        if (vHist.length > MAX) vHist.shift();
        drawChart();

        document.getElementById("ts").textContent = new Date().toLocaleTimeString();
      })
      .catch(function(){
        document.getElementById("ts").textContent = "connection lost";
      });
  }

  refresh();
  setInterval(refresh, 1500);
</script>
</body>
</html>
)rawhtml";

  return html;
}

void handleRoot() { server.send(200, "text/html", getHTML()); }

void setup() {
  Serial.begin(115200);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(ENA, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);

  dht.begin();

  Wire.begin(SDA_PIN, SCL_PIN);
  if (mpu.begin()) {
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    mpuReady = true;
    Serial.println("MPU6050 ready.");
  } else {
    Serial.println("MPU6050 not detected - check wiring.");
  }

  WiFi.begin(ssid, password);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Dashboard: http://");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/start", handleStart);
  server.on("/stop", handleStop);
  server.on("/silence", handleSilence);
  server.on("/calibrate", handleCalibrate);
  server.on("/resetrt", handleResetRuntime);
  server.begin();

  lastSecTick = millis();
}

void loop() {
  server.handleClient();

  if (motorRunning) motorForward();
  else motorStop();

  // dht22
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t)) temperature = t;
  if (!isnan(h)) humidity = h;

  // acs712 current sensor
  int raw = analogRead(ACS712_PIN);
  float volt = raw * (3.3 / 4095.0);
  current = (volt - 2.5) / 0.185;

  // mpu6050 vibration
  if (mpuReady) {
    sensors_event_t a, g, tmp;
    mpu.getEvent(&a, &g, &tmp);
    ax = a.acceleration.x;
    ay = a.acceleration.y;
    az = a.acceleration.z;
    gx = g.gyro.x;
    gy = g.gyro.y;
    gz = g.gyro.z;
    accMag = sqrt(ax*ax + ay*ay + az*az);
    vibration = fabs(accMag - accBase);
  }

  // count motor runtime (seconds)
  if (motorRunning && millis() - lastSecTick >= 1000) {
    motorSeconds++;
    lastSecTick = millis();
  }
  if (!motorRunning) {
    lastSecTick = millis();
  }

  // auto-unmute
  if (buzzerMuted && millis() >= muteUntil) buzzerMuted = false;

  // fault check
  bool fault = (temperature > 55) || (current > 2.0) || (vibration > 6.0);

  if (fault) {
    digitalWrite(BUZZER, buzzerMuted ? LOW : HIGH);
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_GREEN, LOW);
  } else {
    digitalWrite(BUZZER, LOW);
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_GREEN, HIGH);
    buzzerMuted = false;
  }

  calculateHealth();

  delay(1000);
}
