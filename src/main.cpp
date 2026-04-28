#include <WiFi.h>

const char* ssid = "ESP_CAM";
const char* password = "12345678";

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println("Demarrage ESP32-CAM (test AP)");

  WiFi.mode(WIFI_AP);
  bool apOk = WiFi.softAP(ssid, password);

  if (!apOk) {
    Serial.println("Echec creation AP WiFi");
    return;
  }

  Serial.println("WiFi AP pret");
  Serial.print("SSID: ");
  Serial.println(ssid);
  Serial.print("IP AP: ");
  Serial.println(WiFi.softAPIP());
}

void loop() {
  static unsigned long lastLog = 0;
  if (millis() - lastLog >= 5000) {
    lastLog = millis();
    Serial.println("AP actif");
  }
}