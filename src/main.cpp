#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <DHT.h>

#define DHTPIN 4
#define DHTTYPE DHT22 // Ubah ke DHT11 jika pakai DHT11
#define RESET_BTN 0   // Tombol BOOT bawaan ESP32
#define LED_PIN 2     // LED biru on-board ESP32 (GPIO 2)

DHT dht(DHTPIN, DHTTYPE);
Preferences prefs;

String serverUrl = "http://11.11.11.108/temp-api/api.php";
unsigned long lastSend = 0;
const unsigned long interval = 10000;

// Fungsi untuk menyalakan LED otomatis saat masuk mode Hotspot / Setup
void configModeCallback(WiFiManager *myWiFiManager) {
  Serial.println("Masuk mode Setup Hotspot! LED biru dinyalakan.");
  digitalWrite(LED_PIN, HIGH);
}

void kirimKeServer() {
  if (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_PIN, HIGH); // Nyalakan LED jika WiFi tiba-tiba putus
    return;
  }

  digitalWrite(LED_PIN, LOW); // Pastikan LED mati saat WiFi normal

  float suhu = dht.readTemperature();
  float kelembaban = dht.readHumidity();

  if (isnan(suhu) || isnan(kelembaban)) {
    Serial.println("Gagal membaca sensor DHT!");
    return;
  }

  HTTPClient http;
  http.begin(serverUrl);
  http.addHeader("Content-Type", "application/json");
  // http.addHeader("Host", "sensor.test"); // Aktifkan jika pakai Virtual Host

  String jsonPayload = "{\"suhu\":" + String(suhu, 2) + ",\"kelembaban\":" + String(kelembaban, 2) + "}";
  int httpResponseCode = http.POST(jsonPayload);

  if (httpResponseCode > 0) {
    Serial.print("Terkirim! HTTP: ");
    Serial.println(httpResponseCode);
  } else {
    Serial.print("Gagal kirim, error: ");
    Serial.println(http.errorToString(httpResponseCode));
  }

  http.end();
}

void setup() {
  Serial.begin(115200);
  pinMode(RESET_BTN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // Nyala saat baru booting/mencari WiFi

  dht.begin();

  prefs.begin("config", false);
  serverUrl = prefs.getString("url", serverUrl);

  WiFiManager wm;
  wm.setConfigPortalTimeout(180);
  wm.setAPCallback(configModeCallback); // Pasang pemicu LED mode hotspot

  WiFiManagerParameter custom_url("server", "URL API Server", serverUrl.c_str(), 100);
  wm.addParameter(&custom_url);

  if (!wm.autoConnect("ESP32-Suhu-Setup", "12345678")) {
    Serial.println("Timeout, merestart ESP32...");
    ESP.restart();
  }

  String newUrl = String(custom_url.getValue());
  if (newUrl.length() > 0 && newUrl != serverUrl) {
    serverUrl = newUrl;
    prefs.putString("url", serverUrl);
  }

  // WiFi berhasil terhubung -> matikan LED biru
  digitalWrite(LED_PIN, LOW);
  Serial.println("WiFi terhubung! Target API: " + serverUrl);
}

void loop() {
  // Tekan & tahan tombol BOOT selama 3 detik untuk reset
  if (digitalRead(RESET_BTN) == LOW) {
    delay(3000);
    if (digitalRead(RESET_BTN) == LOW) {
      Serial.println("Reset pengaturan WiFi...");

      // Kedipkan LED biru 5x sebagai tanda reset berhasil
      for (int i = 0; i < 5; i++) {
        digitalWrite(LED_PIN, HIGH);
        delay(150);
        digitalWrite(LED_PIN, LOW);
        delay(150);
      }

      WiFiManager wm;
      wm.resetSettings();
      ESP.restart();
    }
  }

  // Pantau status WiFi secara real-time
  if (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_PIN, HIGH);
  } else {
    digitalWrite(LED_PIN, LOW);
  }

  if (millis() - lastSend >= interval) {
    lastSend = millis();
    kirimKeServer();
  }
}