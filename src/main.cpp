// ===============================================================
//  Proyek: Tongkat Tunanetra Arah + DFPlayer + Sensor Air + Proximity + GPS
//  Board: ESP32
//  Perbaikan: Kirim data HTTPClient hanya setelah GPS DEC dicetak
//  Modifikasi: Gunakan char buffer untuk parsing GPS guna menghindari fragmentasi memory dan potensi crash Guru Meditation
// ===============================================================

#include <Arduino.h>
#include <ArduinoJson.h>
#include <string.h> // Untuk strncmp, strcpy, dll.

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

unsigned long delay_iot = 5000; // minimal interval HTTP request
unsigned long last_request = 0;

DynamicJsonDocument data(2048);

// ===== Pin Sensor Ultrasonik =====
#define TRIG_KANAN 13
#define ECHO_KANAN 12
#define TRIG_KIRI 18
#define ECHO_KIRI 19
#define TRIG_DEPAN 22
#define ECHO_DEPAN 23

// ===== Pin Sensor Air & Proximity =====
#define WATER_SENSOR 25
#define PROXIMITY_SENSOR 26

// ===== DFPlayer Mini =====
HardwareSerial dfSerial(1);
DFRobotDFPlayerMini myDFPlayer;
#define DF_RX 16
#define DF_TX 17

// ===== GPS (UART2) =====
HardwareSerial gpsSerial(2);
#define GPS_RX 33
#define GPS_TX 32
#define GPS_BAUD 57600

// ===== Variabel =====
unsigned long lastPlayTime = 0;
const unsigned long delayAntarAudio = 3000;
bool waterWasWet = false;
bool proximityWasActive = false;

// ===== Variabel koordinat GPS =====
char latitudeRaw[20] = "";
char longitudeRaw[20] = "";
char latDir[2] = "";
char lonDir[2] = "";
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
float convertToDecimal(const char* rawValue) {
    if (strlen(rawValue) == 0) return 0.0;
    float val = atof(rawValue);
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
void prosesData() {
    if (WiFi.status() != WL_CONNECTED) return;
    if (millis() - last_request < delay_iot) return;

    HTTPClient http;
    String url = String(server_url) + String(apikey) + "&latd=" + String(latitudeDec, 6) + "&lotd=" + String(longitudeDec, 6);
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
    last_request = millis();
}

// ===== Fungsi parsing GGA =====
void parseGGA(const char* nmea) {
    char fields[15][30] = {0}; // Array untuk fields, max 30 char per field
    int fieldIndex = 0;
    char *p = (char*)nmea;
    char *start = p;

    while (*p && fieldIndex < 15) {
        if (*p == ',') {
            *p = '\0';
            strcpy(fields[fieldIndex], start);
            fieldIndex++;
            start = p + 1;
        }
        p++;
    }
    // Field terakhir
    if (fieldIndex < 15) {
        strcpy(fields[fieldIndex], start);
        fieldIndex++;
    }

    strcpy(latitudeRaw, fields[2]);
    strcpy(latDir, fields[3]);
    strcpy(longitudeRaw, fields[4]);
    strcpy(lonDir, fields[5]);

    if (strlen(latitudeRaw) > 0 && strlen(longitudeRaw) > 0) {
        latitudeDec = convertToDecimal(latitudeRaw);
        longitudeDec = convertToDecimal(longitudeRaw);
        if (strcmp(latDir, "S") == 0) latitudeDec = -latitudeDec;
        if (strcmp(lonDir, "W") == 0) longitudeDec = -longitudeDec;

        // Tampilkan di Serial Monitor
        Serial.println("===========================================");
        Serial.print("Latitude RAW : "); Serial.println(latitudeRaw);
        Serial.print("Longitude RAW: "); Serial.println(longitudeRaw);
        Serial.print("Latitude DEC : "); Serial.println(latitudeDec, 6);
        Serial.print("Longitude DEC: "); Serial.println(longitudeDec, 6);
        Serial.println("===========================================");

        // ==== Kirim data ke server setelah dicetak ====
        prosesData();
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
}

// ===== LOOP =====
void loop() {
    // ==== BACA DATA GPS ====
    static char nmeaLine[256];
    static int idx = 0;
    while (gpsSerial.available()) {
        char c = gpsSerial.read();
        if (c == '\n') {
            nmeaLine[idx] = '\0';
            if (strncmp(nmeaLine, "$GNGGA", 6) == 0 || strncmp(nmeaLine, "$GPGGA", 6) == 0) {
                parseGGA(nmeaLine); // prosesData dipanggil di sini
            }
            idx = 0;
        } else if (c != '\r') {
            if (idx < 255) nmeaLine[idx++] = c;
        }
    }

    // ==== Sensor jarak ====
    float jarakKanan = getDistance(TRIG_KANAN, ECHO_KANAN);
    float jarakKiri  = getDistance(TRIG_KIRI, ECHO_KIRI);
    float jarakDepan = getDistance(TRIG_DEPAN, ECHO_DEPAN);

    bool waterNowWet = (digitalRead(WATER_SENSOR) == HIGH);
    bool proximityNowActive = (digitalRead(PROXIMITY_SENSOR) == LOW);

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
else if (proximityNowActive && waktuSekarang - lastPlayTime > delayAntarAudio) {
    myDFPlayer.play(2);
    Serial.println("🚶 Sensor proximity AKTIF! Memutar 0002.mp3 berulang.");
    lastPlayTime = waktuSekarang;
}

    waterWasWet = waterNowWet;
    proximityWasActive = proximityNowActive;

    delay(600); // loop ringan
}