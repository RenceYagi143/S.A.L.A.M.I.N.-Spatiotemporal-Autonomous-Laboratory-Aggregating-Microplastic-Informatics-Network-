#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h> 
#include <WebServer.h>   
#include <LittleFS.h>
#include <WiFiClientSecure.h>

// ==========================================
// 1. CONFIGURATION & CREDENTIALS
// ==========================================
const char* ssid = "IoT"; 
const char* password = "12345678";

// Dual Cloud Endpoints
const String SCRIPT_URL = "https://script.google.com/macros/s/AKfycbzHseYlUQqtwkILgVQxdifiCusekLKgxhCN9lrBCcpUN7kTYYcTVimcXVe1tNdTeZo/exec";
const String FIREBASE_URL = "https://gen-lang-client-0061160538-default-rtdb.asia-southeast1.firebasedatabase.app/telemetry.json";

#define ss 5
#define rst 14
#define dio0 2

#define PH_NEUTRAL_VOLTAGE (2.5) 
#define TDS_CONVERSION_FACTOR (0.5) 
#define EC_TO_SALINITY (0.55)
#define TURBIDITY_TSS_FACTOR (1.2) 

bool isOfflineMode = false;
String latestJsonData = "{}"; 

WebServer server(80);

// ==========================================
// 2. MATH & DIAGNOSTIC FUNCTIONS
// ==========================================
String getTempStatus(float t) { return (t < 24.0) ? "Sub-Optimal (Cool)" : (t <= 32.0) ? "Nominal (Optimal)" : "Critical (Overheating)"; }
String getHumStatus(float h) { return (h < 40.0) ? "Dry" : (h <= 70.0) ? "Comfortable" : "High Humidity"; }
String getAirQualityStatus(float a) { return (a <= 50.0) ? "Pristine (Clean Air)" : (a <= 100.0) ? "Moderate (Acceptable)" : "Warning (Polluted)"; }
String getPhStatus(float p) { return (p < 6.5) ? "Acidic (Warning)" : (p <= 9.0) ? "Nominal (Neutral)" : "Alkaline (Warning)"; }
String getTdsStatus(float t) { return (t < 300.0) ? "Pristine (Low Mineral)" : (t <= 1000.0) ? "Nominal (Freshwater)" : "Critical (Contaminated)"; }
String getSalinityStatus(float s) { return (s < 1.0) ? "Freshwater Baseline" : (s <= 10.0) ? "Brackish Mixing Zone" : "Saline Intrusion"; }
String getTurbidityStatus(float t) { return (t < 15.0) ? "Pristine (Clear)" : (t <= 80.0) ? "Moderate (Cloudy)" : "Critical (High Debris)"; }
String getPressureStatus(float hpa) { return (hpa < 1000.0) ? "Low (Storm Risk)" : (hpa <= 1022.0) ? "Stable / Nominal" : "High (Clear Skies)"; }
String getGasStatus(float ohms) { return (ohms < 10.0) ? "High VOCs Detected" : (ohms <= 50.0) ? "Moderate VOCs" : "Air Stabilized"; }
String getAltitudeStatus(float alt) { return (alt < 0.0) ? "Below Sea Level" : (alt <= 500.0) ? "Lowland / Coastal" : "Highland / Elevated"; }
String getDewPointStatus(float dp) { return (dp < 18.0) ? "Dry / Crisp" : (dp <= 24.0) ? "Muggy" : "Condensation / Fog Risk"; }
String getHeatIndexStatus(float hi) { return (hi < 27.0) ? "Safe" : (hi <= 32.0) ? "Caution" : (hi <= 41.0) ? "Extreme Caution" : "Danger (Heat Stroke)"; }
String getEcStatus(float ec) { return (ec < 50.0) ? "Ultra-Pure Water" : (ec <= 800.0) ? "Nominal Freshwater" : "High Ion Concentration"; }
String getVoltageDiagnostic(float v) { return (v < 0.1) ? "Low Signal (Check Wire!)" : (v <= 3.3) ? "Nominal / Healthy Flow" : "High Signal Limit"; }

String getNIPStatus(float nip) { return (nip < 1.0) ? "Pristine (Trace Particulates)" : (nip <= 8.0) ? "Low Particulate Matter" : (nip <= 25.0) ? "Moderate Suspended Debris" : "CRITICAL (High Non-Ionic Load)"; }

float calculateDewPoint(float tempC, float humidity) {
  float gamma = (17.27 * tempC) / (237.3 + tempC) + log(humidity / 100.0);
  return (237.3 * gamma) / (17.27 - gamma);
}
float calculateHeatIndex(float tempC, float humidity) {
  float tempF = (tempC * 9.0/5.0) + 32.0;
  float hi = -42.379 + 2.04901523*tempF + 10.14333127*humidity - 0.22475541*tempF*humidity - 6.83783e-3*tempF*tempF - 5.481717e-2*humidity*humidity + 1.22874e-3*tempF*tempF*humidity + 8.5282e-4*tempF*humidity*humidity - 1.99e-6*tempF*tempF*humidity*humidity;
  return (hi - 32.0) * 5.0/9.0; 
}

// ==========================================
// 3. FRONTEND DASHBOARD HTML
// ==========================================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>Dr.One Command Center</title>
  <style>
    body { background-color: #0d1117; color: #00ff00; font-family: 'Courier New', Courier, monospace; margin: 0; padding: 20px; }
    h1 { text-align: center; border-bottom: 2px solid #00ff00; padding-bottom: 10px; }
    table { width: 100%; max-width: 950px; margin: 20px auto; border-collapse: collapse; border: 1px solid #00ff00; }
    th, td { border: 1px solid #00ff00; padding: 10px; text-align: left; }
    th { background-color: #161b22; }
    .sys-info { text-align: center; margin-bottom: 5px; font-size: 1.2em;}
    .clock { text-align: center; color: #888; font-size: 0.9em; margin-bottom: 20px; }
    .controls { border: 1px dashed #00ff00; padding: 15px; text-align: center; max-width: 600px; margin: 0 auto 20px auto; background-color: #161b22;}
    .btn { background-color: #00ff00; color: #000; padding: 8px 15px; text-decoration: none; font-weight: bold; border-radius: 3px; cursor: pointer; border: none; font-size: 1em;}
    .btn:hover { background-color: #00cc00; }
    .btn-danger { background-color: #ff4444; color: white; width: 100%; margin-top: 15px;}
    .btn-danger:hover { background-color: #cc0000; }
    .dropdown { padding: 8px; font-size: 1em; background-color: #0d1117; color: #00ff00; border: 1px solid #00ff00; border-radius: 3px; width: 250px; outline: none;}
    .download-row { display: flex; justify-content: center; gap: 10px; align-items: center; margin-top: 15px; }
  </style>
</head>
<body>
  <h1>DR.ONE COMMAND CENTER: LIVE TELEMETRY</h1>
  
  <div class="sys-info">
      Signal Strength (RSSI): <span id="rssi">--</span> dBm | Connection: <span id="net-status">--</span>
  </div>
  
  <div class="clock">
      Last Data Received: <span id="timestamp">Booting receiver array...</span>
  </div>

  <div class="controls">
      <h3 style="margin-top: 0; color: #fff;">📁 DATA LOG MANAGER</h3>
      
      <div class="download-row">
        <select id="file-dropdown" class="dropdown">
          <option value="">Scanning memory...</option>
        </select>
        <button onclick="downloadSelected()" class="btn">💾 DOWNLOAD</button>
      </div>

      <button onclick="deleteData()" class="btn btn-danger">🗑️ CLEAR MEMORY</button>
  </div>
  
  <table>
    <tr><th>PARAMETER</th><th>VALUE</th><th>UNIT</th><th>DIAGNOSTIC STATUS</th></tr>
    
    <tr><td colspan="4" style="background-color: #161b22; text-align:center; color: #fff;">--- AIR QUALITY ---</td></tr>
    
    <tr><td>Air Temperature</td><td id="a_temp">--</td><td>C</td><td id="stat_a_temp">--</td></tr>
    <tr><td>Relative Humidity</td><td id="a_hum">--</td><td>%</td><td id="stat_a_hum">--</td></tr>
    <tr><td>Barometric Pressure</td><td id="a_pres">--</td><td>hPa</td><td id="stat_a_pres">--</td></tr>
    <tr><td>Air Quality Index</td><td id="a_aqi">--</td><td>AQI</td><td id="stat_a_aqi">--</td></tr>
    <tr><td>Gas Resist (VOCs)</td><td id="a_gas">--</td><td>KOhms</td><td id="stat_a_gas">--</td></tr>
    <tr><td>Est. Altitude</td><td id="a_alt">--</td><td>Meters</td><td id="stat_a_alt">--</td></tr>
    <tr><td>Dew Point</td><td id="a_dew">--</td><td>C</td><td id="stat_a_dew">--</td></tr>
    <tr><td>Heat Index</td><td id="a_heat">--</td><td>C</td><td id="stat_a_heat">--</td></tr>
    
    <tr><td colspan="4" style="background-color: #161b22; text-align:center; color: #fff;">--- WATER QUALITY ---</td></tr>
    <tr><td>Water Temperature</td><td id="w_temp">--</td><td>C</td><td id="stat_w_temp">--</td></tr>
    <tr><td>pH Level</td><td id="w_ph">--</td><td>pH</td><td id="stat_w_ph">--</td></tr>
    <tr><td>Total Diss. Solids</td><td id="w_tds">--</td><td>ppm</td><td id="stat_w_tds">--</td></tr>
    <tr><td>Elec. Conductivity</td><td id="w_ec">--</td><td>uS/cm</td><td id="stat_w_ec">--</td></tr>
    <tr><td>Salinity Profile</td><td id="w_sal">--</td><td>ppt</td><td id="stat_w_sal">--</td></tr>
    <tr><td>Turbidity</td><td id="w_turb">--</td><td>NTU</td><td id="stat_w_turb">--</td></tr>
    <tr><td>Total Susp. Solids</td><td id="w_tss">--</td><td>mg/L</td><td id="stat_w_tss">--</td></tr>
    <tr><td style="color:#ffcc00; font-weight:bold;">Non-Ionic Particulates</td><td id="w_nip" style="color:#ffcc00; font-weight:bold;">--</td><td style="color:#ffcc00;">mg/L</td><td id="stat_w_nip" style="color:#ffcc00;">--</td></tr>

    <tr><td colspan="4" style="background-color: #161b22; text-align:center; color: #fff;">--- HARDWARE DIAGNOSTICS ---</td></tr>
    <tr><td>Raw pH Voltage</td><td id="v_ph">--</td><td>Volts</td><td id="stat_v_ph">--</td></tr>
    <tr><td>Raw TDS Voltage</td><td id="v_tds">--</td><td>Volts</td><td id="stat_v_tds">--</td></tr>
    <tr><td>Raw Turb Voltage</td><td id="v_turb">--</td><td>Volts</td><td id="stat_v_turb">--</td></tr>
  </table>

  <script>
    function deleteData() {
      if(confirm("⚠️ WARNING: This will permanently wipe all CSV files!")) {
        fetch('/delete', { method: 'POST' })
          .then(response => {
            if(response.ok) { 
              alert("SUCCESS: Memory formatted completely!"); 
              document.getElementById('file-dropdown').innerHTML = '<option value="">Scanning memory...</option>';
              fetchFiles(); 
            }
          });
      }
    }

    function downloadSelected() {
      const dropdown = document.getElementById('file-dropdown');
      const selectedFile = dropdown.value;
      if (selectedFile) {
        window.location.href = `/download?file=${selectedFile}`;
      } else {
        alert("No files available to download yet.");
      }
    }

    function fetchFiles() {
      fetch('/api/files')
        .then(response => response.json())
        .then(files => {
          const dropdown = document.getElementById('file-dropdown');
          if(files.length === 0) {
             dropdown.innerHTML = '<option value="">No logs recorded yet...</option>';
             return;
          }
          if (dropdown.options.length !== files.length || dropdown.options[0].value === "") {
              dropdown.innerHTML = "";
              files.forEach(file => {
                 dropdown.innerHTML += `<option value="${file}">${file}</option>`;
              });
          }
        });
    }

    // --- SYNCHRONIZATION FIX: Locked to exactly 10 seconds (10000 ms) ---
    setInterval(function() {
      fetch('/api/live').then(response => response.json()).then(data => {
          
          if (data.status) {
             document.getElementById('net-status').innerText = data.status;
          }

          if (!data.hardware) {
             document.getElementById('timestamp').innerText = "Awaiting Dr.One LoRa Telemetry...";
             return; 
          }

          document.getElementById('rssi').innerText = data.rssi;
          document.getElementById('timestamp').innerText = new Date().toLocaleTimeString();

          document.getElementById('a_temp').innerText = data.hardware.air_temp_c;
          document.getElementById('a_hum').innerText = data.hardware.air_hum;
          document.getElementById('a_pres').innerText = data.hardware.air_pres;
          document.getElementById('a_gas').innerText = data.hardware.air_gas;
          document.getElementById('a_alt').innerText = data.derived.altitude_m.toFixed(2);
          document.getElementById('a_dew').innerText = data.derived.dew_point.toFixed(2);
          document.getElementById('a_heat').innerText = data.derived.heat_idx.toFixed(2);
          document.getElementById('a_aqi').innerText = data.derived.iaq.toFixed(1);

          document.getElementById('w_temp').innerText = data.hardware.water_temp_c;
          document.getElementById('w_ph').innerText = data.derived.compensated_ph.toFixed(2);
          document.getElementById('w_tds').innerText = data.derived.tds_ppm.toFixed(1);
          document.getElementById('w_ec').innerText = data.derived.ec_us.toFixed(1);
          document.getElementById('w_sal').innerText = data.derived.salinity_ppt.toFixed(3);
          document.getElementById('w_turb').innerText = data.derived.turb_ntu.toFixed(1);
          document.getElementById('w_tss').innerText = data.derived.tss.toFixed(1);
          document.getElementById('w_nip').innerText = data.derived.nip_density.toFixed(2);

          document.getElementById('v_ph').innerText = data.hardware.raw_ph_v.toFixed(3);
          document.getElementById('v_tds').innerText = data.hardware.raw_tds_v.toFixed(3);
          document.getElementById('v_turb').innerText = data.hardware.raw_turb_v.toFixed(3);

          document.getElementById('stat_a_temp').innerText = data.analysis.a_temp_stat;
          document.getElementById('stat_a_hum').innerText = data.analysis.a_hum_stat;
          document.getElementById('stat_a_pres').innerText = data.analysis.a_pres_stat;
          document.getElementById('stat_a_gas').innerText = data.analysis.a_gas_stat;
          document.getElementById('stat_a_alt').innerText = data.analysis.a_alt_stat;
          document.getElementById('stat_a_dew').innerText = data.analysis.a_dew_stat;
          document.getElementById('stat_a_heat').innerText = data.analysis.a_heat_stat;
          document.getElementById('stat_a_aqi').innerText = data.analysis.a_aqi_stat;
          document.getElementById('stat_w_temp').innerText = data.analysis.w_temp_stat;
          document.getElementById('stat_w_ph').innerText = data.analysis.w_ph_stat;
          document.getElementById('stat_w_tds').innerText = data.analysis.w_tds_stat;
          document.getElementById('stat_w_ec').innerText = data.analysis.w_ec_stat;
          document.getElementById('stat_w_sal').innerText = data.analysis.w_sal_stat;
          document.getElementById('stat_w_turb').innerText = data.analysis.w_turb_stat;
          document.getElementById('stat_w_tss').innerText = data.analysis.w_turb_stat; 
          document.getElementById('stat_w_nip').innerText = data.analysis.w_nip_stat;
          document.getElementById('stat_v_ph').innerText = data.analysis.v_ph_stat;
          document.getElementById('stat_v_tds').innerText = data.analysis.v_tds_stat;
          document.getElementById('stat_v_turb').innerText = data.analysis.v_turb_stat;
      });
      fetchFiles(); 
    }, 10000); 
    
    window.onload = fetchFiles;
  </script>
</body>
</html>
)rawliteral";

// ==========================================
// 4. DYNAMIC FILE SYSTEM LOGGING
// ==========================================
void appendDataToStorage(String dataString, int filePartNumber) {
  String fileName = "/Log_Part_" + String(filePartNumber) + ".csv";
  File file = LittleFS.open(fileName, FILE_APPEND);
  if (file) { 
    file.print(dataString); 
    file.close(); 
  }
}

// ==========================================
// 4.5 URL ENCODER (Memory Optimized)
// ==========================================
String urlEncode(String str) {
  String encodedString = "";
  encodedString.reserve(str.length() * 3); 
  char c; char code0; char code1;
  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (c == ' ') { encodedString += '+'; } 
    else if (isalnum(c)) { encodedString += c; } 
    else {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9) { code1 = (c & 0xf) - 10 + 'A'; }
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9) { code0 = c - 10 + 'A'; }
      encodedString += '%';
      encodedString += code0;
      encodedString += code1;
    }
  }
  return encodedString;
}

// ==========================================
// 5. MAIN SETUP
// ==========================================
void setup() {
  Serial.begin(115200);
  Serial.println("\nInitializing Dr.One GROUND STATION...");

  LittleFS.begin(true);

  WiFi.mode(WIFI_AP_STA); 
  int maxRetries = 3; int currentRetry = 0; bool connected = false;

  while (currentRetry < maxRetries && !connected) {
    Serial.printf("\n>> Wi-Fi Connection Attempt %d of %d...\n", currentRetry + 1, maxRetries);
    WiFi.begin(ssid, password);
    int timeoutWait = 0;
    while (WiFi.status() != WL_CONNECTED && timeoutWait < 20) { delay(500); Serial.print("."); timeoutWait++; }

    if (WiFi.status() == WL_CONNECTED) { connected = true; } 
    else { Serial.println("\n>> Timeout reached. Resetting modem..."); WiFi.disconnect(); delay(1000); currentRetry++; }
  }

  // --- UI FIX: Clearly establishing the Black Box Doctrine ---
  if (connected) {
    Serial.println("\n>> SUCCESS: Cloud Uplink Established.");
    isOfflineMode = false;
    latestJsonData = "{\"status\":\"ONLINE + LOCAL BACKUP\"}"; 
  } else {
    Serial.println("\n>> WARNING: Deploying OFFLINE COMMAND CENTER.");
    isOfflineMode = true;
    latestJsonData = "{\"status\":\"OFFLINE (LOCAL ONLY)\"}"; 
    WiFi.softAP("DrOne_Command_Center", ""); 
    Serial.print(">> AP IP Address: "); Serial.println(WiFi.softAPIP());
  }

  server.on("/", HTTP_GET, []() { server.send(200, "text/html", index_html); });
  
  server.on("/api/live", HTTP_GET, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", latestJsonData);
  });

  server.on("/api/files", HTTP_GET, []() {
    File root = LittleFS.open("/");
    File file = root.openNextFile();
    String jsonArray = "[";
    bool first = true;
    while(file) {
      String fname = String(file.name());
      if(fname.startsWith("Log_Part")) {
        if(!first) jsonArray += ",";
        jsonArray += "\"" + fname + "\"";
        first = false;
      }
      file = root.openNextFile();
    }
    jsonArray += "]";
    server.send(200, "application/json", jsonArray);
  });

  server.on("/download", HTTP_GET, []() {
    String fileName = "/" + server.arg("file");
    if (LittleFS.exists(fileName)) {
      File file = LittleFS.open(fileName, FILE_READ);
      server.sendHeader("Content-Disposition", "attachment; filename=" + server.arg("file"));
      server.streamFile(file, "text/csv");
      file.close();
    } else {
      server.send(404, "text/plain", "Error: File not found.");
    }
  });

  // THE NEW TOTAL NUCLEAR MEMORY WIPE
  server.on("/delete", HTTP_POST, []() {
    Serial.println("\n>> WARNING: TOTAL MEMORY FORMAT INITIATED!");
    LittleFS.format();
    LittleFS.begin(true);
    Serial.println(">> SUCCESS: Memory drive has been completely wiped clean.");
    server.send(200, "text/plain", "Memory formatted completely.");
  });
  
  server.begin();

  LoRa.setPins(ss, rst, dio0);
  if (!LoRa.begin(433E6)) { Serial.println(">> Error: LoRa Radio Init Failed!"); while (1); }
  LoRa.setSpreadingFactor(12);
  LoRa.setSignalBandwidth(125E3);
  Serial.println("\n[ SYSTEM READY ] Listening for Dr.One Transmissions...\n");
}

// ==========================================
// 6. MAIN LOOP
// ==========================================
void loop() {
  server.handleClient(); 

  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    
    Serial.println("\n==================================================");
    Serial.println("[!] ANTENNA TRIGGERED! Incoming radio wave detected...");
    Serial.printf(">> Raw Packet Size: %d bytes\n", packetSize);
    Serial.printf(">> Signal Strength (RSSI): %d dBm\n", LoRa.packetRssi());

    String incoming = "";
    while (LoRa.available()) { incoming += (char)LoRa.read(); }
    
    Serial.print(">> RAW PAYLOAD READ: ");
    Serial.println(incoming);

    float values[9] = {0}; 
    int current_index = 0;
    for (int i = 0; i < 9; i++) {
      int next_comma = incoming.indexOf(',', current_index);
      if (next_comma == -1) next_comma = incoming.length();
      if (current_index < incoming.length()) {
         values[i] = incoming.substring(current_index, next_comma).toFloat();
      }
      current_index = next_comma + 1;
    }

    float a_temp = values[0]; float a_hum = values[1]; float a_pres = values[2]; float a_gas = values[3];
    float w_temp = values[4]; float v_ph = values[5];  float v_tds = values[6];  float v_turb = values[7];
    float nip_density = values[8]; 

    float comp_pH   = 7.0 + ((PH_NEUTRAL_VOLTAGE - v_ph) / (0.1984 * (w_temp + 273.15)));
    float tds_ppm   = v_tds * 250.0;        
    float turb_ntu  = v_turb * 800.0;        
    float ec_us_cm  = tds_ppm / TDS_CONVERSION_FACTOR;
    float salinity  = ec_us_cm * EC_TO_SALINITY / 1000.0; 
    float tss       = turb_ntu * TURBIDITY_TSS_FACTOR;
    float iaq_est   = log10(50.0 / (a_gas == 0 ? 1 : a_gas)) * 50.0;
    float dew_point = calculateDewPoint(a_temp, a_hum);
    float heat_idx  = calculateHeatIndex(a_temp, a_hum);
    float altitude_m = 44330.0 * (1.0 - pow(a_pres / 1013.25, 0.1903)); 

    Serial.println("\n>> RECONSTRUCTING PARAMETERS LOCALLY:");
    Serial.printf("   1-4 (Air): Temp=%.2fC, Hum=%.2f%%, Pres=%.2fhPa, Gas=%.2fKOhms\n", a_temp, a_hum, a_pres, a_gas);
    Serial.printf("   5-8 (Raw Water): Temp=%.2fC, V_pH=%.2fV, V_TDS=%.2fV, V_Turb=%.2fV\n", w_temp, v_ph, v_tds, v_turb);
    Serial.printf("   9-13 (Water Math): pH=%.2f, TDS=%.2fppm, EC=%.2fuS, Sal=%.3fppt, Turb=%.2fNTU\n", comp_pH, tds_ppm, ec_us_cm, salinity, turb_ntu);
    Serial.printf("   14-19 (Advanced): TSS=%.2f, IAQ=%.2f, Alt=%.2fm, Dew=%.2fC, HeatIdx=%.2fC, NIP=%.2fmg/L\n", tss, iaq_est, altitude_m, dew_point, heat_idx, nip_density);

    JsonDocument doc;
    doc["status"] = isOfflineMode ? "OFFLINE (LOCAL ONLY)" : "ONLINE + LOCAL BACKUP";
    doc["rssi"] = LoRa.packetRssi();
    
    doc["hardware"]["air_temp_c"] = a_temp;
    doc["hardware"]["air_hum"] = a_hum;
    doc["hardware"]["air_pres"] = a_pres;
    doc["hardware"]["air_gas"] = a_gas;
    doc["hardware"]["water_temp_c"] = w_temp; 
    doc["hardware"]["raw_ph_v"] = v_ph; 
    doc["hardware"]["raw_tds_v"] = v_tds; 
    doc["hardware"]["raw_turb_v"] = v_turb;
    
    doc["derived"]["altitude_m"] = altitude_m;
    doc["derived"]["dew_point"] = dew_point;
    doc["derived"]["heat_idx"] = heat_idx;
    doc["derived"]["iaq"] = iaq_est;
    doc["derived"]["compensated_ph"] = comp_pH; 
    doc["derived"]["tds_ppm"] = tds_ppm; 
    doc["derived"]["ec_us"] = ec_us_cm;
    doc["derived"]["salinity_ppt"] = salinity; 
    doc["derived"]["turb_ntu"] = turb_ntu;
    doc["derived"]["tss"] = tss;
    doc["derived"]["nip_density"] = nip_density; 
    
    doc["analysis"]["a_temp_stat"] = getTempStatus(a_temp);
    doc["analysis"]["a_hum_stat"] = getHumStatus(a_hum);
    doc["analysis"]["a_pres_stat"] = getPressureStatus(a_pres);
    doc["analysis"]["a_gas_stat"] = getGasStatus(a_gas);
    doc["analysis"]["a_alt_stat"] = getAltitudeStatus(altitude_m);
    doc["analysis"]["a_dew_stat"] = getDewPointStatus(dew_point);
    doc["analysis"]["a_heat_stat"] = getHeatIndexStatus(heat_idx);
    doc["analysis"]["a_aqi_stat"] = getAirQualityStatus(iaq_est);
    doc["analysis"]["w_temp_stat"] = getTempStatus(w_temp);
    doc["analysis"]["w_ph_stat"] = getPhStatus(comp_pH); 
    doc["analysis"]["w_tds_stat"] = getTdsStatus(tds_ppm);
    doc["analysis"]["w_ec_stat"] = getEcStatus(ec_us_cm);
    doc["analysis"]["w_sal_stat"] = getSalinityStatus(salinity);
    doc["analysis"]["w_turb_stat"] = getTurbidityStatus(tss);
    doc["analysis"]["w_nip_stat"] = getNIPStatus(nip_density); 
    doc["analysis"]["v_ph_stat"] = getVoltageDiagnostic(v_ph);
    doc["analysis"]["v_tds_stat"] = getVoltageDiagnostic(v_tds);
    doc["analysis"]["v_turb_stat"] = getVoltageDiagnostic(v_turb);

    latestJsonData = ""; 
    serializeJson(doc, latestJsonData);

    int filePartNumber = (millis() / 300000) + 1;
    float elapsedSeconds = millis() / 1000.0;
    
    String block = "MISSION ELAPSED TIME: " + String(elapsedSeconds) + " Seconds,,,\n";
    block += "PARAMETER,VALUE,UNIT,DIAGNOSTIC STATUS\n";
    
    block += "Air Temperature," + String(a_temp) + ",C," + getTempStatus(a_temp) + "\n";
    block += "Relative Humidity," + String(a_hum) + ",%," + getHumStatus(a_hum) + "\n";
    block += "Barometric Pressure," + String(a_pres) + ",hPa," + getPressureStatus(a_pres) + "\n";
    block += "Air Quality Index," + String(iaq_est) + ",AQI," + getAirQualityStatus(iaq_est) + "\n";
    block += "Gas Resist (VOCs)," + String(a_gas) + ",KOhms," + getGasStatus(a_gas) + "\n";
    block += "Est. Altitude," + String(altitude_m) + ",Meters," + getAltitudeStatus(altitude_m) + "\n";
    block += "Dew Point," + String(dew_point) + ",C," + getDewPointStatus(dew_point) + "\n";
    block += "Heat Index," + String(heat_idx) + ",C," + getHeatIndexStatus(heat_idx) + "\n";
    
    block += "Water Temperature," + String(w_temp) + ",C," + getTempStatus(w_temp) + "\n";
    block += "pH Level," + String(comp_pH) + ",pH," + getPhStatus(comp_pH) + "\n";
    block += "Total Diss. Solids," + String(tds_ppm) + ",ppm," + getTdsStatus(tds_ppm) + "\n";
    block += "Elec. Conductivity," + String(ec_us_cm) + ",uS/cm," + getEcStatus(ec_us_cm) + "\n";
    block += "Salinity Profile," + String(salinity) + ",ppt," + getSalinityStatus(salinity) + "\n";
    block += "Turbidity," + String(turb_ntu) + ",NTU," + getTurbidityStatus(tss) + "\n";
    block += "Total Susp. Solids," + String(tss) + ",mg/L," + getTurbidityStatus(tss) + "\n";
    block += "Non-Ionic Particulates," + String(nip_density) + ",mg/L," + getNIPStatus(nip_density) + "\n";
    
    block += "Raw pH Voltage," + String(v_ph) + ",Volts," + getVoltageDiagnostic(v_ph) + "\n";
    block += "Raw TDS Voltage," + String(v_tds) + ",Volts," + getVoltageDiagnostic(v_tds) + "\n";
    block += "Raw Turb Voltage," + String(v_turb) + ",Volts," + getVoltageDiagnostic(v_turb) + "\n";
    
    block += "\n\n";

    appendDataToStorage(block, filePartNumber);
    Serial.printf("\n>> SUCCESS: 19 Parameters locally stored in Log_Part_%d.csv\n", filePartNumber);

    if (!isOfflineMode && WiFi.status() == WL_CONNECTED) {
      
      // ==========================================
      // PHASE 1: FIREBASE REAL-TIME TELEMETRY (For Dashboard)
      // ==========================================
      Serial.println(">> Transmitting Telemetry Payload to Firebase...");
      HTTPClient httpFirebase;
      httpFirebase.begin(FIREBASE_URL);
      httpFirebase.addHeader("Content-Type", "application/json");
      
      int fbResponseCode = httpFirebase.PUT(latestJsonData);
      
      if (fbResponseCode > 0) {
        Serial.printf(">> SUCCESS: Firebase Response Code %d\n", fbResponseCode);
      } else {
        Serial.printf(">> HTTP Error (Firebase): %s\n", httpFirebase.errorToString(fbResponseCode).c_str());
      }
      httpFirebase.end();

      // ==========================================
      // PHASE 2: GOOGLE SHEETS ARCHIVE (For Hard Logging)
      // ==========================================
      Serial.println(">> Transmitting to Google Space Command...");
      WiFiClientSecure client;
      client.setInsecure(); 
      
      HTTPClient httpSheets;
      httpSheets.setTimeout(10000); 
      
      String finalURL = SCRIPT_URL + "?payload=" + urlEncode(latestJsonData);
      
      httpSheets.begin(client, finalURL);
      httpSheets.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS); 
      
      int sheetsResponseCode = httpSheets.GET(); 
      
      if (sheetsResponseCode == 200) {
        Serial.println(">> Google Sheets Uplink: FLIGHT LOGGED SUCCESSFULLY (Code: 200)");
      } else if (sheetsResponseCode > 0) {
        Serial.printf(">> Google Sheets Uplink: GOOGLE REJECTED DATA (Code: %d)\n", sheetsResponseCode);
      } else {
        Serial.printf(">> Google Sheets Uplink: CONNECTION FAILED (%s)\n", httpSheets.errorToString(sheetsResponseCode).c_str());
      }
      httpSheets.end();
    }
    
    Serial.println("==================================================\n");
  }
}