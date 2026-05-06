#include <Wire.h>
#include <RTClib.h>
#include <SD.h>
#include <ArduinoLowPower.h>

RTC_DS3231 rtc;

// ==== KONFIGURACJA SPRZĘTOWA ====

const int sensorPin = A1;
const int relayPin  = 2;

// Im WIĘKSZA wartość, tym BARDZIEJ SUCHO
const int DRY_THRESHOLD = 620;

// Liczba próbek czujnika
const int NUM_SAMPLES = 5;

// Czas pracy pompki (ms)
const unsigned long PUMP_TIME_MS = 5000;

// ==== KONFIGURACJA CZASÓW POMIARU ====

const int NUM_MEASUREMENTS = 3;

struct MeasurementTime {
  int hour;
  int minute;
};

MeasurementTime times[NUM_MEASUREMENTS] = {
 {8, 00},
 {14, 00},
 {20, 00},
};

// Flagi wykonania pomiarów
bool measurementDone[NUM_MEASUREMENTS];
int lastDay = -1;


// ==== SETUP ====

void setup() {

  Serial.begin(9600);
  delay(3000);

  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW); // pompa OFF

  Wire.begin();


  
  // RTC
  if (!rtc.begin()) {
    Serial.println("BLAD: Nie wykryto RTC!");
    while (1);
  }

  if (rtc.lostPower()) {
    Serial.println("RTC stracil zasilanie - ustawiam czas");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  

  // SD
  if (!SD.begin(SDCARD_SS_PIN)) {
    Serial.println("BLAD: Nie wykryto karty SD!");
    while (1);
  }

  // Reset flag
  for (int i = 0; i < NUM_MEASUREMENTS; i++) {
    measurementDone[i] = false;
  }

  Serial.println("System podlewania uruchomiony");
}


// ==== FUNKCJA POMIARU ====

int measureMoistureAverage() {
  long sum = 0;

  Serial.println("Rozpoczynam pomiar wilgotnosci:");

  for (int i = 0; i < NUM_SAMPLES; i++) {
    int value = analogRead(sensorPin);
    sum += value;

    Serial.print("  Pomiar ");
    Serial.print(i + 1);
    Serial.print(": ");
    Serial.println(value);

    delay(200);
  }

  int average = sum / NUM_SAMPLES;

  Serial.print(">>> SREDNIA z ");
  Serial.print(NUM_SAMPLES);
  Serial.print(" pomiarow: ");
  Serial.println(average);

  return average;
}


// ==== ZAPIS NA SD ====

void logToSD(DateTime now, int avgMoisture) {
  File logFile = SD.open("wilgotno.csv", FILE_WRITE);

  if (logFile) {
    // data;godzina;wartosc
    logFile.print(now.year());
    logFile.print("-");
    logFile.print(now.month());
    logFile.print("-");
    logFile.print(now.day());
    logFile.print(";");

    logFile.print(now.hour());
    logFile.print(":");
    if (now.minute() < 10) logFile.print("0");
    logFile.print(now.minute());
    logFile.print(";");

    logFile.println(avgMoisture);

    logFile.close();
    Serial.println("Zapisano dane na karte SD");
  } else {
    Serial.println("BLAD zapisu na karte SD");
  }
}


// ==== LOOP ====

void loop() {
  DateTime now = rtc.now();
  Serial.print(now.hour());
  Serial.print(":");
  Serial.println(now.minute());
  Serial.flush();

  // Nowy dzień → reset flag
  if (now.day() != lastDay) {
    for (int i = 0; i < NUM_MEASUREMENTS; i++) {
      measurementDone[i] = false;
    }
    lastDay = now.day();
  }

  // Sprawdzenie wszystkich zaplanowanych pomiarów
  for (int i = 0; i < NUM_MEASUREMENTS; i++) {
    if (!measurementDone[i] &&
        now.hour() == times[i].hour &&
        now.minute() == times[i].minute) {

      Serial.print("===== POMIAR NR ");
      Serial.print(i + 1);
      Serial.println(" =====");

      int avgMoisture = measureMoistureAverage();

      logToSD(now, avgMoisture);

      if (avgMoisture > DRY_THRESHOLD) {
        Serial.println("WYNIK: SUCHO -> Wlaczam pompe");
        digitalWrite(relayPin, HIGH);
        delay(PUMP_TIME_MS);
        digitalWrite(relayPin, LOW);
        Serial.println("Pompa wylaczona");
      } else {
        Serial.println("WYNIK: WILGOTNO -> Nie podlewam");
      }

      Serial.println("===========================");
      measurementDone[i] = true;
    }
  }

  LowPower.sleep(60 * 1000);
}
