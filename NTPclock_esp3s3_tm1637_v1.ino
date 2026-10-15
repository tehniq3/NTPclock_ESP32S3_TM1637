// ceas NTP cu ESP32-S3 Supermini si afisaj din 4 cifre cu TM1637
// implicare: Nicu FLORICA (niq_ro), munca: Gemini, teste rele: niq_ro
// v.0 - versiune initiala
// v.0a - conectare la 2 retele wifi, schimbare intensitatea luminasa zi-noapte, verificare conectare la retea
// v.1 - adaugare informatii de la open-meteo

#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ezTime.h>
#include <TM1637TinyDisplay.h>

// Configurare pini afișaj
#define CLK_PIN 5
#define DIO_PIN 6

// Niveluri de luminozitate
#define LUMINA_ZI 4   
#define LUMINA_NOAPTE 1 

// Inițializare obiecte
WiFiMulti wifiMulti;
TM1637TinyDisplay display(CLK_PIN, DIO_PIN);
Timezone myTZ;

// URL-ul Open-Meteo API pentru Craiova (Preluare temperatură și umiditate curentă)
const String openMeteoUrl = "http://open-meteo.com";

// Variabile pentru controlul timpului (non-blocant)
unsigned long ultimaVerificareWiFi = 0;
unsigned long ultimaVerificareMeteo = 0;
const unsigned long intervalWiFi = 5000;
const unsigned long intervalMeteo = 900000; // Actualizare meteo la fiecare 15 minute

int luminozitateCurenta = -1;
float temperatura = 0.0;
int umiditate = 0;
bool dateMeteoValide = false;

void setup() {
  Serial.begin(115600);
  
  display.setBrightness(LUMINA_ZI);
  display.flipDisplay(false); 
  display.showString("WIFI"); 

  // Configurare rețele Wi-Fi
  wifiMulti.addAP("bbk", "internet");
  wifiMulti.addAP("bbk2", "internet2");

  Serial.print("Conectare la Wi-Fi...");
  while (wifiMulti.run() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nConectat la Wi-Fi!");
  display.showString("CONN");
  delay(1000);

  waitForSync();
  myTZ.setLocation("Europe/Bucharest");
  
  // Preluare inițială date meteo
  actualizeazaMeteo();
  display.clear();
}

void loop() {
  events(); // Menține NTP activ

  // 1. Verificare conexiune Wi-Fi la 5 secunde
  if (millis() - ultimaVerificareWiFi >= intervalWiFi) {
    ultimaVerificareWiFi = millis();
    if (WiFi.status() != WL_CONNECTED) {
      wifiMulti.run(); 
    }
  }

  // 2. Verificare Meteo la 15 minute
  if (millis() - ultimaVerificareMeteo >= intervalMeteo) {
    actualizeazaMeteo();
  }

  // 3. Extragere timp curent
  int ore = myTZ.dateTime("H").toInt();
  int minute = myTZ.dateTime("i").toInt();
  int secunde = myTZ.dateTime("s").toInt();
  int timpAfisat = (ore * 100) + minute;

  // 4. Setare automată luminozitate (Zi / Noapte)
  int luminozitateTinta = (ore >= 21 || ore < 7) ? LUMINA_NOAPTE : LUMINA_ZI;
  if (luminozitateCurenta != luminozitateTinta) {
    luminozitateCurenta = luminozitateTinta;
    display.setBrightness(luminozitateCurenta);
  }

  // 5. SECVENȚA DE AFIȘARE DINAMICĂ (Ciclu de 21 de secunde)
  int restSecunde = secunde % 21;

  if (restSecunde < 15) {
    // Primele 15 secunde: Afișează CEASUL cu punctele clipind
    bool puncteActive = (secunde % 2 == 0);
    display.showNumberDec(timpAfisat, puncteActive ? 0b01000000 : 0, true);
  } 
  else if (restSecunde >= 15 && restSecunde < 18) {
    // Următoarele 3 secunde: Afișează TEMPERATURA (ex: "23*C")
    if (dateMeteoValide) {
      int tempInt = (int)round(temperatura);
      display.showNumberDec(tempInt, 0, false, 2, 0); // Pune valoarea pe primii doi digiți
      
      // Construim manual simbolul de grad și litera 'C' pe digiții 3 și 4
      uint8_t segmenteTemp[] = { 
        SEG_A | SEG_B | SEG_F | SEG_G, // Simbolul de grad (°)
        SEG_A | SEG_D | SEG_E | SEG_F  // Litera C
      };
      display.setSegments(segmenteTemp, 2, 2);
    } else {
      display.showString("TE--");
    }
  } 
  else {
    // Ultimele 3 secunde: Afișează UMIDITATEA (ex: "65*1" ca să imite procente)
    if (dateMeteoValide) {
      display.showNumberDec(umiditate, 0, false, 2, 0); // Valoarea pe primii doi digiți
      
      // Simulare procent în 7 segmente folosind două cercuri mici succesive (*1 / °o)
      uint8_t segmenteUmiditate[] = { 
        SEG_A | SEG_B | SEG_F | SEG_G, // Cerc mic sus (°)
        SEG_C | SEG_D | SEG_E | SEG_G  // Cerc mic jos (o)
      };
      display.setSegments(segmenteUmiditate, 2, 2);
    } else {
      display.showString("HM--");
    }
  }

  delay(200); // Stabilitate buclă
}
j
// Funcție actualizată cu HTTPS și WiFiClientSecure pentru eliminarea erorii -1
void actualizeazaMeteo() {
  if (WiFi.status() == WL_CONNECTED) {
    
    // Coordonate GPS Craiova
    String latitudinea = "44.3311";
    String longitudinea = "23.8147";
    
    // MODIFICARE 1: Folosim "https://" securizat în loc de "http://"
    String adresaCompleta = "https://api.open-meteo.com/v1/forecast?latitude=" + latitudinea + 
                            "&longitude=" + longitudinea + 
                            "&current=temperature_2m,relative_humidity_2m";

    // MODIFICARE 2: Folosim WiFiClientSecure pentru conexiuni SSL (HTTPS)
    WiFiClientSecure client;
    
    // Îi spunem plăcuței să nu verifice certificatul SSL public (evită blocarea memoriei)
    client.setInsecure(); 

    HTTPClient http;
    
    // Inițializăm conexiunea HTTPS folosind clientul securizat
    http.begin(client, adresaCompleta); 
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      
      // Alocare memorie pentru ArduinoJson v6
      DynamicJsonDocument doc(1024); 
      DeserializationError error = deserializeJson(doc, payload);

      if (!error) {
        temperatura = doc["current"]["temperature_2m"];
        umiditate = doc["current"]["relative_humidity_2m"];
        dateMeteoValide = true;
        ultimaVerificareMeteo = millis();
        Serial.printf("[Meteo Craiova] Temp: %.1f C, Umiditate: %d%%\n", temperatura, umiditate);
      } else {
        Serial.println("[Meteo] Eroare la procesarea JSON-ului.");
        dateMeteoValide = false;
      }
    } else {
      // Afișează eroarea exactă dacă mai apare vreo problemă
      Serial.printf("[Meteo] Eroare HTTP: %s (Cod: %d)\n", http.errorToString(httpCode).c_str(), httpCode);
      dateMeteoValide = false;
    }
    http.end();
  } else {
    Serial.println("[Meteo] Nu s-a putut apela API-ul deoarece Wi-Fi este deconectat.");
  }
}
