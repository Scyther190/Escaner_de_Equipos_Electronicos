/*
  ESP32 Room Device Scanner + WebServer + OUI Vendor Detection
  - Escanea BLE advertisements y redes WiFi visibles.
  - Identifica fabricantes por dirección MAC (OUI database).
  - Muestra resultados en Serial Monitor.
  - Servidor web en modo AP: http://192.168.4.1
*/

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>

// --- Configuración WiFi AP ---
const char* apSSID = "ESP32-Scanner";
const char* apPASS = "12345678";

// --- Configuración BLE ---
const int BLE_SCAN_TIME = 5; // segundos

// --- Variables globales ---
WebServer server(80);
String lastScanHTML = "";
bool scanInProgress = false;

// --- Base de datos OUI (Fabricantes más comunes) ---
struct OUIEntry {
  uint32_t oui;      // primeros 3 bytes como número
  const char* vendor;
  const char* deviceType;
};

// Lista de fabricantes más comunes (puedes expandir esta lista)
const OUIEntry ouiDatabase[] = {
  // Apple
  {0x001EC2, "Apple", "iPhone/iPad/Mac"},
  {0x0050E4, "Apple", "iPhone/iPad/Mac"},
  {0x0017F2, "Apple", "iPhone/iPad/Mac"},
  {0x001124, "Apple", "iPhone/iPad/Mac"},
  {0x002332, "Apple", "iPhone/iPad/Mac"},
  {0x002608, "Apple", "iPhone/iPad/Mac"},
  {0x0025BC, "Apple", "iPhone/iPad/Mac"},
  {0x28E02C, "Apple", "iPhone/iPad/Mac"},
  {0x40A6D9, "Apple", "iPhone/iPad/Mac"},
  {0x58B035, "Apple", "iPhone/iPad/Mac"},
  {0x64B9E8, "Apple", "iPhone/iPad/Mac"},
  {0x68AE20, "Apple", "iPhone/iPad/Mac"},
  {0x70DEE2, "Apple", "iPhone/iPad/Mac"},
  {0x78CA39, "Apple", "iPhone/iPad/Mac"},
  {0x7C6DF8, "Apple", "iPhone/iPad/Mac"},
  {0x8C2937, "Apple", "iPhone/iPad/Mac"},
  {0x90B21F, "Apple", "iPhone/iPad/Mac"},
  {0x98FE94, "Apple", "iPhone/iPad/Mac"},
  {0xA45E60, "Apple", "iPhone/iPad/Mac"},
  {0xBC926B, "Apple", "iPhone/iPad/Mac"},
  {0xD0817A, "Apple", "iPhone/iPad/Mac"},
  {0xF0DBF8, "Apple", "iPhone/iPad/Mac"},
  
  // Samsung
  {0x001D25, "Samsung", "Galaxy/SmartTV"},
  {0x002454, "Samsung", "Galaxy/SmartTV"},
  {0x0025E5, "Samsung", "Galaxy/SmartTV"},
  {0x34BE00, "Samsung", "Galaxy/SmartTV"},
  {0x3C28E0, "Samsung", "Galaxy/SmartTV"},
  {0x5C0A5B, "Samsung", "Galaxy/SmartTV"},
  {0x68A86D, "Samsung", "Galaxy/SmartTV"},
  {0x78D6F0, "Samsung", "Galaxy/SmartTV"},
  {0x8C77B5, "Samsung", "Galaxy/SmartTV"},
  {0xA020A6, "Samsung", "Galaxy/SmartTV"},
  {0xE8508B, "Samsung", "Galaxy/SmartTV"},
  
  // Google/Nest
  {0x6C709F, "Google", "Nest/Chromecast"},
  {0x802AA8, "Google", "Nest/Chromecast"},
  {0xF4F5D8, "Google", "Nest/Chromecast"},
  {0x54FA3E, "Google", "Nest/Chromecast"},
  
  // Amazon
  {0x747548, "Amazon", "Echo/Fire"},
  {0x8C1F64, "Amazon", "Echo/Fire"},
  {0xFC65DE, "Amazon", "Echo/Fire"},
  
  // Xiaomi
  {0x34CE00, "Xiaomi", "Mi/Redmi"},
  {0x64B473, "Xiaomi", "Mi/Redmi"},
  {0x78118C, "Xiaomi", "Mi/Redmi"},
  {0x8CF710, "Xiaomi", "Mi/Redmi"},
  {0x50EC50, "Xiaomi", "Mi/Redmi"},
  
  // Huawei
  {0x002E5D, "Huawei", "Smartphone/Router"},
  {0x00E04C, "Huawei", "Smartphone/Router"},
  {0x1C1D86, "Huawei", "Smartphone/Router"},
  {0x48F17A, "Huawei", "Smartphone/Router"},
  {0x6C4B90, "Huawei", "Smartphone/Router"},
  {0x9C28EF, "Huawei", "Smartphone/Router"},
  
  // Sony
  {0x001A80, "Sony", "PlayStation/TV"},
  {0x002196, "Sony", "PlayStation/TV"},
  {0x0C1420, "Sony", "PlayStation/TV"},
  {0x7C1E52, "Sony", "PlayStation/TV"},
  
  // Nintendo
  {0x001656, "Nintendo", "Switch/Wii"},
  {0x0009BF, "Nintendo", "Switch/Wii"},
  {0x001F32, "Nintendo", "Switch/Wii"},
  {0x002659, "Nintendo", "Switch/Wii"},
  {0x0025A0, "Nintendo", "Switch/Wii"},
  
  // Intel
  {0x001E67, "Intel", "WiFi Card"},
  {0x0024D7, "Intel", "WiFi Card"},
  {0x7C7A91, "Intel", "WiFi Card"},
  
  // TP-Link
  {0x001A92, "TP-Link", "Router/AP"},
  {0x00272D, "TP-Link", "Router/AP"},
  {0x14CF92, "TP-Link", "Router/AP"},
  {0x50C7BF, "TP-Link", "Router/AP"},
  
  // D-Link
  {0x001CF0, "D-Link", "Router/AP"},
  {0x001E58, "D-Link", "Router/AP"},
  {0x14D64D, "D-Link", "Router/AP"},
  
  // Netgear
  {0x001B2F, "Netgear", "Router/AP"},
  {0x00146C, "Netgear", "Router/AP"},
  {0x2C30F3, "Netgear", "Router/AP"},
  
  // Microsoft
  {0x001DD8, "Microsoft", "Xbox/Surface"},
  {0x7CD1C3, "Microsoft", "Xbox/Surface"},
  
  // LG
  {0x001C62, "LG", "SmartTV/Phone"},
  {0x0025E4, "LG", "SmartTV/Phone"},
  {0x64B310, "LG", "SmartTV/Phone"},
  
  // Espressif (ESP32/ESP8266)
  {0x240AC4, "Espressif", "ESP32/ESP8266"},
  {0x30AEA4, "Espressif", "ESP32/ESP8266"},
  {0x807D3A, "Espressif", "ESP32/ESP8266"},
  {0x84CCA8, "Espressif", "ESP32/ESP8266"},
  {0xCC50E3, "Espressif", "ESP32/ESP8266"},
  
  // Raspberry Pi
  {0xB827EB, "Raspberry Pi", "Single Board Computer"},
  {0xDCA632, "Raspberry Pi", "Single Board Computer"},
  
  // ASUS
  {0x001E8C, "ASUS", "Router/Laptop"},
  {0x0018F3, "ASUS", "Router/Laptop"},
  {0x2C56DC, "ASUS", "Router/Laptop"},
  
  // Broadcom
  {0x001018, "Broadcom", "WiFi Chip"},
  {0x000AF7, "Broadcom", "WiFi Chip"}
};

const int OUI_DATABASE_SIZE = sizeof(ouiDatabase) / sizeof(OUIEntry);

// --- Función para extraer OUI de MAC ---
uint32_t extractOUI(const String& mac) {
  // Convertir los primeros 6 caracteres hex a número
  String ouiStr = mac.substring(0, 2) + mac.substring(3, 5) + mac.substring(6, 8);
  return strtoul(ouiStr.c_str(), NULL, 16);
}

// --- Buscar fabricante por OUI ---
String lookupVendor(const String& mac, String& deviceType) {
  uint32_t oui = extractOUI(mac);
  
  for (int i = 0; i < OUI_DATABASE_SIZE; i++) {
    if (ouiDatabase[i].oui == oui) {
      deviceType = ouiDatabase[i].deviceType;
      return String(ouiDatabase[i].vendor);
    }
  }
  
  deviceType = "Unknown";
  return "Unknown";
}

// --- Escaneo BLE ---
bool doBLEScan(String &htmlOut) {
  if (!BLEDevice::getInitialized()) {
    Serial.println("BLE no inicializado");
    htmlOut += "<h3>❌ Error: BLE no disponible</h3>";
    return false;
  }

  BLEScan* pBLEScan = BLEDevice::getScan();
  if (!pBLEScan) {
    Serial.println("Error: no se pudo obtener scanner BLE");
    htmlOut += "<h3>❌ Error: Scanner BLE no disponible</h3>";
    return false;
  }

  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
  
  Serial.println("\n--- Escaneo BLE: iniciando ---");

  BLEScanResults* results = pBLEScan->start(BLE_SCAN_TIME, false); 
  if (!results) {
    Serial.println("Error: resultados BLE nulos");
    htmlOut += "<h3>❌ Error en escaneo BLE</h3>";
    return false;
  }

  int count = results->getCount();
  Serial.printf("BLE: dispositivos encontrados: %d\n", count);

  htmlOut += "<h3>📱 Dispositivos BLE (" + String(count) + ")</h3>";
  
  if (count == 0) {
    htmlOut += "<p>No se encontraron dispositivos BLE</p>";
  } else {
    htmlOut += "<table border='1' style='width:100%'>";
    htmlOut += "<tr style='background-color:#f0f0f0'><th>#</th><th>MAC Address</th><th>RSSI</th><th>Nombre</th><th>Fabricante</th><th>Tipo</th></tr>";
    
    for (int i = 0; i < count; ++i) {
      BLEAdvertisedDevice d = results->getDevice(i);
      String mac = d.getAddress().toString().c_str();
      mac.toUpperCase();
      int rssi = d.getRSSI();
      String name = d.haveName() ? d.getName().c_str() : String("sin nombre");
      
      String deviceType;
      String vendor = lookupVendor(mac, deviceType);

      Serial.printf(" BLE %2d) MAC: %s | RSSI: %d dBm | Name: %s | Vendor: %s | Type: %s\n",
                    i + 1, mac.c_str(), rssi, name.c_str(), vendor.c_str(), deviceType.c_str());

      String rowColor = (i % 2 == 0) ? "style='background-color:#f9f9f9'" : "";
      htmlOut += "<tr " + rowColor + "><td>" + String(i + 1) + "</td><td><code>" + mac + "</code></td>";
      htmlOut += "<td>" + String(rssi) + " dBm</td><td>" + name + "</td>";
      htmlOut += "<td><strong>" + vendor + "</strong></td><td><em>" + deviceType + "</em></td></tr>";
    }
    htmlOut += "</table>";
  }

  pBLEScan->clearResults();
  Serial.println("--- Escaneo BLE: terminado ---");
  return true;
}

// --- Escaneo WiFi ---
bool doWiFiScan(String &htmlOut) {
  Serial.println("\n--- Escaneo WiFi ---");
  
  WiFi.mode(WIFI_AP_STA);
  delay(100);

  int n = WiFi.scanNetworks();
  Serial.printf("WiFi encontrados: %d\n", n);

  htmlOut += "<h3>📶 Redes WiFi (" + String(n) + ")</h3>";

  if (n == 0) {
    htmlOut += "<p>No se encontraron redes WiFi</p>";
  } else {
    htmlOut += "<table border='1' style='width:100%'>";
    htmlOut += "<tr style='background-color:#f0f0f0'><th>#</th><th>SSID</th><th>BSSID (MAC)</th><th>RSSI</th><th>Seguridad</th><th>Fabricante</th><th>Tipo</th></tr>";
    
    for (int i = 0; i < n; ++i) {
      String ssid = WiFi.SSID(i);
      String bssid = WiFi.BSSIDstr(i);
      int32_t rssi = WiFi.RSSI(i);
      String enc = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "OPEN" : "WPA/WPA2";

      String deviceType;
      String vendor = lookupVendor(bssid, deviceType);

      Serial.printf(" WIFI %2d) SSID: %s | BSSID: %s | RSSI: %d dBm | %s | Vendor: %s\n",
                    i + 1, ssid.c_str(), bssid.c_str(), rssi, enc.c_str(), vendor.c_str());

      String rowColor = (i % 2 == 0) ? "style='background-color:#f9f9f9'" : "";
      htmlOut += "<tr " + rowColor + "><td>" + String(i + 1) + "</td><td><strong>" + ssid + "</strong></td>";
      htmlOut += "<td><code>" + bssid + "</code></td><td>" + String(rssi) + " dBm</td>";
      htmlOut += "<td>" + enc + "</td><td><strong>" + vendor + "</strong></td><td><em>" + deviceType + "</em></td></tr>";
    }
    htmlOut += "</table>";
  }

  WiFi.scanDelete();
  WiFi.mode(WIFI_AP);
  delay(100);
  
  Serial.println("--- Escaneo WiFi terminado ---");
  return true;
}

// --- Realizar escaneo completo ---
void performFullScan() {
  if (scanInProgress) {
    Serial.println("Escaneo ya en progreso, ignorando...");
    return;
  }
  
  scanInProgress = true;
  lastScanHTML = "";
  
  Serial.println("=== Iniciando escaneo completo ===");
  
  doBLEScan(lastScanHTML);
  delay(500);
  doWiFiScan(lastScanHTML);
  
  // Estadísticas
  lastScanHTML += "<hr><h3>📊 Estadísticas</h3>";
  lastScanHTML += "<p>✅ <strong>Último escaneo:</strong> " + String(millis() / 1000) + " segundos desde inicio<br>";
  lastScanHTML += "🔍 <strong>Base de datos OUI:</strong> " + String(OUI_DATABASE_SIZE) + " fabricantes conocidos</p>";
  
  scanInProgress = false;
  Serial.println("=== Escaneo completo terminado ===");
}

// --- Páginas web ---
void handleRoot() {
  String page = "<!DOCTYPE html><html><head>";
  page += "<meta charset='UTF-8'>";
  page += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  page += "<title>ESP32 Device Scanner</title>";
  page += "<style>";
  page += "body{font-family:'Segoe UI',Arial,sans-serif;margin:20px;background:#f5f5f5;}";
  page += ".container{max-width:1200px;margin:0 auto;background:white;padding:20px;border-radius:8px;box-shadow:0 2px 10px rgba(0,0,0,0.1);}";
  page += "h1{color:#2c3e50;text-align:center;margin-bottom:30px;}";
  page += "h3{color:#34495e;border-bottom:2px solid #3498db;padding-bottom:5px;}";
  page += "table{border-collapse:collapse;width:100%;margin-bottom:20px;}";
  page += "th,td{padding:8px 12px;text-align:left;border:1px solid #ddd;}";
  page += "th{background-color:#3498db;color:white;}";
  page += "tr:nth-child(even){background-color:#f8f9fa;}";
  page += "code{background:#e9ecef;padding:2px 4px;border-radius:3px;font-family:monospace;}";
  page += ".scan-btn{background:#27ae60;color:white;padding:12px 24px;border:none;border-radius:5px;font-size:16px;cursor:pointer;}";
  page += ".scan-btn:disabled{background:#bdc3c7;cursor:not-allowed;}";
  page += ".scan-btn:hover:not(:disabled){background:#2ecc71;}";
  page += ".status{text-align:center;margin:20px 0;}";
  page += ".footer{text-align:center;margin-top:30px;color:#7f8c8d;font-size:14px;}";
  page += "</style>";
  page += "</head><body>";
  page += "<div class='container'>";
  page += "<h1>🔍 ESP32 Device Scanner</h1>";
  
  if (scanInProgress) {
    page += "<div class='status'><p><strong>🔄 Escaneo en progreso...</strong></p></div>";
    page += "<meta http-equiv='refresh' content='3'>";
  }
  
  if (lastScanHTML.length() > 0) {
    page += lastScanHTML;
  } else {
    page += "<div class='status'><p>Presiona 'Escanear Dispositivos' para buscar dispositivos cercanos</p></div>";
  }
  
  page += "<div class='status'>";
  page += "<form action='/rescan' method='get'>";
  page += "<button class='scan-btn' type='submit' " + String(scanInProgress ? "disabled" : "") + ">";
  page += scanInProgress ? "⏳ Escaneando..." : "🔍 Escanear Dispositivos";
  page += "</button></form></div>";
  
  page += "<div class='footer'>";
  page += "<p>📡 ESP32 IP: <code>" + WiFi.softAPIP().toString() + "</code> | ";
  page += "📊 Base OUI: " + String(OUI_DATABASE_SIZE) + " fabricantes</p>";
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
  server.send(404, "text/plain", "Página no encontrada");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== ESP32 Room Device Scanner con OUI Detection ===");

  // Inicializar BLE
  Serial.println("Inicializando BLE...");
  BLEDevice::init("");
  Serial.println("BLE inicializado correctamente");

  // Iniciar WiFi AP
  Serial.println("Configurando WiFi AP...");
  WiFi.mode(WIFI_AP);
  bool ok = WiFi.softAP(apSSID, apPASS);
  
  if (ok) {
    Serial.print("WiFi AP iniciado. SSID: ");
    Serial.print(apSSID);
    Serial.print(" | IP: ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("Error al iniciar AP.");
    return;
  }

  // Configurar servidor web
  server.on("/", handleRoot);
  server.on("/rescan", handleRescan);
  server.onNotFound(handleNotFound);
  server.begin();
  
  Serial.println("Servidor web iniciado en http://192.168.4.1");
  Serial.printf("Base de datos OUI: %d fabricantes conocidos\n", OUI_DATABASE_SIZE);
  
  // Escaneo inicial
  Serial.println("\nRealizando escaneo inicial...");
  performFullScan();
  
  Serial.println("\nSistema listo! 🚀");
}

void loop() {
  server.handleClient();
  delay(10);
}