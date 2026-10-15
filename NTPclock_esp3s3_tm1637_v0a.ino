// ceas NTP cu ESP32-S3 Supermini si afisaj din 4 cifre cu TM1637
// implicare: Nicu FLORICA (niq_ro)
// v.0 - versiune initiala
// v.0a - conectare la 2 retele wifi, schimbare intensitatea luminasa zi-noapte, verificare conectare la retea

#include <WiFi.h>
#include <WiFiMulti.h>
#include <ezTime.h>
#include <TM1637TinyDisplay.h>

// Configurare pini afișaj
#define CLK_PIN 5
#define DIO_PIN 6

// Niveluri de luminozitate (pe o scară de la 0 la 7)
#define LUMINA_ZI 4   // Luminozitate medie pentru zi
#define LUMINA_NOAPTE 1 // Luminozitate foarte discretă pentru noapte

// Inițializare obiect pentru gestionarea mai multor rețele Wi-Fi
WiFiMulti wifiMulti;

// Inițializare ecran
TM1637TinyDisplay display(CLK_PIN, DIO_PIN);

// Obiect pentru gestionarea timpului local
Timezone myTZ;

// Variabile pentru verificarea periodică a Wi-Fi-ului
unsigned long ultimaVerificareWiFi = 0;
const unsigned long intervalVerificare = 5000; 

// Reține starea curentă a luminozității pentru a nu trimite comanda inutil la fiecare buclă
int luminozitateCurenta = -1;

void setup() {
  Serial.begin(115600);
  
  // Inițializare ecran
  display.setBrightness(LUMINA_ZI);
  display.flipDisplay(false); 
  display.showString("WIFI"); 

  // Configurare rețele Wi-Fi
  wifiMulti.addAP("bbk", "internet");
  wifiMulti.addAP("bbk2", "internet2");

  // Conectare inițială
  Serial.print("Conectare la Wi-Fi...");
  while (wifiMulti.run() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nConectat cu succes la Wi-Fi!");
  display.showString("CONN");
  delay(1000);

  // Sincronizare NTP
  waitForSync();
  
  // Setare fus orar pentru România
  myTZ.setLocation("Europe/Bucharest");
  Serial.println("Oră sincronizată!");
  display.clear();
}

void loop() {
  // 1. Verificare periodică a conexiunii Wi-Fi
  if (millis() - ultimaVerificareWiFi >= intervalVerificare) {
    ultimaVerificareWiFi = millis();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[Wi-Fi] Conexiune pierdută! Reconectare în fundal...");
      wifiMulti.run(); 
    }
  }

  // 2. Menține sincronizarea NTP în fundal
  events();

  // 3. Extragere oră și minute
  int ore = myTZ.dateTime("H").toInt();
  int minute = myTZ.dateTime("i").toInt();
  int timpAfisat = (ore * 100) + minute;

  // 4. GESTIONARE LUMINOZITATE AUTOMATĂ (Noaptea între 22:00 și 07:00)
  int luminozitateTinta;
  if (ore >= 22 || ore < 7) {
    luminozitateTinta = LUMINA_NOAPTE;
  } else {
    luminozitateTinta = LUMINA_ZI;
  }

  // Schimbăm luminozitatea doar dacă s-a modificat pragul orar, ca să evităm pâlpâirea ecranului
  if (luminozitateCurenta != luminozitateTinta) {
    luminozitateCurenta = luminozitateTinta;
    display.setBrightness(luminozitateCurenta);
    Serial.print("[Ecran] Luminozitate schimbată la nivelul: ");
    Serial.println(luminozitateCurenta);
  }

  // 5. Logica pentru clipirea secundelor
  bool puncteActive = (myTZ.dateTime("s").toInt() % 2 == 0);

  if (puncteActive) {
    display.showNumberDec(timpAfisat, 0b01000000, true);
  } else {
    display.showNumberDec(timpAfisat, 0, true);
  }

  delay(100);
}
