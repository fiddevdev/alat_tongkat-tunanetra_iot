// ===============================================================
//  Proyek: Tongkat Tunanetra dengan 3 Sensor Ultrasonik HC-SR04
//  Board:  ESP32
//  Deskripsi:
//  - Jika salah satu sensor mendeteksi objek < 11 cm → buzzer aktif
// ===============================================================
#include <Arduino.h>
// Sensor 1
#define TRIG1 13
#define ECHO1 12

// Sensor 2
#define TRIG2 18
#define ECHO2 19

// Sensor 3
#define TRIG3 22
#define ECHO3 23

// Buzzer
#define BUZZER 15   // ⚠️ Catatan: Jika pin 15 digunakan untuk sensor juga, ganti ke pin lain (misal 4 atau 27)

float distance1, distance2, distance3;
const int batasJarak = 11; // cm

float getDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 25000); // batas waktu 25 ms (≈ 4 meter)
  float distance = duration * 0.034 / 2;         // konversi ke cm
  return distance;
}

void setup() {
  Serial.begin(115200);

  pinMode(TRIG1, OUTPUT);
  pinMode(ECHO1, INPUT);

  pinMode(TRIG2, OUTPUT);
  pinMode(ECHO2, INPUT);

  pinMode(TRIG3, OUTPUT);
  pinMode(ECHO3, INPUT);

  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);
}

void loop() {
  distance1 = getDistance(TRIG1, ECHO1);
  distance2 = getDistance(TRIG2, ECHO2);
  distance3 = getDistance(TRIG3, ECHO3);

  Serial.print("Sensor1: "); Serial.print(distance1); Serial.print(" cm | ");
  Serial.print("Sensor2: "); Serial.print(distance2); Serial.print(" cm | ");
  Serial.print("Sensor3: "); Serial.print(distance3); Serial.println(" cm");

  // Jika salah satu sensor mendeteksi objek < 11 cm
  if (distance1 < batasJarak || distance2 < batasJarak || distance3 < batasJarak) {
    digitalWrite(BUZZER, HIGH); // hidupkan buzzer
  } else {
    digitalWrite(BUZZER, LOW);  // matikan buzzer
  }

  delay(200); // pembacaan tiap 0.2 detik
}

// Fungsi untuk menghitung jarak dari sensor HC-SR04

