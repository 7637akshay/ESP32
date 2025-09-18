#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <Preferences.h>

// WiFi credentials
const char* ssid = "Knest-R&D-2G";
const char* password = "Knest@007";

// GitHub repo links
const char* version_url = "https://raw.githubusercontent.com/7637akshay/ESP32-S4/main/version.txt";
const char* base_firmware_url = "https://raw.githubusercontent.com/7637akshay/ESP32-S4/main/firmware_V";

Preferences preferences;   // For storing current version
String currentVersion = "1.0";   // default if nothing stored

// Timer variables
unsigned long lastUpdateCheck = 0;
const unsigned long updateInterval = 5 * 60 * 1000; // 5 minutes in ms

// ------------------ NVS helpers ------------------
void saveVersion(String ver) {
  preferences.begin("ota", false);
  preferences.putString("fw_version", ver);
  preferences.end();
}

String loadVersion() {
  preferences.begin("ota", true);
  String ver = preferences.getString("fw_version", "1.0");
  preferences.end();
  return ver;
}

// ------------------ OTA Update ------------------
void checkForUpdates() {
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;

  Serial.println("\n[OTA] Checking for update...");
  if (http.begin(client, version_url)) {
    int httpCode = http.GET();

    if (httpCode == 200) {
      String newVersion = http.getString();
      newVersion.trim();
      Serial.printf("[OTA] Latest version on server: %s\n", newVersion.c_str());
      Serial.printf("[OTA] Current device version: %s\n", currentVersion.c_str());

      if (newVersion != currentVersion) {
        Serial.println("[OTA] New version available! Starting update...");

        // Build firmware filename dynamically (e.g., firmware_V1.1.bin)
        String firmwareURL = String(base_firmware_url) + newVersion + ".bin";
        Serial.printf("[OTA] Downloading firmware: %s\n", firmwareURL.c_str());

        HTTPClient httpUpdate;
        if (httpUpdate.begin(client, firmwareURL)) {
          int updateCode = httpUpdate.GET();

          if (updateCode == 200) {
            int contentLength = httpUpdate.getSize();
            if (contentLength > 0) {
              if (Update.begin(contentLength)) {
                WiFiClient* stream = httpUpdate.getStreamPtr();
                size_t written = Update.writeStream(*stream);
                if (written == contentLength) {
                  Serial.println("[OTA] Firmware written successfully");
                } else {
                  Serial.printf("[OTA] Written only : %d/%d bytes\n", written, contentLength);
                }

                if (Update.end()) {
                  if (Update.isFinished()) {
                    Serial.println("[OTA] ✅ Update successful! Rebooting...");
                    saveVersion(newVersion);   // Save new version to flash
                    ESP.restart();
                  } else {
                    Serial.println("[OTA] Update not finished.");
                  }
                } else {
                  Serial.printf("[OTA] Update error: %s\n", Update.errorString());
                }
              }
            }
          } else {
            Serial.printf("[OTA] Firmware download failed. HTTP code: %d\n", updateCode);
          }
          httpUpdate.end();
        }
      } else {
        Serial.println("[OTA] Device is already on the latest version.");
      }
    } else {
      Serial.printf("[OTA] Version check failed. HTTP code: %d\n", httpCode);
    }
    http.end();
  }
}

// ------------------ Setup & Loop ------------------
void setup() {
  Serial.begin(115200);
  pinMode(2, OUTPUT);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n WiFi connected!");

  currentVersion = loadVersion();  // Load stored version
  Serial.printf("[OTA] Loaded version from flash: %s\n", currentVersion.c_str());

  // Initial update check at boot
  checkForUpdates();
  lastUpdateCheck = millis();
}

void loop() {
  // Blink LED (normal operation)
  digitalWrite(2, HIGH);
  delay(500);
  digitalWrite(2, LOW);
  delay(500);

  // Check for updates every 5 minutes
  if (millis() - lastUpdateCheck >= updateInterval) {
    checkForUpdates();
    lastUpdateCheck = millis();
  }
}
