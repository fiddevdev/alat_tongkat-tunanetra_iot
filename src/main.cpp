// ===============================================================
//  Proyek: Tongkat Tunanetra Arah (Kanan–Kiri–Depan) + DFPlayer + Sensor Air + Proximity + GPS
//  Board:  ESP32
//  Deskripsi tambahan:
//  - GPS Beitian BN-220 (TX→RX33, RX→TX32) baud 57600
//  - Menampilkan latitude & longitude mentah dan desimal dari NMEA ($GPGGA)
// ===============================================================

#include <Arduino.h>
#include <ArduinoJson.h>

#ifdef ESP32
#include <WiFi.h>
#include <HTTPClient.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#endif

#include "DFRobotDFPlayerMini.h"

// ===== WiFi dan server =====
const char* ssid = "wifi-iot";
const char* password = "password-iot";
const char* server_url = "http://labrobotika.go-web.my.id/server.php?apikey=";
const char* apikey = "d5191f277ec094e277e96155cc4a4215";
unsigned long delay_iot = 100;
unsigned long last_request = 0;
DynamicJsonDocument data(2048);

// ===== Pin Sensor Ultrasonik =====
#define TRIG_KANAN 13
#define ECHO_KANAN 12
#define TRIG_KIRI 18
#define ECHO_KIRI 19
#define TRIG_DEPAN 22
#define ECHO_DEPAN 23

// ===== Pin Sensor Air dan Proximity =====
#define WATER_SENSOR 25
#define PROXIMITY_SENSOR 26

// ===== DFPlayer Mini =====
HardwareSerial dfSerial(1);
DFRobotDFPlayerMini myDFPlayer;
#define DF_RX 16
#define DF_TX 17

// ===== GPS (UART2) =====
HardwareSerial gpsSerial(2);
#define GPS_RX 33   // RX ESP32 (ke TX GPS)
#define GPS_TX 32   // TX ESP32 (ke RX GPS)
#define GPS_BAUD 57600

// ===== Variabel =====
unsigned long lastPlayTime = 0;
const unsigned long delayAntarAudio = 3000;
bool waterWasWet = false;
bool proximityWasActive = false;

// ===== Variabel koordinat GPS =====
String latitudeRaw = "";
String longitudeRaw = "";
String latDir = "";
String lonDir = "";
float latitudeDec = 0.0;
float longitudeDec = 0.0;

// ===== Fungsi ukur jarak =====
float getDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH, 25000);
  return duration * 0.034 / 2;
}

// ===== Fungsi konversi GPS mentah ke desimal =====
float convertToDecimal(String rawValue) {
  if (rawValue == "") return 0.0;
  float val = rawValue.toFloat();
  int degrees = int(val / 100);
  float minutes = val - (degrees * 100);
  return degrees + (minutes / 60.0);
}

// ===== WiFi =====
void setupWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Menghubungkan WiFi");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 10) {
    delay(1000);
    Serial.print(".");
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" WiFi Terhubung!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(" Gagal terhubung ke WiFi.");
  }
}

// ===== Kirim data ke server =====
void prosesData(String queryParams = "") {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  String url = String(server_url) + String(apikey) + queryParams;
  url.replace(" ", "%20");
  Serial.println("Request ke: " + url);
  http.begin(url);
  http.addHeader("Accept", "application/json");
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    String response = http.getString();
    DeserializationError err = deserializeJson(data, response);
    if (err) {
      Serial.println("Gagal parsing JSON: " + String(err.c_str()));
    } else {
      Serial.println("Data JSON diterima.");
    }
  } else {
    Serial.println("HTTP Error: " + String(httpCode));
  }
  http.end();
}

// ===== Fungsi parsing GGA =====
void parseGGA(String nmea) {
  int fieldIndex = 0;
  String fields[15];
  for (int i = 0; i < nmea.length(); i++) {
    if (nmea[i] == ',') fieldIndex++;
    else fields[fieldIndex] += nmea[i];
  }

  latitudeRaw = fields[2];   // contoh: 0136.99816
  latDir = fields[3];        // S
  longitudeRaw = fields[4];  // contoh: 10337.41959
  lonDir = fields[5];        // E

  if (latitudeRaw != "" && longitudeRaw != "") {
    latitudeDec = convertToDecimal(latitudeRaw);
    longitudeDec = convertToDecimal(longitudeRaw);
    if (latDir == "S") latitudeDec = -latitudeDec;
    if (lonDir == "W") longitudeDec = -longitudeDec;

    Serial.println("===========================================");
    Serial.print("Latitude RAW : "); Serial.println(latitudeRaw);
    Serial.print("Longitude RAW: "); Serial.println(longitudeRaw);
    Serial.print("Latitude DEC : "); Serial.println(latitudeDec, 6);
    Serial.print("Longitude DEC: "); Serial.println(longitudeDec, 6);
    Serial.println("===========================================");
      if (WiFi.status() == WL_CONNECTED && millis() - last_request >= delay_iot) {
    if (latitudeRaw != "" && longitudeRaw != "") {
prosesData("&latd=" + String(latitudeDec, 6) + "&lotd=" + String(longitudeDec, 6));
    } else {
      Serial.println("⚠️ Data GPS belum tersedia, menunggu fix...");
    }
    last_request = millis();

    String out_1 = data["out_1"] | "0";
    String out_2 = data["out_2"] | "0";
    String out_3 = data["out_3"] | "0";

    Serial.println("out_1: " + out_1);
    Serial.println("out_2: " + out_2);
    Serial.println("out_3: " + out_3);
  }
  }
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  Serial.println("=== Sistem Tongkat Tunanetra dengan GPS Mentah + Desimal ===");

  // Sensor ultrasonik
  pinMode(TRIG_KANAN, OUTPUT);
  pinMode(ECHO_KANAN, INPUT);
  pinMode(TRIG_KIRI, OUTPUT);
  pinMode(ECHO_KIRI, INPUT);
  pinMode(TRIG_DEPAN, OUTPUT);
  pinMode(ECHO_DEPAN, INPUT);

  // Sensor air & proximity
  pinMode(WATER_SENSOR, INPUT);
  pinMode(PROXIMITY_SENSOR, INPUT);

  // DFPlayer
  dfSerial.begin(9600, SERIAL_8N1, DF_RX, DF_TX);
  Serial.println("Menghubungkan DFPlayer Mini...");
  if (!myDFPlayer.begin(dfSerial)) {
    Serial.println("❌ Gagal terhubung ke DFPlayer Mini!");
    while (true);
  }
  myDFPlayer.volume(25);
  Serial.println("✅ DFPlayer Mini Siap!");

  // GPS
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX, GPS_TX);
  Serial.println("✅ GPS Beitian Siap (baud 57600)");

  // WiFi
  setupWiFi();
  last_request = millis();
}

// ===== LOOP =====
void loop() {
  // ==== BACA DATA GPS ====
  static String nmeaLine = "";
  while (gpsSerial.available()) {
    char c = gpsSerial.read();
    if (c == '\n') {
      if (nmeaLine.startsWith("$GNGGA") || nmeaLine.startsWith("$GPGGA")) {
        parseGGA(nmeaLine);
      }
      nmeaLine = "";
    } else {
      nmeaLine += c;
    }
  }

  // ==== Sensor jarak ====
  float jarakKanan = getDistance(TRIG_KANAN, ECHO_KANAN);
  float jarakKiri  = getDistance(TRIG_KIRI, ECHO_KIRI);
  float jarakDepan = getDistance(TRIG_DEPAN, ECHO_DEPAN);

  bool waterNowWet = (digitalRead(WATER_SENSOR) == HIGH);
  bool proximityNowActive = (digitalRead(PROXIMITY_SENSOR) == HIGH);

  Serial.print("Kanan: "); Serial.print(jarakKanan); Serial.print(" cm | ");
  Serial.print("Kiri: "); Serial.print(jarakKiri); Serial.print(" cm | ");
  Serial.print("Depan: "); Serial.print(jarakDepan); Serial.print(" cm | ");
  Serial.print("Air: "); Serial.print(waterNowWet ? "Basah" : "Kering"); Serial.print(" | ");
  Serial.print("Proximity: "); Serial.println(proximityNowActive ? "Objek" : "Tidak");

  unsigned long waktuSekarang = millis();

  // ==== Audio sesuai kondisi ====
  if (jarakKanan > 0 && jarakKanan < 10 && waktuSekarang - lastPlayTime > delayAntarAudio) {
    myDFPlayer.play(6);
    Serial.println("🔊 Objek di KANAN! Memutar 0006.mp3");
    lastPlayTime = waktuSekarang;
  } 
  else if (jarakKiri > 0 && jarakKiri < 10 && waktuSekarang - lastPlayTime > delayAntarAudio) {
    myDFPlayer.play(5);
    Serial.println("🔊 Objek di KIRI! Memutar 0005.mp3");
    lastPlayTime = waktuSekarang;
  } 
  else if (jarakDepan > 0 && jarakDepan < 60 && waktuSekarang - lastPlayTime > delayAntarAudio) {
    myDFPlayer.play(4);
    Serial.println("🔊 Objek di DEPAN! Memutar 0004.mp3");
    lastPlayTime = waktuSekarang;
  } 
  else if (waterNowWet && !waterWasWet && waktuSekarang - lastPlayTime > delayAntarAudio) {
    myDFPlayer.play(3);
    Serial.println("💧 Sensor air BASAH! Memutar 0003.mp3 sekali.");
    lastPlayTime = waktuSekarang;
  } 
  else if (proximityNowActive && !proximityWasActive && waktuSekarang - lastPlayTime > delayAntarAudio) {
    myDFPlayer.play(2);
    Serial.println("🚶 Sensor proximity AKTIF! Memutar 0002.mp3 sekali.");
    lastPlayTime = waktuSekarang;
  }

  waterWasWet = waterNowWet;
  proximityWasActive = proximityNowActive;

  // ==== Kirim data ke server ====


  delay(500);
}
