// ceas NTP cu ESP32-S3 Supermini si afisaj din 4 cifre cu TM1637
// implicare: Nicu FLORICA (niq_ro)
// v.0 - versiune initiala

#include <WiFi.h>
#include <ezTime.h>
#include <TM1637TinyDisplay.h>

// Configurare pini afișaj
#define CLK_PIN 5
#define DIO_PIN 6

// Datele rețelei tale Wi-Fi
const char* ssid     = "bbk2"; //"NUME_RETEA_WIFI";
const char* password = "internet2";  //"PAROLA_RETEA_WIFI";

// Inițializare ecran
TM1637TinyDisplay display(CLK_PIN, DIO_PIN);

// Obiect pentru gestionarea timpului local
Timezone myTZ;

void setup() {
  Serial.begin(115600);
  
  // Inițializare ecran (luminozitate de la 0 la 7)
  display.setBrightness(4);
  display.flipDisplay(false); 
  display.showString("WIFI"); 

  // Conectare la Wi-Fi
  Serial.print("Conectare la Wi-Fi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConectat la Wi-Fi!");
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
  // Menține sincronizarea NTP în fundal
  events();

  // Extragere oră și minute
  int ore = myTZ.dateTime("H").toInt();
  int minute = myTZ.dateTime("i").toInt();
  
  // Formatare număr (ex: 14:35 devine 1435)
  int timpAfisat = (ore * 100) + minute;

  // Secundele clipesc: pornit la secundele pare, oprit la cele impare
  bool puncteActive = (myTZ.dateTime("s").toInt() % 2 == 0);

  // Activăm sau dezactivăm cele două puncte (colon) din mijloc
  if (puncteActive) {
    display.showNumberDec(timpAfisat, 0b01000000, true); // 0b01000000 aprinde punctele
  } else {
    display.showNumberDec(timpAfisat, 0, true);          // 0 stinge punctele
  }

  // Pauză scurtă pentru stabilitate
  delay(200);
}  
