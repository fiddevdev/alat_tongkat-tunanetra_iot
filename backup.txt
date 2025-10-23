// ===============================================================
//  Proyek: Tongkat Tunanetra Arah (Kanan–Kiri–Depan) + DFPlayer + Sensor Air + Proximity
//  Board:  ESP32
//  Deskripsi:
//  - Sensor KANAN (TRIG=13, ECHO=12) < 10 cm → audio 0006.mp3 (dikanan mu ada penghalang)
//  - Sensor KIRI  (TRIG=18, ECHO=19) < 10 cm → audio 0005.mp3 (dikiri mu ada penghalang)
//  - Sensor DEPAN (TRIG=22, ECHO=23) < 60 cm → audio 0004.mp3 (didepan mu ada penghalang)
//  - Sensor AIR   (GPIO25) HIGH = basah → audio 0003.mp3 (sekali)
//  - Sensor PROXIMITY (GPIO26) HIGH = objek terdeteksi → audio 0002.mp3 (sekali)
//
//  Daftar audio:
//  001 hati hati
//  002 awas di depan ada tangga
//  003 hati hati ada air
//  004 di depan mu ada penghalang
//  005 di kirimu ada penghalang
//  006 di kanan mu ada penghalang
// ===============================================================

#include "Arduino.h"
#include "DFRobotDFPlayerMini.h"

// ===== Pin Sensor Ultrasonik =====
#define TRIG_KANAN 13
#define ECHO_KANAN 12

#define TRIG_KIRI 18
#define ECHO_KIRI 19

#define TRIG_DEPAN 22
#define ECHO_DEPAN 23

// ===== Pin Sensor Air dan Proximity =====
#define WATER_SENSOR 25     // HIGH = basah
#define PROXIMITY_SENSOR 26 // HIGH = objek terdeteksi

// ===== DFPlayer Mini (UART1 di ESP32) =====
HardwareSerial mySerial(1);
DFRobotDFPlayerMini myDFPlayer;
#define DF_RX 16  // RX DFPlayer ← TX ESP32
#define DF_TX 17  // TX DFPlayer → RX ESP32

// ===== Variabel =====
unsigned long lastPlayTime = 0;
const unsigned long delayAntarAudio = 3000; // jeda minimal antar audio (ms)

bool waterWasWet = false;     // status terakhir air
bool proximityWasActive = false; // status terakhir proximity

// ===== Fungsi untuk Mengukur Jarak =====
float getDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 25000); // timeout 25 ms
  float distance = duration * 0.034 / 2;         // konversi ke cm
  return distance;
}

void setup() {
  Serial.begin(115200);

  // Inisialisasi sensor ultrasonik
  pinMode(TRIG_KANAN, OUTPUT);
  pinMode(ECHO_KANAN, INPUT);
  pinMode(TRIG_KIRI, OUTPUT);
  pinMode(ECHO_KIRI, INPUT);
  pinMode(TRIG_DEPAN, OUTPUT);
  pinMode(ECHO_DEPAN, INPUT);

  // Inisialisasi sensor air & proximity
  pinMode(WATER_SENSOR, INPUT);
  pinMode(PROXIMITY_SENSOR, INPUT);

  // Inisialisasi DFPlayer
  mySerial.begin(9600, SERIAL_8N1, DF_RX, DF_TX);
  Serial.println("Menghubungkan DFPlayer Mini...");

  if (!myDFPlayer.begin(mySerial)) {
    Serial.println("❌ Gagal terhubung ke DFPlayer Mini!");
    while (true);
  }

  Serial.println("✅ DFPlayer Mini Siap!");
  myDFPlayer.volume(25);  // volume 0–30
}

void loop() {
  // ==== Baca sensor ultrasonik ====
  float jarakKanan = getDistance(TRIG_KANAN, ECHO_KANAN);
  float jarakKiri  = getDistance(TRIG_KIRI, ECHO_KIRI);
  float jarakDepan = getDistance(TRIG_DEPAN, ECHO_DEPAN);

  // ==== Baca sensor air dan proximity ====
  bool waterNowWet = (digitalRead(WATER_SENSOR) == HIGH);
  bool proximityNowActive = (digitalRead(PROXIMITY_SENSOR) == HIGH);

  Serial.print("Kanan: "); Serial.print(jarakKanan); Serial.print(" cm | ");
  Serial.print("Kiri: "); Serial.print(jarakKiri); Serial.print(" cm | ");
  Serial.print("Depan: "); Serial.print(jarakDepan); Serial.print(" cm | ");
  Serial.print("Air: "); Serial.print(waterNowWet ? "Basah" : "Kering"); Serial.print(" | ");
  Serial.print("Proximity: "); Serial.println(proximityNowActive ? "Objek" : "Tidak");

  unsigned long waktuSekarang = millis();

  // ==== Deteksi arah halangan ====
  if (jarakKanan > 0 && jarakKanan < 10 && waktuSekarang - lastPlayTime > delayAntarAudio) {
    myDFPlayer.play(6);  // kanan
    Serial.println("🔊 Objek di KANAN! Memutar 0006.mp3");
    lastPlayTime = waktuSekarang;
  } 
  else if (jarakKiri > 0 && jarakKiri < 10 && waktuSekarang - lastPlayTime > delayAntarAudio) {
    myDFPlayer.play(5);  // kiri
    Serial.println("🔊 Objek di KIRI! Memutar 0005.mp3");
    lastPlayTime = waktuSekarang;
  } 
  else if (jarakDepan > 0 && jarakDepan < 60 && waktuSekarang - lastPlayTime > delayAntarAudio) {
    myDFPlayer.play(4);  // depan
    Serial.println("🔊 Objek di DEPAN! Memutar 0004.mp3");
    lastPlayTime = waktuSekarang;
  } 
  // ==== Deteksi Air (sekali saat pertama kali basah) ====
  else if (waterNowWet && !waterWasWet && waktuSekarang - lastPlayTime > delayAntarAudio) {
    myDFPlayer.play(3);  // air
    Serial.println("💧 Sensor air BASAH! Memutar 0003.mp3 sekali.");
    lastPlayTime = waktuSekarang;
  } 
  // ==== Deteksi Proximity (sekali saat pertama kali objek terdeteksi) ====
  else if (proximityNowActive && !proximityWasActive && waktuSekarang - lastPlayTime > delayAntarAudio) {
    myDFPlayer.play(2);  // proximity
    Serial.println("🚶 Sensor proximity AKTIF! Memutar 0002.mp3 sekali.");
    lastPlayTime = waktuSekarang;
  }

  // Update status terakhir
  waterWasWet = waterNowWet;
  proximityWasActive = proximityNowActive;

  delay(200);
}
