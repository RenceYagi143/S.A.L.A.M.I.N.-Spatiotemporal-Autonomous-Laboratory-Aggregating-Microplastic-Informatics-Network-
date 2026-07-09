function doGet(e) {
  var sheet = SpreadsheetApp.getActiveSpreadsheet().getActiveSheet();
  
  try {
    var data = JSON.parse(e.parameter.payload);
    
    // 1. Force Philippine Time
    SpreadsheetApp.getActiveSpreadsheet().setSpreadsheetTimeZone("Asia/Manila");
    var timestamp = Utilities.formatDate(new Date(), "Asia/Manila", "MMMM dd, yyyy - hh:mm:ss a");
    
    // =========================================================
    // ROUTE A: ESP32 SENSOR UPLINK (Columns A to J)
    // =========================================================
    if (data.hardware) {
      var blockData = [
        ["Time and Date:", timestamp, "", ""],
        ["PARAMETER", "VALUE", "UNIT", "DIAGNOSTIC STATUS"],
        ["Air Temperature", data.hardware.air_temp_c, "C", data.analysis.a_temp_stat],
        ["Relative Humidity", data.hardware.air_hum, "%", data.analysis.a_hum_stat],
        ["Barometric Pressure", data.hardware.air_pres, "hPa", data.analysis.a_pres_stat],
        ["Air Quality Index", data.derived.iaq, "AQI", data.analysis.a_aqi_stat],
        ["Gas Resist (VOCs)", data.hardware.air_gas, "KOhms", data.analysis.a_gas_stat],
        ["Est. Altitude", data.derived.altitude_m, "Meters", data.analysis.a_alt_stat],
        ["Dew Point", data.derived.dew_point, "C", data.analysis.a_dew_stat],
        ["Heat Index", data.derived.heat_idx, "C", data.analysis.a_heat_stat],
        ["Water Temperature", data.hardware.water_temp_c, "C", data.analysis.w_temp_stat],
        ["pH Level", data.derived.compensated_ph, "pH", data.analysis.w_ph_stat],
        ["Total Diss. Solids", data.derived.tds_ppm, "ppm", data.analysis.w_tds_stat],
        ["Elec. Conductivity", data.derived.ec_us, "uS/cm", data.analysis.w_ec_stat],
        ["Salinity Profile", data.derived.salinity_ppt, "ppt", data.analysis.w_sal_stat],
        ["Turbidity", data.derived.turb_ntu, "NTU", data.analysis.w_turb_stat],
        ["Total Susp. Solids", data.derived.tss, "mg/L", data.analysis.w_turb_stat], 
        ["Non-Ionic Particulates", data.derived.nip_density, "mg/L", data.analysis.w_nip_stat], 
        ["Raw pH Voltage", data.hardware.raw_ph_v, "Volts", data.analysis.v_ph_stat],
        ["Raw TDS Voltage", data.hardware.raw_tds_v, "Volts", data.analysis.v_tds_stat],
        ["Raw Turb Voltage", data.hardware.raw_turb_v, "Volts", data.analysis.v_turb_stat]
      ];
      
      var aColumnData = sheet.getRange("A:A").getValues();
      var lastRowA = 0;
      for (var i = aColumnData.length - 1; i >= 0; i--) {
        if (aColumnData[i][0] !== "") { lastRowA = i + 1; break; }
      }
      var startRow = (lastRowA === 0) ? 1 : lastRowA + 3;
      
      sheet.getRange(startRow, 1, blockData.length, blockData[0].length).setValues(blockData);
      sheet.getRange(startRow, 1, 1, 2).setFontWeight("bold");
      sheet.getRange(startRow + 1, 1, 1, 4).setFontWeight("bold");
      
      generateAveragesDashboard(sheet);
      return ContentService.createTextOutput("SUCCESS: Sensor Data Logged & Updated");
    }
    
    // =========================================================
    // ROUTE B: AI CAMERA UPLINK (Columns M to P)
    // =========================================================
    else if (data.source === "camera") {
      var camBlockData = [
        ["Time and Date:", timestamp, "", ""],
        ["PARAMETER", "VALUE", "UNIT", "DIAGNOSTIC STATUS"],
        ["Latitude", data.lat, "deg", "GPS Lock"],
        ["Longitude", data.lon, "deg", "GPS Lock"],
        ["Frame Plastics", data.frame_plastics, "items", "AI Live Count"],
        ["Unique Plastics", data.unique_plastics, "items", "AI Tracked"],
        ["Est. Trash Area", data.trash_area, "sqm", "Optical Math"],
        ["Water Optics", data.water_color, "Optical", "HSV Analysis"]
      ];
      
      var mColumnData = sheet.getRange("M:M").getValues();
      var lastRowM = 0;
      for (var i = mColumnData.length - 1; i >= 0; i--) {
        if (mColumnData[i][0] !== "") { lastRowM = i + 1; break; }
      }
      var startRowM = (lastRowM === 0) ? 1 : lastRowM + 3;
      
      // Paste to Column M (Column 13)
      sheet.getRange(startRowM, 13, camBlockData.length, camBlockData[0].length).setValues(camBlockData);
      sheet.getRange(startRowM, 13, 1, 2).setFontWeight("bold");
      sheet.getRange(startRowM + 1, 13, 1, 4).setFontWeight("bold");
      
      generateCameraAveragesDashboard(sheet);
      return ContentService.createTextOutput("SUCCESS: Camera Data Logged & Updated");
    }

  } catch(error) {
    return ContentService.createTextOutput("ERROR: " + error.toString());
  }
}

// =================================================================
// THE ANALYTICS ENGINES
// =================================================================

function getMode(arr) {
  var counts = {}; var maxCount = 0; var mode = arr[0];
  for (var i = 0; i < arr.length; i++) {
    var el = arr[i]; counts[el] = (counts[el] || 0) + 1;
    if (counts[el] > maxCount) { maxCount = counts[el]; mode = el; }
  }
  return mode;
}

function getOrdinal(n) {
  var s = ["th", "st", "nd", "rd"], v = n % 100;
  return n + (s[(v - 20) % 10] || s[v] || s[0]);
}

// --- ESP32 SENSOR DASHBOARD (Columns G-J) ---
function generateAveragesDashboard(sheet) {
  var aColumnData = sheet.getRange("A:A").getValues();
  var maxRowA = 0;
  for (var i = aColumnData.length - 1; i >= 0; i--) {
    if (aColumnData[i][0] !== "") { maxRowA = i + 1; break; }
  }
  if (maxRowA === 0) return;
  var rawData = sheet.getRange(1, 1, maxRowA, 4).getValues();
  
  var readings = []; var paramNames = []; var paramUnits = [];
  
  for (var i = 0; i < rawData.length; i++) {
    if (rawData[i][0] === "Time and Date:") {
      var timeStr = rawData[i][1];
      var ts = new Date(timeStr.replace(" - ", " ")).getTime();
      if (isNaN(ts)) continue;
      
      var reading = { timeStr: timeStr, timestamp: ts, values: [], statuses: [] };
      i++; 
      for (var j = 0; j < 19; j++) {
        i++;
        if (i >= rawData.length) break;
        if (readings.length === 0) { paramNames.push(rawData[i][0]); paramUnits.push(rawData[i][2]); }
        reading.values.push(parseFloat(rawData[i][1]) || 0);
        reading.statuses.push(rawData[i][3]);
      }
      readings.push(reading);
    }
  }
  if (readings.length === 0) return;

  var overallSums = new Array(19).fill(0);
  var overallStatuses = Array.from({length: 19}, () => []);
  var buckets = {}; 
  var t0 = readings[0].timestamp; 

  for (var k = 0; k < readings.length; k++) {
    var r = readings[k];
    var bucketId = Math.floor((r.timestamp - t0) / 300000); 
    
    if (!buckets[bucketId]) {
      buckets[bucketId] = { startStr: r.timeStr, endStr: r.timeStr, count: 0, sums: new Array(19).fill(0), statuses: Array.from({length: 19}, () => []) };
    }
    buckets[bucketId].endStr = r.timeStr;
    buckets[bucketId].count++;
    
    for (var j = 0; j < 19; j++) {
      overallSums[j] += r.values[j];
      overallStatuses[j].push(r.statuses[j]);
      buckets[bucketId].sums[j] += r.values[j];
      buckets[bucketId].statuses[j].push(r.statuses[j]);
    }
  }

  var outData = [["", "", "", ""]]; 
  var boldRows = [];

  function addBlock(title, startT, endT, avgs, modes) {
    var startIdx = outData.length + 1; 
    outData.push([title, "", "", ""]);
    outData.push(["Time and Date:", startT + " to " + endT, "", ""]);
    outData.push(["PARAMETER", "VALUE", "UNIT", "DIAGNOSTIC STATUS"]);
    boldRows.push(startIdx, startIdx + 1, startIdx + 2); 
    for (var i = 0; i < 19; i++) { outData.push([paramNames[i], avgs[i].toFixed(2), paramUnits[i], modes[i]]); }
    outData.push(["", "", "", ""]); outData.push(["", "", "", ""]); 
  }

  var overallAvgs = overallSums.map(s => s / readings.length);
  var overallModeStatuses = overallStatuses.map(getMode);
  addBlock("OVERALL INTERVAL AVERAGE", readings[0].timeStr, readings[readings.length - 1].timeStr, overallAvgs, overallModeStatuses);

  var maxBucket = Math.max(...Object.keys(buckets).map(Number));
  for (var b = 0; b <= maxBucket; b++) {
    if (buckets[b]) {
      var title = getOrdinal(b + 1) + " FIVE-MINUTE INTERVAL AVERAGE";
      var bAvgs = buckets[b].sums.map(s => s / buckets[b].count);
      var bModes = buckets[b].statuses.map(getMode);
      addBlock(title, buckets[b].startStr, buckets[b].endStr, bAvgs, bModes);
    }
  }

  sheet.getRange("G:J").clear();
  sheet.getRange(1, 7, outData.length, 4).setValues(outData);
  var boldRangeList = boldRows.map(row => "G" + row + ":J" + row);
  if(boldRangeList.length > 0) sheet.getRangeList(boldRangeList).setFontWeight("bold");
}

// --- AI CAMERA DASHBOARD (Columns S-V, leaving Q & R blank) ---
function generateCameraAveragesDashboard(sheet) {
  var mColumnData = sheet.getRange("M:M").getValues();
  var maxRowM = 0;
  for (var i = mColumnData.length - 1; i >= 0; i--) {
    if (mColumnData[i][0] !== "") { maxRowM = i + 1; break; }
  }
  if (maxRowM === 0) return;
  
  // Read Raw Data starting at Col M (13)
  var rawData = sheet.getRange(1, 13, maxRowM, 4).getValues();
  
  var readings = []; var paramNames = []; var paramUnits = [];
  
  for (var i = 0; i < rawData.length; i++) {
    if (rawData[i][0] === "Time and Date:") {
      var timeStr = rawData[i][1];
      var ts = new Date(timeStr.replace(" - ", " ")).getTime();
      if (isNaN(ts)) continue;
      
      var reading = { timeStr: timeStr, timestamp: ts, values: [], statuses: [] };
      i++; 
      for (var j = 0; j < 6; j++) { // 6 Camera Parameters
        i++;
        if (i >= rawData.length) break;
        if (readings.length === 0) { paramNames.push(rawData[i][0]); paramUnits.push(rawData[i][2]); }
        // Water Optics text handled safely
        var val = parseFloat(rawData[i][1]);
        reading.values.push(isNaN(val) ? 0 : val);
        reading.statuses.push(rawData[i][3]);
      }
      readings.push(reading);
    }
  }
  if (readings.length === 0) return;

  var overallSums = new Array(6).fill(0);
  var overallStatuses = Array.from({length: 6}, () => []);
  var buckets = {}; 
  var t0 = readings[0].timestamp; 

  for (var k = 0; k < readings.length; k++) {
    var r = readings[k];
    var bucketId = Math.floor((r.timestamp - t0) / 300000); 
    
    if (!buckets[bucketId]) {
      buckets[bucketId] = { startStr: r.timeStr, endStr: r.timeStr, count: 0, sums: new Array(6).fill(0), statuses: Array.from({length: 6}, () => []) };
    }
    buckets[bucketId].endStr = r.timeStr;
    buckets[bucketId].count++;
    
    for (var j = 0; j < 6; j++) {
      overallSums[j] += r.values[j];
      overallStatuses[j].push(r.statuses[j] || r.values[j]); 
      buckets[bucketId].sums[j] += r.values[j];
      buckets[bucketId].statuses[j].push(r.statuses[j] || r.values[j]);
    }
  }

  var outData = [["", "", "", ""]]; 
  var boldRows = [];

  function addBlock(title, startT, endT, avgs, modes) {
    var startIdx = outData.length + 1; 
    outData.push([title, "", "", ""]);
    outData.push(["Time and Date:", startT + " to " + endT, "", ""]);
    outData.push(["PARAMETER", "AVERAGE/LAST", "UNIT", "DIAGNOSTIC STATUS"]);
    boldRows.push(startIdx, startIdx + 1, startIdx + 2); 
    for (var i = 0; i < 6; i++) { 
        var dispVal = paramNames[i] === "Water Optics" ? modes[i] : avgs[i].toFixed(4);
        outData.push([paramNames[i], dispVal, paramUnits[i], modes[i]]); 
    }
    outData.push(["", "", "", ""]); outData.push(["", "", "", ""]); 
  }

  var overallAvgs = overallSums.map(s => s / readings.length);
  var overallModeStatuses = overallStatuses.map(getMode);
  addBlock("OVERALL INTERVAL AVERAGE", readings[0].timeStr, readings[readings.length - 1].timeStr, overallAvgs, overallModeStatuses);

  var maxBucket = Math.max(...Object.keys(buckets).map(Number));
  for (var b = 0; b <= maxBucket; b++) {
    if (buckets[b]) {
      var title = getOrdinal(b + 1) + " FIVE-MINUTE INTERVAL AVERAGE";
      var bAvgs = buckets[b].sums.map(s => s / buckets[b].count);
      var bModes = buckets[b].statuses.map(getMode);
      addBlock(title, buckets[b].startStr, buckets[b].endStr, bAvgs, bModes);
    }
  }

  // Write to Column S (Column 19) through V (Column 22), keeping Q & R completely empty
  sheet.getRange("S:V").clear();
  sheet.getRange(1, 19, outData.length, 4).setValues(outData);
  var boldRangeList = boldRows.map(row => "S" + row + ":V" + row);
  if(boldRangeList.length > 0) sheet.getRangeList(boldRangeList).setFontWeight("bold");
}