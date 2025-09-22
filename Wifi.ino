/*
  ESP32 Room Device Scanner - WiFi Cliente + Online API
  - Se conecta directamente a tu red WiFi existente
  - Identifica fabricantes via API online (macvendors.com)  
  - Servidor web accesible desde cualquier dispositivo en tu red
  - IP asignada automáticamente por tu router
*/

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <ESPmDNS.h>  // Para acceso fácil via nombre

// --- Configuración WiFi ---
const char* wifiSSID = "AQUILES";        // ⚠️ CAMBIAR POR TU RED
const char* wifiPASS = "azumisalas";     // ⚠️ CAMBIAR POR TU PASSWORD
const char* deviceName = "esp32scanner";       // Nombre para acceso fácil

// --- Configuración BLE ---
const int BLE_SCAN_TIME = 5;

// --- Configuración API ---
const String API_BASE_URL = "https://api.macvendors.com/";
const int API_TIMEOUT = 5000;
const int API_DELAY = 1100;

// --- Variables globales ---
WebServer server(80);
String lastScanHTML = "";
bool scanInProgress = false;
String espIP = "";
int apiCallsCount = 0;

// --- Cache de consultas ---
struct VendorCache {
  String oui;
  String vendor;
  String deviceType;
  unsigned long timestamp;
};

const int CACHE_SIZE = 50;
VendorCache vendorCache[CACHE_SIZE];
int cacheIndex = 0;

// --- Conectividad WiFi ---
bool connectToWiFi() {
  Serial.println("\n=== Conectando a WiFi ===");
  Serial.printf("Red: %s\n", wifiSSID);
  
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(deviceName);
  WiFi.begin(wifiSSID, wifiPASS);
  
  Serial.print("Conectando");
  unsigned long startTime = millis();
  
  while (WiFi.status() != WL_CONNECTED && (millis() - startTime) < 20000) {
    delay(500);
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    espIP = WiFi.localIP().toString();
    Serial.printf("✅ WiFi conectado!\n");
    Serial.printf("📡 IP del ESP32: %s\n", espIP.c_str());
    Serial.printf("🌐 Acceso web: http://%s\n", espIP.c_str());
    Serial.printf("🔗 Acceso fácil: http://%s.local\n", deviceName);
    
    // Configurar mDNS para acceso fácil
    if (MDNS.begin(deviceName)) {
      MDNS.addService("http", "tcp", 80);
      Serial.printf("✅ mDNS configurado: %s.local\n", deviceName);
    }
    
    return true;
  } else {
    Serial.println();
    Serial.println("❌ Error: No se pudo conectar a WiFi");
    Serial.println("⚠️ Verifica las credenciales en el código");
    return false;
  }
}

// --- Cache management ---
String getCachedVendor(const String& oui, String& deviceType) {
  for (int i = 0; i < CACHE_SIZE; i++) {
    if (vendorCache[i].oui == oui && vendorCache[i].vendor.length() > 0) {
      deviceType = vendorCache[i].deviceType;
      return vendorCache[i].vendor;
    }
  }
  return "";
}

void cacheVendor(const String& oui, const String& vendor, const String& deviceType) {
  vendorCache[cacheIndex].oui = oui;
  vendorCache[cacheIndex].vendor = vendor;
  vendorCache[cacheIndex].deviceType = deviceType;
  vendorCache[cacheIndex].timestamp = millis();
  
  cacheIndex = (cacheIndex + 1) % CACHE_SIZE;
}

// --- Consulta online de fabricante ---
String lookupVendorOnline(const String& mac, String& deviceType) {
  String oui = mac.substring(0, 2) + mac.substring(3, 5) + mac.substring(6, 8);
  oui.toUpperCase();
  
  // Verificar cache
  String cachedVendor = getCachedVendor(oui, deviceType);
  if (cachedVendor.length() > 0) {
    Serial.printf("📦 Cache: %s -> %s\n", oui.c_str(), cachedVendor.c_str());
    return cachedVendor;
  }
  
  // Verificar conectividad
  if (WiFi.status() != WL_CONNECTED) {
    deviceType = "No WiFi";
    return "Disconnected";
  }
  
  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();
  
  String url = API_BASE_URL + oui;
  Serial.printf("🌐 API: %s\n", oui.c_str());
  
  http.begin(client, url);
  http.addHeader("User-Agent", "ESP32Scanner/2.0");
  http.setTimeout(API_TIMEOUT);
  
  int httpCode = http.GET();
  String vendor = "Unknown";
  deviceType = "Unknown";
  
  apiCallsCount++;
  
  if (httpCode == 200) {
    vendor = http.getString();
    vendor.trim();
    
    if (vendor.length() > 50) vendor = vendor.substring(0, 50);
    
    // Inferir tipo de dispositivo
    if (vendor.indexOf("Apple") >= 0) deviceType = "iPhone/iPad/Mac";
    else if (vendor.indexOf("Samsung") >= 0) deviceType = "Galaxy/SmartTV";
    else if (vendor.indexOf("Google") >= 0) deviceType = "Nest/Chromecast";
    else if (vendor.indexOf("Amazon") >= 0) deviceType = "Echo/Fire";
    else if (vendor.indexOf("Sony") >= 0) deviceType = "PlayStation/TV";
    else if (vendor.indexOf("Microsoft") >= 0) deviceType = "Xbox/Surface";
    else if (vendor.indexOf("Nintendo") >= 0) deviceType = "Switch/Wii";
    else if (vendor.indexOf("Xiaomi") >= 0) deviceType = "Mi/Redmi";
    else if (vendor.indexOf("Huawei") >= 0) deviceType = "Smartphone/Router";
    else if (vendor.indexOf("Intel") >= 0) deviceType = "WiFi Card";
    else if (vendor.indexOf("TP-Link") >= 0) deviceType = "Router/AP";
    else if (vendor.indexOf("D-Link") >= 0) deviceType = "Router/AP";
    else if (vendor.indexOf("Netgear") >= 0) deviceType = "Router/AP";
    else if (vendor.indexOf("ASUS") >= 0) deviceType = "Router/Laptop";
    else if (vendor.indexOf("LG") >= 0) deviceType = "SmartTV/Phone";
    else if (vendor.indexOf("Espressif") >= 0) deviceType = "ESP32/ESP8266";
    else if (vendor.indexOf("Raspberry") >= 0) deviceType = "Single Board Computer";
    else if (vendor.indexOf("Broadcom") >= 0) deviceType = "WiFi Chip";
    else deviceType = "Network Device";
    
    Serial.printf("✅ %s (%s)\n", vendor.c_str(), deviceType.c_str());
    
  } else if (httpCode == 404) {
    vendor = "Unknown Vendor";
    Serial.printf("⚠️ OUI no encontrado\n");
  } else if (httpCode == 429) {
    vendor = "Rate Limited";
    deviceType = "Too Many Requests";
    Serial.printf("⚠️ Rate limit (429)\n");
  } else {
    vendor = "API Error";
    deviceType = "HTTP " + String(httpCode);
    Serial.printf("❌ Error HTTP: %d\n", httpCode);
  }
  
  http.end();
  cacheVendor(oui, vendor, deviceType);
  delay(API_DELAY);
  
  return vendor;
}

// --- Escaneo BLE ---
bool doBLEScan(String &htmlOut) {
  if (!BLEDevice::getInitialized()) {
    htmlOut += "<h3>❌ Error: BLE no disponible</h3>";
    return false;
  }

  BLEScan* pBLEScan = BLEDevice::getScan();
  if (!pBLEScan) {
    htmlOut += "<h3>❌ Error: Scanner BLE no disponible</h3>";
    return false;
  }

  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
  
  Serial.println("\n--- Escaneo BLE ---");

  BLEScanResults* results = pBLEScan->start(BLE_SCAN_TIME, false);
  if (!results) {
    htmlOut += "<h3>❌ Error en escaneo BLE</h3>";
    return false;
  }

  int count = results->getCount();
  Serial.printf("BLE: %d dispositivos encontrados\n", count);

  htmlOut += "<h3>📱 Dispositivos BLE (" + String(count) + ")</h3>";
  
  if (count == 0) {
    htmlOut += "<p>No se encontraron dispositivos BLE</p>";
  } else {
    htmlOut += "<div style='overflow-x:auto;'><table border='1' style='width:100%;min-width:800px;'>";
    htmlOut += "<tr style='background-color:#3498db;color:white'><th>#</th><th>MAC Address</th><th>RSSI</th><th>Nombre BLE</th><th>Fabricante</th><th>Tipo Dispositivo</th></tr>";
    
    for (int i = 0; i < count; ++i) {
      BLEAdvertisedDevice d = results->getDevice(i);
      String mac = d.getAddress().toString().c_str();
      mac.toUpperCase();
      int rssi = d.getRSSI();
      String name = d.haveName() ? d.getName().c_str() : String("sin nombre");
      
      String deviceType;
      String vendor = lookupVendorOnline(mac, deviceType);

      Serial.printf(" BLE %02d) %s | %d dBm | %s | %s\n", i + 1, mac.c_str(), rssi, name.c_str(), vendor.c_str());

      String rowColor = (i % 2 == 0) ? "style='background-color:#f8f9fa'" : "";
      htmlOut += "<tr " + rowColor + "><td>" + String(i + 1) + "</td>";
      htmlOut += "<td><code style='font-size:12px'>" + mac + "</code></td>";
      htmlOut += "<td>" + String(rssi) + " dBm</td>";
      htmlOut += "<td>" + name + "</td>";
      htmlOut += "<td><strong>" + vendor + "</strong></td>";
      htmlOut += "<td><em>" + deviceType + "</em></td></tr>";
    }
    htmlOut += "</table></div>";
  }

  pBLEScan->clearResults();
  return true;
}

// --- Escaneo WiFi ---
bool doWiFiScan(String &htmlOut) {
  Serial.println("\n--- Escaneo WiFi ---");

  int n = WiFi.scanNetworks();
  Serial.printf("WiFi: %d redes encontradas\n", n);

  htmlOut += "<h3>📶 Redes WiFi (" + String(n) + ")</h3>";

  if (n == 0) {
    htmlOut += "<p>No se encontraron redes WiFi</p>";
  } else {
    htmlOut += "<div style='overflow-x:auto;'><table border='1' style='width:100%;min-width:900px;'>";
    htmlOut += "<tr style='background-color:#3498db;color:white'><th>#</th><th>SSID</th><th>BSSID (MAC)</th><th>RSSI</th><th>Canal</th><th>Seguridad</th><th>Fabricante</th><th>Tipo</th></tr>";
    
    for (int i = 0; i < n; ++i) {
      String ssid = WiFi.SSID(i);
      String bssid = WiFi.BSSIDstr(i);
      int32_t rssi = WiFi.RSSI(i);
      int32_t channel = WiFi.channel(i);
      String enc;
      
      switch (WiFi.encryptionType(i)) {
        case WIFI_AUTH_OPEN: enc = "OPEN"; break;
        case WIFI_AUTH_WEP: enc = "WEP"; break;
        case WIFI_AUTH_WPA_PSK: enc = "WPA"; break;
        case WIFI_AUTH_WPA2_PSK: enc = "WPA2"; break;
        case WIFI_AUTH_WPA_WPA2_PSK: enc = "WPA/WPA2"; break;
        case WIFI_AUTH_WPA2_ENTERPRISE: enc = "WPA2-ENT"; break;
        default: enc = "UNKNOWN"; break;
      }

      String deviceType;
      String vendor = lookupVendorOnline(bssid, deviceType);

      Serial.printf(" WIFI %02d) %s | %s | %d dBm | Ch%d | %s\n", i + 1, ssid.c_str(), bssid.c_str(), rssi, channel, vendor.c_str());

      String rowColor = (i % 2 == 0) ? "style='background-color:#f8f9fa'" : "";
      htmlOut += "<tr " + rowColor + "><td>" + String(i + 1) + "</td>";
      htmlOut += "<td><strong>" + ssid + "</strong></td>";
      htmlOut += "<td><code style='font-size:12px'>" + bssid + "</code></td>";
      htmlOut += "<td>" + String(rssi) + " dBm</td>";
      htmlOut += "<td>" + String(channel) + "</td>";
      htmlOut += "<td>" + enc + "</td>";
      htmlOut += "<td><strong>" + vendor + "</strong></td>";
      htmlOut += "<td><em>" + deviceType + "</em></td></tr>";
    }
    htmlOut += "</table></div>";
  }

  WiFi.scanDelete();
  return true;
}

// --- Escaneo completo ---
void performFullScan() {
  if (scanInProgress) {
    Serial.println("⚠️ Escaneo en progreso...");
    return;
  }
  
  scanInProgress = true;
  lastScanHTML = "";
  apiCallsCount = 0;
  
  Serial.println("\n========== ESCANEO INICIADO ==========");
  unsigned long startTime = millis();
  
  doBLEScan(lastScanHTML);
  delay(500);
  doWiFiScan(lastScanHTML);
  
  unsigned long scanDuration = (millis() - startTime) / 1000;
  
  // Estadísticas
  lastScanHTML += "<hr><div style='background:#e8f5e8;padding:15px;border-radius:8px;margin:20px 0;'>";
  lastScanHTML += "<h3 style='margin-top:0;color:#27ae60;'>📊 Estadísticas del Escaneo</h3>";
  lastScanHTML += "<div style='display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:15px;'>";
  lastScanHTML += "<div><strong>⏱️ Duración:</strong><br>" + String(scanDuration) + " segundos</div>";
  lastScanHTML += "<div><strong>🌐 Estado WiFi:</strong><br>" + String(WiFi.status() == WL_CONNECTED ? "✅ Conectado" : "❌ Desconectado") + "</div>";
  lastScanHTML += "<div><strong>📞 Consultas API:</strong><br>" + String(apiCallsCount) + " realizadas</div>";
  lastScanHTML += "<div><strong>📦 Cache:</strong><br>" + String(min(cacheIndex, CACHE_SIZE)) + "/" + String(CACHE_SIZE) + " entradas</div>";
  lastScanHTML += "<div><strong>🔋 Uptime:</strong><br>" + String(millis() / 1000) + " segundos</div>";
  lastScanHTML += "<div><strong>📡 IP ESP32:</strong><br>" + espIP + "</div>";
  lastScanHTML += "</div></div>";
  
  scanInProgress = false;
  Serial.printf("========== ESCANEO COMPLETADO (%lu seg) ==========\n", scanDuration);
}

// --- Páginas web ---
void handleRoot() {
  String page = "<!DOCTYPE html><html><head>";
  page += "<meta charset='UTF-8'>";
  page += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  page += "<title>ESP32 Device Scanner</title>";
  page += "<style>";
  page += "* { box-sizing: border-box; }";
  page += "body { font-family: 'Segoe UI', Arial, sans-serif; margin: 0; padding: 20px; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); min-height: 100vh; }";
  page += ".container { max-width: 1400px; margin: 0 auto; background: white; border-radius: 15px; box-shadow: 0 10px 30px rgba(0,0,0,0.2); overflow: hidden; }";
  page += ".header { background: linear-gradient(135deg, #2c3e50 0%, #3498db 100%); color: white; padding: 30px; text-align: center; }";
  page += ".header h1 { margin: 0; font-size: 2.5em; font-weight: 300; }";
  page += ".header p { margin: 10px 0 0 0; opacity: 0.9; }";
  page += ".content { padding: 30px; }";
  page += "h3 { color: #2c3e50; border-left: 4px solid #3498db; padding-left: 15px; margin-top: 40px; }";
  page += "table { width: 100%; border-collapse: collapse; margin: 20px 0; box-shadow: 0 2px 10px rgba(0,0,0,0.1); border-radius: 8px; overflow: hidden; }";
  page += "th { background: #3498db; color: white; padding: 15px 10px; text-align: left; font-weight: 600; }";
  page += "td { padding: 12px 10px; border-bottom: 1px solid #eee; }";
  page += "tr:hover { background-color: #f8f9fa; }";
  page += "code { background: #f1f2f6; padding: 4px 8px; border-radius: 4px; font-family: 'Courier New', monospace; font-size: 0.9em; }";
  page += ".scan-btn { background: linear-gradient(135deg, #27ae60, #2ecc71); color: white; padding: 15px 30px; border: none; border-radius: 8px; font-size: 18px; cursor: pointer; transition: all 0.3s; font-weight: 600; }";
  page += ".scan-btn:hover:not(:disabled) { transform: translateY(-2px); box-shadow: 0 5px 15px rgba(39,174,96,0.3); }";
  page += ".scan-btn:disabled { background: #bdc3c7; cursor: not-allowed; transform: none; }";
  page += ".status { text-align: center; margin: 30px 0; }";
  page += ".footer { text-align: center; margin-top: 40px; padding: 20px; background: #f8f9fa; color: #7f8c8d; border-top: 1px solid #eee; }";
  page += ".loading { display: inline-block; width: 20px; height: 20px; border: 3px solid #f3f3f3; border-radius: 50%; border-top-color: #3498db; animation: spin 1s ease-in-out infinite; }";
  page += "@keyframes spin { to { transform: rotate(360deg); } }";
  page += "@media (max-width: 768px) { .header h1 { font-size: 2em; } th, td { padding: 8px 5px; font-size: 0.9em; } }";
  page += "</style>";
  page += "</head><body>";
  
  page += "<div class='container'>";
  page += "<div class='header'>";
  page += "<h1>🔍 ESP32 Device Scanner</h1>";
  page += "<p>Detección inteligente de dispositivos con identificación online de fabricantes</p>";
  page += "</div>";
  
  page += "<div class='content'>";
  
  if (scanInProgress) {
    page += "<div class='status'>";
    page += "<div class='loading'></div>";
    page += "<p><strong>🔄 Escaneo en progreso...</strong><br>";
    page += "<small>Consultando APIs online para identificar fabricantes...</small></p>";
    page += "</div>";
    page += "<meta http-equiv='refresh' content='5'>";
  }
  
  if (lastScanHTML.length() > 0) {
    page += lastScanHTML;
  } else {
    page += "<div class='status'>";
    page += "<p style='font-size:1.2em;color:#7f8c8d;'>Presiona el botón para comenzar a escanear dispositivos en tu red</p>";
    page += "</div>";
  }
  
  page += "<div class='status'>";
  page += "<form action='/rescan' method='get'>";
  page += "<button class='scan-btn' type='submit' " + String(scanInProgress ? "disabled" : "") + ">";
  page += scanInProgress ? "⏳ Escaneando..." : "🔍 Escanear Dispositivos";
  page += "</button></form>";
  page += "</div>";
  
  page += "</div>";
  
  page += "<div class='footer'>";
  page += "<p><strong>📡 ESP32 IP:</strong> <code>" + espIP + "</code> | ";
  page += "<strong>🌐 Acceso fácil:</strong> <code>" + String(deviceName) + ".local</code><br>";
  page += "<strong>🔧 API:</strong> macvendors.com | <strong>📦 Cache:</strong> " + String(min(cacheIndex, CACHE_SIZE)) + "/" + String(CACHE_SIZE) + " entradas</p>";
  page += "</div>";
  
  page += "</div></body></html>";
  
  server.send(200, "text/html", page);
}

void handleRescan() {
  performFullScan();
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

void handleNotFound() {
  server.send(404, "text/html", "<h1>404 - Página no encontrada</h1><a href='/'>Volver al inicio</a>");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n==================================================");
  Serial.println("🚀 ESP32 DEVICE SCANNER v2.0");
  Serial.println("📡 Modo: WiFi Cliente + API Online");
  Serial.println("==================================================");

  // Inicializar cache
  for (int i = 0; i < CACHE_SIZE; i++) {
    vendorCache[i].oui = "";
    vendorCache[i].vendor = "";
    vendorCache[i].deviceType = "";
    vendorCache[i].timestamp = 0;
  }

  // Conectar a WiFi
  if (!connectToWiFi()) {
    Serial.println("❌ FALLO CRÍTICO: Sin conexión WiFi");
    Serial.println("⚠️ Verifica las credenciales y reinicia el ESP32");
    while (true) {
      delay(1000);
    }
  }

  // Inicializar BLE
  Serial.println("\n📱 Inicializando Bluetooth...");
  BLEDevice::init("");
  Serial.println("✅ BLE listo");

  // Configurar servidor web
  server.on("/", handleRoot);
  server.on("/rescan", handleRescan);
  server.onNotFound(handleNotFound);
  server.begin();
  
  Serial.println("\n🌐 Servidor web iniciado");
  Serial.println("📋 INFORMACIÓN DE ACCESO:");
  Serial.println("   • IP directa: http://" + espIP);
  Serial.println("   • Nombre fácil: http://" + String(deviceName) + ".local");
  
  // Escaneo inicial
  Serial.println("\n🔍 Realizando escaneo inicial...");
  performFullScan();
  
  Serial.println("\n✅ SISTEMA COMPLETAMENTE LISTO");
  Serial.println("🎯 Accede desde cualquier dispositivo en tu red WiFi");
}

void loop() {
  // Verificar conexión WiFi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️ WiFi desconectado, reintentando...");
    connectToWiFi();
  }
  
  server.handleClient();
  delay(10);
}