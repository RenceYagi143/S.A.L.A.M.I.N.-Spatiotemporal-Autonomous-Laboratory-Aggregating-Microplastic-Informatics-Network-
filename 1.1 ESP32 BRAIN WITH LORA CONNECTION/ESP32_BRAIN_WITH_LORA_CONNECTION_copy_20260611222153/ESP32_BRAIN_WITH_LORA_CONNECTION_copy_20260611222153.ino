#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <math.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>
#include <Adafruit_ADS1X15.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// ==========================================
// 1. HARDWARE PINS & CONSTANTS
// ==========================================
// LoRa SPI Pins
#define ss 5
#define rst 14
#define dio0 2

// Sensor Objects
Adafruit_BME680 bme; 
Adafruit_ADS1115 ads;

#define ONE_WIRE_BUS 4
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature waterTemp(&oneWire);

#define SEALEVELPRESSURE_HPA (1013.25)
#define PH_NEUTRAL_VOLTAGE (2.5) 
#define TDS_CONVERSION_FACTOR (0.5) 
#define EC_TO_SALINITY (0.55)
#define TURBIDITY_TSS_FACTOR (1.2) 

// ==========================================
// 2. MATH FUNCTIONS
// ==========================================
float calculateDewPoint(float tempC, float humidity) {
  float gamma = (17.27 * tempC) / (237.3 + tempC) + log(humidity / 100.0);
  return (237.3 * gamma) / (17.27 - gamma);
}

float calculateHeatIndex(float tempC, float humidity) {
  float tempF = (tempC * 9.0/5.0) + 32.0;
  float hi = -42.379 + 2.04901523*tempF + 10.14333127*humidity 
             - 0.22475541*tempF*humidity - 6.83783e-3*tempF*tempF 
             - 5.481717e-2*humidity*humidity + 1.22874e-3*tempF*tempF*humidity 
             + 8.5282e-4*tempF*humidity*humidity - 1.99e-6*tempF*tempF*humidity*humidity;
  return (hi - 32.0) * 5.0/9.0; 
}

float calculateCompensatedPH(float rawVoltage, float waterTempC) {
  float kelvin = waterTempC + 273.15;
  float nernstSlope = 0.1984 * kelvin; 
  return 7.0 + ((PH_NEUTRAL_VOLTAGE - rawVoltage) / nernstSlope);
}

// Advanced Multi-Variable Non-Ionic Particulate (NIP) Soft-Sensor
float calculateNonIonicParticulates(float v_turb, float tds_ppm, float comp_pH, float waterTempC) {
  
  // Turbidity sensor outputs ~4.2V in completely clear water.
  float scatterIntensity = 4.2 - v_turb;
  if (scatterIntensity < 0) scatterIntensity = 0;

  // We offset ionic natural minerals (TDS) to isolate non-conductive physical particles.
  float mineralOffset = tds_ppm * 0.0015;
  
  // Temperature-dependent water density correction
  float tempCorrection = 1.0 + (0.002 * (waterTempC - 25.0));

  // Compute final Non-Ionic Particulate (NIP) mass estimation
  float nip_density = (scatterIntensity * 18.64) - mineralOffset;
  
  if (comp_pH < 6.0 || comp_pH > 8.5) {
    nip_density *= 1.08; 
  }
  
  nip_density *= tempCorrection;
  
  // Floor the value at 0 so we don't return negative mass, but allow raw high values.
  if (nip_density < 0.0) nip_density = 0.0;
  return nip_density;
}

// ==========================================
// 3. DIAGNOSTIC RULES
// ==========================================
String getTempStatus(float temp) {
  if (temp < 24.0) return "Sub-Optimal (Cool)";
  if (temp <= 32.0) return "Nominal (Optimal)";
  return "Critical (Overheating)";
}
String getHumStatus(float hum) {
  if (hum < 40.0) return "Dry";
  if (hum <= 70.0) return "Comfortable";
  return "High Humidity";
}
String getAirQualityStatus(float aqi) {
  if (aqi <= 50.0) return "Pristine (Clean Air)";
  if (aqi <= 100.0) return "Moderate (Acceptable)";
  return "Warning (Polluted)";
}
String getPhStatus(float ph) {
  if (ph < 6.5) return "Acidic (Warning)";
  if (ph <= 9.0) return "Nominal (Neutral)";
  return "Alkaline (Warning)";
}
String getTdsStatus(float tds) {
  if (tds < 300.0) return "Pristine (Low Mineral)";
  if (tds <= 1000.0) return "Nominal (Freshwater)";
  return "Critical (Contaminated)";
}
String getSalinityStatus(float sal) {
  if (sal < 1.0) return "Freshwater Baseline";
  if (sal <= 10.0) return "Brackish Mixing Zone";
  return "Saline Intrusion";
}
String getTurbidityStatus(float tss) {
  if (tss < 15.0) return "Pristine (Clear)";
  if (tss <= 80.0) return "Moderate (Cloudy)";
  return "Critical (High Debris)";
}
String getPressureStatus(float hpa) {
  if (hpa < 1000.0) return "Low (Storm Risk)";
  if (hpa <= 1022.0) return "Stable / Nominal";
  return "High (Clear Skies)";
}
String getGasStatus(float ohms) {
  if (ohms < 10.0) return "High VOCs Detected";
  if (ohms <= 50.0) return "Moderate VOCs";
  return "Air Stabilized";
}
String getAltitudeStatus(float alt) {
  if (alt < 0.0) return "Below Sea Level";
  if (alt <= 500.0) return "Lowland / Coastal";
  return "Highland / Elevated";
}
String getDewPointStatus(float dp) {
  if (dp < 18.0) return "Dry / Crisp";
  if (dp <= 24.0) return "Muggy";
  return "Condensation / Fog Risk";
}
String getHeatIndexStatus(float hi) {
  if (hi < 27.0) return "Safe";
  if (hi <= 32.0) return "Caution";
  if (hi <= 41.0) return "Extreme Caution";
  return "Danger (Heat Stroke)";
}
String getEcStatus(float ec) {
  if (ec < 50.0) return "Ultra-Pure Water";
  if (ec <= 800.0) return "Nominal Freshwater";
  return "High Ion Concentration";
}
String getVoltageDiagnostic(float v) {
  if (v < 0.1) return "Low Signal (Check Wire!)";
  if (v <= 3.3) return "Nominal / Healthy Flow";
  return "High Signal Limit";
}
String getNIPStatus(float nip) {
  if (nip < 1.0) return "Pristine (Trace Particulates)";
  if (nip <= 8.0) return "Low Particulate Matter";
  if (nip <= 25.0) return "Moderate Suspended Debris";
  return "CRITICAL (High Non-Ionic Load)";
}

// ==========================================
// 4. MAIN SETUP
// ==========================================
void setup() {
  Serial.begin(115200);
  Serial.println("\nInitializing Dr.One Master Analytical Engine & LoRa Bridge...");

  // Boot Sensors
  if (!bme.begin()) { Serial.println("Error: BME680 Missing! Check I2C."); while (1); }
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150);

  if (!ads.begin(0x48)) { Serial.println("Error: ADS1115 Missing! Check I2C."); while (1); }
  ads.setGain(GAIN_ONE); 
  waterTemp.begin();

  // Boot LoRa Radio
  LoRa.setPins(ss, rst, dio0);
  
  if (!LoRa.begin(433E6)) {
    Serial.println("Error: LoRa Radio Initialization Failed!");
    while (1);
  }
  
  // Maximize range and penetration over water
  LoRa.setTxPower(20);
  LoRa.setSpreadingFactor(12);
  LoRa.setSignalBandwidth(125E3);

  Serial.println("\nSUCCESS: LoRa Transmitter Online!");
  Serial.println("All Systems GO! Commencing analysis stream...\n");
  delay(1000);
}

// ==========================================
// 5. MAIN LOOP
// ==========================================
void loop() {
  // --- A. Read Raw Hardware ---
  bme.performReading();
  waterTemp.requestTemperatures();
  
  float a_temp  = bme.temperature;
  float a_hum   = bme.humidity;
  float a_pres  = bme.pressure / 100.0;
  float a_gas   = bme.gas_resistance / 1000.0;
  
  float w_temp  = waterTemp.getTempCByIndex(0);
  float v_ph    = ads.computeVolts(ads.readADC_SingleEnded(0));
  float v_tds   = ads.computeVolts(ads.readADC_SingleEnded(1));
  float v_turb  = ads.computeVolts(ads.readADC_SingleEnded(2));

  // --- B. Execute Math (Soft Sensors) ---
  float alt       = bme.readAltitude(SEALEVELPRESSURE_HPA); 
  float dew_point = calculateDewPoint(a_temp, a_hum);
  float heat_idx  = calculateHeatIndex(a_temp, a_hum);
  float iaq_est   = log10(50.0 / (a_gas == 0 ? 1 : a_gas)) * 50.0; 
  
  float comp_pH   = calculateCompensatedPH(v_ph, w_temp);
  float tds_ppm   = v_tds * 250.0;        
  float turb_ntu  = v_turb * 800.0;        
  float ec_us_cm  = tds_ppm / TDS_CONVERSION_FACTOR;
  float salinity  = ec_us_cm * EC_TO_SALINITY / 1000.0; 
  float tss       = turb_ntu * TURBIDITY_TSS_FACTOR;

  // Calculate Non-Ionic Particulates (NIP)
  float nip_density = calculateNonIonicParticulates(v_turb, tds_ppm, comp_pH, w_temp);

  // --- C. Print Full ASCII Dashboard to Serial Monitor ---
  Serial.println("\n===============================================================================");
  Serial.println("| DR.ONE COMMAND CENTER: LIVE TELEMETRY (ALL PARAMETERS)                      |");
  Serial.println("===============================================================================");
  Serial.println("| PARAMETER              | VALUE      | UNIT     | DIAGNOSTIC STATUS          |");
  Serial.println("|------------------------|------------|----------|----------------------------|");
  
  Serial.printf ("| %-22s | %-10.2f | %-8s | %-26s |\n", "Air Temperature", a_temp, "C", getTempStatus(a_temp).c_str());
  Serial.printf ("| %-22s | %-10.2f | %-8s | %-26s |\n", "Relative Humidity", a_hum, "%", getHumStatus(a_hum).c_str());
  Serial.printf ("| %-22s | %-10.2f | %-8s | %-26s |\n", "Barometric Pressure", a_pres, "hPa", getPressureStatus(a_pres).c_str());
  Serial.printf ("| %-22s | %-10.1f | %-8s | %-26s |\n", "Air Quality Index", iaq_est, "AQI", getAirQualityStatus(iaq_est).c_str());
  Serial.printf ("| %-22s | %-10.2f | %-8s | %-26s |\n", "Gas Resist (VOCs)", a_gas, "KOhms", getGasStatus(a_gas).c_str());
  Serial.printf ("| %-22s | %-10.2f | %-8s | %-26s |\n", "Est. Altitude", alt, "Meters", getAltitudeStatus(alt).c_str());
  Serial.printf ("| %-22s | %-10.2f | %-8s | %-26s |\n", "Dew Point", dew_point, "C", getDewPointStatus(dew_point).c_str());
  Serial.printf ("| %-22s | %-10.2f | %-8s | %-26s |\n", "Heat Index", heat_idx, "C", getHeatIndexStatus(heat_idx).c_str());
  
  Serial.println("|------------------------|------------|----------|----------------------------|");
  
  Serial.printf ("| %-22s | %-10.2f | %-8s | %-26s |\n", "Water Temperature", w_temp, "C", getTempStatus(w_temp).c_str());
  Serial.printf ("| %-22s | %-10.2f | %-8s | %-26s |\n", "pH Level", comp_pH, "pH", getPhStatus(comp_pH).c_str());
  Serial.printf ("| %-22s | %-10.1f | %-8s | %-26s |\n", "Total Diss. Solids", tds_ppm, "ppm", getTdsStatus(tds_ppm).c_str());
  Serial.printf ("| %-22s | %-10.1f | %-8s | %-26s |\n", "Elec. Conductivity", ec_us_cm, "uS/cm", getEcStatus(ec_us_cm).c_str());
  Serial.printf ("| %-22s | %-10.3f | %-8s | %-26s |\n", "Salinity Profile", salinity, "ppt", getSalinityStatus(salinity).c_str());
  Serial.printf ("| %-22s | %-10.1f | %-8s | %-26s |\n", "Turbidity", turb_ntu, "NTU", getTurbidityStatus(tss).c_str());
  Serial.printf ("| %-22s | %-10.2f | %-8s | %-26s |\n", "Total Susp. Solids", tss, "mg/L", getTurbidityStatus(tss).c_str());
  
  Serial.println("|------------------------|------------|----------|----------------------------|");
  
  // NEW real-time analytical evaluation row insertion
  Serial.printf ("| %-22s | %-10.2f | %-8s | %-26s |\n", "Non-Ionic Particulates", nip_density, "mg/L", getNIPStatus(nip_density).c_str());
  
  Serial.println("|------------------------|------------|----------|----------------------------|");

  Serial.printf ("| %-22s | %-10.3f | %-8s | %-26s |\n", "Raw pH Voltage", v_ph, "Volts", getVoltageDiagnostic(v_ph).c_str());
  Serial.printf ("| %-22s | %-10.3f | %-8s | %-26s |\n", "Raw TDS Voltage", v_tds, "Volts", getVoltageDiagnostic(v_tds).c_str());
  Serial.printf ("| %-22s | %-10.3f | %-8s | %-26s |\n", "Raw Turb Voltage", v_turb, "Volts", getVoltageDiagnostic(v_turb).c_str());

  Serial.println("===============================================================================");

  // --- D. Package the LoRa Payload & Transmit ---
  // Appended nip_density cleanly to the CSV payload data array string
  String payload = String(a_temp)    + "," + 
                   String(a_hum)     + "," + 
                   String(a_pres)    + "," + 
                   String(a_gas)     + "," + 
                   String(w_temp)    + "," + 
                   String(v_ph)      + "," + 
                   String(v_tds)     + "," + 
                   String(v_turb)    + "," +
                   String(nip_density);
                   
  Serial.print("\n>> Transmitting Compressed Telemetry via LoRa: ");
  Serial.println(payload);
  
  Serial.print(">> Attempting to fire radio waves into the air... ");
  
  LoRa.beginPacket();
  LoRa.print(payload);
  int transmissionState = LoRa.endPacket(); 
  
  if (transmissionState == 1) {
    Serial.println("SUCCESS! Data successfully pushed out of the antenna.");
  } else {
    Serial.println("FAILED! The LoRa chip refused to transmit.");
  }
  
  delay(10000); 
}