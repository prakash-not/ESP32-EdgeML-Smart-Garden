/*
 * PROJECT: IIITH SMART GARDEN AI (Final Hybrid Edition)
 * PLANT: Panda Plant (Succulent) - Loves Dry, Hates Wet.
 * FEATURES: Auto-Watering, Day/Night Lighting, Safety Alerts, Manual Override.
 * FIXES: Time Sync Timeout, Active Low Relay, Water Level Safety.
 */

#include <WiFi.h>
#include <WebServer.h>
#include "DHT.h"
#include "time.h"

// ==========================================
// 1. SETTINGS & PIN DEFINITIONS
// ==========================================
const char* ssid = "Google pixel 8a";     
const char* password = "789789789789"; 

// Time Settings for India (UTC +5:30)
const long  gmtOffset_sec = 19800; 
const int   daylightOffset_sec = 0;
const char* ntpServer = "pool.ntp.org";

// Pins
#define RELAY_PIN     26  // Pump (Active LOW: LOW=ON, HIGH=OFF)
#define DHT_PIN       4   // Temp & Hum
#define SOIL_PIN      34  // Soil Moisture (Analog)
#define LDR_PIN       35  // Light (Analog)
#define MQ2_PIN       32  // Gas (Analog)
#define WATER_LVL_PIN 33  // Water Level (Analog/Digital)
#define BUZZER_PIN    19  // Alarm
#define LED_PIN       21  // Grow Light

#define DHTTYPE DHT11
DHT dht(DHT_PIN, DHTTYPE);
WebServer server(80);

// ==========================================
// 2. GLOBAL VARIABLES
// ==========================================
float temp = 0; float hum = 0;
int soil = 0; int gas = 0; int light = 0; int water = 0;

// Status Messages
String systemState = "INITIALIZING...";
String tStat="--", hStat="--", sStat="--", gStat="--", lStat="--", wStat="--";

// Timers & Flags for Automation
bool manualOverride = false;
unsigned long lastManualAction = 0;
const long MANUAL_TIMEOUT = 60000; // 60 Seconds before returning to Auto

// Pump Logic (Pulse Watering)
bool isPumping = false;
unsigned long pumpStartTime = 0;
unsigned long lastPumpRun = 0; 
const long PUMP_DURATION = 3000;   // Pump ON for 3 seconds
const long PUMP_COOLDOWN = 30000;  // Wait 30 seconds before next check

// Buzzer Logic
bool isBuzzing = false;
unsigned long buzzStartTime = 0;
const long BUZZ_DURATION = 3000;

// ==========================================
// 3. WEB INTERFACE (Scientific UI)
// ==========================================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html lang="en">
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Smart Garden Monitor</title>
  <style>
    :root { 
      --bg: #f0f2f5; 
      --card: #ffffff; 
      --good: #28a745; 
      --bad: #dc3545; 
      --warn: #ffc107; 
      --primary: #0056b3;
    }
    body { font-family: 'Segoe UI', sans-serif; background: var(--bg); text-align: center; margin: 0; color: #333; }
    
    /* Header */
    .header { background: var(--primary); color: white; padding: 25px; box-shadow: 0 4px 6px rgba(0,0,0,0.1); }
    h1 { margin: 0; font-size: 22px; letter-spacing: 1px; }
    .sub { font-size: 12px; opacity: 0.9; margin-top: 5px; font-weight: 300; letter-spacing: 2px; }

    /* Diagnosis Box */
    .main-box { background: var(--card); max-width: 500px; margin: 20px auto; padding: 20px; border-radius: 8px; border-left: 8px solid #ccc; box-shadow: 0 2px 8px rgba(0,0,0,0.05); }
    .lbl { font-size: 10px; font-weight: bold; color: #666; text-transform: uppercase; letter-spacing: 1px; }
    #sys-stat { font-size: 20px; font-weight: 800; margin-top: 10px; color: #333; }

    /* Grid */
    .grid { display: flex; flex-wrap: wrap; justify-content: center; gap: 15px; max-width: 800px; margin: 0 auto; padding: 0 10px; }
    .card { background: var(--card); width: 130px; padding: 15px; border-radius: 10px; box-shadow: 0 2px 5px rgba(0,0,0,0.05); }
    .val { font-size: 24px; font-weight: bold; color: #333; margin: 5px 0; }
    .unit { font-size: 12px; font-weight: normal; color: #777; }
    
    /* Status Badges */
    .badge { font-size: 10px; font-weight: bold; color: white; padding: 4px 8px; border-radius: 4px; display: inline-block; }
    .b-good { background: var(--good); } 
    .b-bad { background: var(--bad); } 
    .b-warn { background: var(--warn); color: #333; }

    /* Controls */
    .ctrl-panel { max-width: 400px; margin: 30px auto; padding-bottom: 50px; }
    .ctrl-row { background: var(--card); padding: 12px 20px; margin-bottom: 10px; border-radius: 8px; display: flex; justify-content: space-between; align-items: center; box-shadow: 0 2px 4px rgba(0,0,0,0.05); }
    .ctrl-lbl { font-weight: 600; font-size: 14px; }
    
    button { border: none; padding: 8px 16px; border-radius: 4px; font-weight: bold; cursor: pointer; color: white; font-size: 12px; margin-left: 5px; }
    .on { background: #007bff; } .off { background: #dc3545; }
    a { text-decoration: none; }
  </style>

  <script>
    setInterval(() => {
      fetch('/data').then(r => r.json()).then(d => {
        // Values
        document.getElementById("t").innerText = d.t; document.getElementById("h").innerText = d.h;
        document.getElementById("s").innerText = d.s; document.getElementById("g").innerText = d.g;
        document.getElementById("l").innerText = d.l; document.getElementById("w").innerText = d.w;
        
        // System Status
        let st = document.getElementById("sys-stat");
        let box = document.getElementById("box");
        st.innerText = d.st;
        
        if(d.st.includes("HEALTHY")) { box.style.borderLeftColor = "#28a745"; st.style.color = "#28a745"; }
        else if(d.st.includes("DANGER") || d.st.includes("ALERT") || d.st.includes("EMPTY")) { box.style.borderLeftColor = "#dc3545"; st.style.color = "#dc3545"; }
        else { box.style.borderLeftColor = "#ffc107"; st.style.color = "#e67e22"; }

        // Badges
        setBadge("tb", d.ts); setBadge("hb", d.hs); setBadge("sb", d.ss);
        setBadge("gb", d.gs); setBadge("lb", d.ls); setBadge("wb", d.ws);
      });
    }, 1500);

    function setBadge(id, txt) {
      let el = document.getElementById(id);
      el.innerText = txt;
      el.className = "badge"; 
      if(txt === "GOOD" || txt === "OPTIMAL" || txt === "FULL" || txt === "CLEAN") el.classList.add("b-good");
      else if(txt === "BAD" || txt === "DANGER" || txt === "EMPTY" || txt === "TOO DRY" || txt === "DARK") el.classList.add("b-bad");
      else el.classList.add("b-warn");
    }
  </script>
</head>
<body>

  <div class="header">
    <h1>PHOENIX PLANT AI</h1>
    <div class="sub">AUTONOMOUS MONITORING SYSTEM</div>
  </div>

  <div id="box" class="main-box">
    <div class="lbl">LIVE DIAGNOSIS</div>
    <div id="sys-stat">INITIALIZING SYSTEM...</div>
  </div>

  <div class="grid">
    <div class="card"><div class="lbl">Temp</div><div class="val"><span id="t">--</span><span class="unit">°C</span></div><div id="tb" class="badge">--</div></div>
    <div class="card"><div class="lbl">Humidity</div><div class="val"><span id="h">--</span><span class="unit">%</span></div><div id="hb" class="badge">--</div></div>
    <div class="card"><div class="lbl">Soil Moist</div><div class="val" id="s">--</div><div id="sb" class="badge">--</div></div>
    <div class="card"><div class="lbl">Air Quality</div><div class="val" id="g">--</div><div id="gb" class="badge">--</div></div>
    <div class="card"><div class="lbl">Light Lvl</div><div class="val" id="l">--</div><div id="lb" class="badge">--</div></div>
    <div class="card"><div class="lbl">Water Tank</div><div class="val" id="w">--</div><div id="wb" class="badge">--</div></div>
  </div>

  <div class="ctrl-panel">
    <div class="lbl" style="margin-bottom:10px; border-bottom:1px solid #ddd;">MANUAL CONTROLS</div>
    
    <div class="ctrl-row">
      <span class="ctrl-lbl">Water Pump</span>
      <div><a href="/pump/on"><button class="on">ON</button></a><a href="/pump/off"><button class="off">OFF</button></a></div>
    </div>
    <div class="ctrl-row">
      <span class="ctrl-lbl">Grow Light</span>
      <div><a href="/light/on"><button class="on">ON</button></a><a href="/light/off"><button class="off">OFF</button></a></div>
    </div>
    <div class="ctrl-row">
      <span class="ctrl-lbl">Alarm</span>
      <div><a href="/buzz/on"><button class="on">TEST</button></a><a href="/buzz/off"><button class="off">SILENCE</button></a></div>
    </div>
  </div>

</body>
</html>
)rawliteral";

// ==========================================
// 4. AUTOMATION LOGIC (Hybrid Mode)
// ==========================================
void checkAutoLogic() {
  unsigned long now = millis();

  // 1. Check if Manual Override has expired (60s timeout)
  if (manualOverride && (now - lastManualAction > MANUAL_TIMEOUT)) {
    manualOverride = false;
    Serial.println(">> Manual Mode Expired. Returning to Auto.");
  }
  // If Manual Override is active, skip all auto logic
  if (manualOverride) return;

  // 2. AUTO PUMP (Only if Tank has water)
  // Panda Plant Logic: Only water if VERY DRY (>3800)
  if (soil > 3800 && water > 1000) { 
    if (!isPumping && (now - lastPumpRun > PUMP_COOLDOWN)) {
      digitalWrite(RELAY_PIN, LOW); // ON (Active Low)
      isPumping = true;
      pumpStartTime = now;
      systemState = "AUTO: WATERING...";
    }
  }
  // Turn Pump OFF after Duration
  if (isPumping && (now - pumpStartTime >= PUMP_DURATION)) {
    digitalWrite(RELAY_PIN, HIGH); // OFF
    isPumping = false;
    lastPumpRun = now;
  }

  // 3. AUTO BUZZER (Safety)
  // If Gas is High (>2400) OR Tank is Empty (<1000)
  if (gas > 2400 || water < 1000) {
    if (!isBuzzing) {
      digitalWrite(BUZZER_PIN, HIGH);
      isBuzzing = true;
      buzzStartTime = now;
      systemState = (water < 1000) ? "ALERT: REFILL TANK" : "DANGER: GAS LEAK";
    }
  }
  if (isBuzzing && (now - buzzStartTime >= BUZZ_DURATION)) {
    digitalWrite(BUZZER_PIN, LOW);
    isBuzzing = false;
  }

  // 4. AUTO LIGHT (Daytime Only)
  struct tm timeinfo;
  // If Time Sync failed, skip this so light doesn't flicker
  if(!getLocalTime(&timeinfo)){ return; }

  int currentHour = timeinfo.tm_hour;
  bool isDaytime = (currentHour >= 7 && currentHour < 18); // 7 AM to 6 PM

  // If it is Daytime AND Light Sensor says Dark (>2800)
  if (isDaytime && light > 2800) {
     digitalWrite(LED_PIN, HIGH);
  } else {
     digitalWrite(LED_PIN, LOW);
  }
}

// ==========================================
// 5. ML DIAGNOSTICS (For Display)
// ==========================================
void runML() {
  // 1. Classify Individual Sensors
  if(temp < 15 || temp > 32) tStat = "BAD"; else tStat = "OPTIMAL";
  if(hum > 70) hStat = "RISK"; else hStat = "GOOD";
  
  // Panda Plant Specific:
  if(soil > 3800) sStat = "TOO DRY";      // Needs Water
  else if(soil < 1600) sStat = "TOO WET"; // Rot Risk
  else sStat = "OPTIMAL";                 // Healthy Range

  if(gas > 2400) gStat = "DANGER"; else gStat = "CLEAN";
  if(light > 2900) lStat = "DARK"; else lStat = "GOOD";
  
  if(water < 1000) wStat = "EMPTY"; else wStat = "FULL";

  // 2. Set Main System Status (If Auto Logic isn't running)
  if (!isPumping && !isBuzzing) {
    if(wStat == "EMPTY") systemState = "ALERT: TANK EMPTY";
    else if(gStat == "DANGER") systemState = "DANGER: BAD AIR";
    else if(sStat == "TOO DRY") systemState = "THIRSTY: NEEDS WATER";
    else if(sStat == "TOO WET") systemState = "WARNING: SOIL WET";
    else if(lStat == "DARK" && tStat == "OPTIMAL") systemState = "ALERT: LOW LIGHT";
    else systemState = "HEALTHY: OPTIMAL";
  }
}

// ==========================================
// 6. SERVER HANDLERS
// ==========================================
void handleData() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if(!isnan(t)) { temp = t; hum = h; }
  
  soil = analogRead(SOIL_PIN);
  gas = analogRead(MQ2_PIN);
  light = analogRead(LDR_PIN);
  water = analogRead(WATER_LVL_PIN);

  runML(); // Update status labels

  String json = "{";
  json += "\"t\":\"" + String(temp,1) + "\",";
  json += "\"h\":\"" + String(hum,0) + "\",";
  json += "\"s\":\"" + String(soil) + "\",";
  json += "\"g\":\"" + String(gas) + "\",";
  json += "\"l\":\"" + String(light) + "\",";
  json += "\"w\":\"" + String(water) + "\",";
  
  json += "\"ts\":\"" + tStat + "\",";
  json += "\"hs\":\"" + hStat + "\",";
  json += "\"ss\":\"" + sStat + "\",";
  json += "\"gs\":\"" + gStat + "\",";
  json += "\"ls\":\"" + lStat + "\",";
  json += "\"ws\":\"" + wStat + "\",";
  
  json += "\"st\":\"" + systemState + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleRoot() { server.send(200, "text/html", index_html); }

// Manual Controls set the Override Flag
void setManual() { manualOverride = true; lastManualAction = millis(); }

void handlePumpOn() { setManual(); if(water>1000) digitalWrite(RELAY_PIN, LOW); server.send(200, "text/plain", "OK"); }
void handlePumpOff() { setManual(); digitalWrite(RELAY_PIN, HIGH); server.send(200, "text/plain", "OK"); }
void handleLightOn() { setManual(); digitalWrite(LED_PIN, HIGH); server.send(200, "text/plain", "OK"); }
void handleLightOff() { setManual(); digitalWrite(LED_PIN, LOW); server.send(200, "text/plain", "OK"); }
void handleBuzzOn() { setManual(); digitalWrite(BUZZER_PIN, HIGH); server.send(200, "text/plain", "OK"); }
void handleBuzzOff() { setManual(); digitalWrite(BUZZER_PIN, LOW); server.send(200, "text/plain", "OK"); }

// ==========================================
// 7. SETUP (With Timeout Fixes)
// ==========================================
void setup() {
  Serial.begin(115200);
  dht.begin();
  
  pinMode(RELAY_PIN, OUTPUT); digitalWrite(RELAY_PIN, HIGH); // Off
  pinMode(BUZZER_PIN, OUTPUT); digitalWrite(BUZZER_PIN, LOW);
  pinMode(LED_PIN, OUTPUT); digitalWrite(LED_PIN, LOW);
  pinMode(WATER_LVL_PIN, INPUT);

  // 1. Connect Wi-Fi (With Timeout)
  Serial.print("Connecting to Wi-Fi");
  WiFi.begin(ssid, password);
  int retry = 0;
  while(WiFi.status() != WL_CONNECTED && retry < 20) {
    delay(500); Serial.print("."); retry++;
  }
  if(WiFi.status() == WL_CONNECTED) Serial.println("\nWiFi Connected!");
  else Serial.println("\nWiFi Failed (Check Credentials). Continuing...");

  // 2. Sync Time (With Timeout)
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.print("Syncing Time");
  struct tm timeinfo;
  retry = 0;
  // Try for 5 seconds max
  while(!getLocalTime(&timeinfo) && retry < 5) {
    delay(1000); Serial.print("."); retry++;
  }
  if(retry < 5) Serial.println("\nTime Synced!");
  else Serial.println("\nTime Sync Failed (Auto-Light disabled).");

  // 3. Start Server
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/pump/on", handlePumpOn);
  server.on("/pump/off", handlePumpOff);
  server.on("/light/on", handleLightOn);
  server.on("/light/off", handleLightOff);
  server.on("/buzz/on", handleBuzzOn);
  server.on("/buzz/off", handleBuzzOff);
  server.begin();

  Serial.println("\n=============================================");
  Serial.print("SYSTEM READY. IP ADDRESS: http://");
  Serial.println(WiFi.localIP());
  Serial.println("=============================================");
}

void loop() {
  server.handleClient();
  checkAutoLogic(); // Runs continuously
}
