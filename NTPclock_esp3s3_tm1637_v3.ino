// ============================================================================
// Ceas NTP pentru ESP32-S3 SuperMini + TM1637
// Autor: Nicu FLORICA (niq_ro)
//
//
// Istoric:
// v0.0  - versiune initiala
// v0.0a - suport 2 retele WiFi
//         verificare conexiune
//         luminozitate automata zi/noapte
//
// v1.0  - integrare Open-Meteo
//
// v3.0  - afisare temperatura
//         afisare umiditate
//         afisare presiune atmosferica (mmHg)
//         afisare P760 pe display
//         daca meteo lipseste afiseaza doar ceasul
//         HTTPS Open-Meteo
//         actualizare meteo la 15 minute
// ============================================================================

#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ezTime.h>

#include <TM1637TinyDisplay.h>

#define CLK_PIN 5
#define DIO_PIN 6

#define LUMINA_ZI     4
#define LUMINA_NOAPTE 1

WiFiMulti wifiMulti;

TM1637TinyDisplay display(CLK_PIN, DIO_PIN);
Timezone myTZ;

unsigned long ultimaVerificareWiFi = 0;
unsigned long ultimaVerificareMeteo = 0;

const unsigned long intervalWiFi = 5000;
const unsigned long intervalMeteo = 900000UL;  // 15 minute

int luminozitateCurenta = -1;

float temperatura = 0.0;
int umiditate = 0;

float presiune_hpa = 0.0;
int presiune_mmhg = 0;

bool dateMeteoValide = false;


// ============================================================================

void actualizeazaMeteo();

// ============================================================================

void setup()
{
  Serial.begin(115200);

  display.setBrightness(LUMINA_ZI);
  display.flipDisplay(false);

  display.showString("WIFI");

  wifiMulti.addAP("bbk", "internet");
  wifiMulti.addAP("bbk2", "internet2");

  Serial.println();
  Serial.println("Conectare WiFi...");

  while (wifiMulti.run() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi conectat");

  display.showString("CONN");
  delay(1000);

  waitForSync();

  myTZ.setLocation("Europe/Bucharest");

  actualizeazaMeteo();

  display.clear();
}

// ============================================================================

void loop()
{
  events();

  if (millis() - ultimaVerificareWiFi >= intervalWiFi)
  {
    ultimaVerificareWiFi = millis();

    if (WiFi.status() != WL_CONNECTED)
    {
      wifiMulti.run();
    }
  }

  if (millis() - ultimaVerificareMeteo >= intervalMeteo)
  {
    actualizeazaMeteo();
  }

  int ore = myTZ.dateTime("H").toInt();
  int minute = myTZ.dateTime("i").toInt();
  int secunde = myTZ.dateTime("s").toInt();

  int timpAfisat = ore * 100 + minute;

  int luminozitateTinta =
      ((ore >= 21) || (ore < 7))
          ? LUMINA_NOAPTE
          : LUMINA_ZI;

  if (luminozitateCurenta != luminozitateTinta)
  {
    luminozitateCurenta = luminozitateTinta;
    display.setBrightness(luminozitateCurenta);
  }

  // ===========================================================
  // Daca nu avem meteo => afisam doar ceasul
  // ===========================================================

  if (!dateMeteoValide)
  {
    bool puncteActive = (secunde % 2 == 0);

    display.showNumberDec(
        timpAfisat,
        puncteActive ? 0b01000000 : 0,
        true);

    delay(200);
    return;
  }

  // ===========================================================
  // Ciclu 24 secunde
  // 0..14  = ceas
  // 15..17 = temperatura
  // 18..20 = umiditate
  // 21..23 = presiune
  // ===========================================================

  int restSecunde = secunde % 24;

  if (restSecunde < 15)
  {
    bool puncteActive = (secunde % 2 == 0);

    display.showNumberDec(
        timpAfisat,
        puncteActive ? 0b01000000 : 0,
        true);
  }

  else if (restSecunde < 18)
  {
    int tempInt = (int)round(temperatura);

    display.showNumberDec(tempInt, 0, false, 2, 0);

    uint8_t segmenteTemp[] =
    {
      SEG_A | SEG_B | SEG_F | SEG_G,  // °
      SEG_A | SEG_D | SEG_E | SEG_F   // C
    };

    display.setSegments(segmenteTemp, 2, 2);
  }

  else if (restSecunde < 21)
  {
    display.showNumberDec(umiditate, 0, false, 2, 0);

    uint8_t segmenteUmiditate[] =
    {
      SEG_A | SEG_B | SEG_F | SEG_G,
      SEG_C | SEG_D | SEG_E | SEG_G
    };

    display.setSegments(segmenteUmiditate, 2, 2);
  }

  else
  {
    uint8_t literaP[] =
    {
      SEG_A | SEG_B | SEG_E | SEG_F | SEG_G
    };

    display.showNumberDec(
        presiune_mmhg,
        0,
        false,
        3,
        1);

    display.setSegments(literaP, 1, 0);
  }

  delay(200);
}

// ============================================================================

void actualizeazaMeteo()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("[METEO] WiFi deconectat");
    return;
  }

  String adresaCompleta =
      "https://api.open-meteo.com/v1/forecast"
      "?latitude=44.3311"
      "&longitude=23.8147"
      "&current=temperature_2m,"
      "relative_humidity_2m,"
      "surface_pressure";

  WiFiClientSecure client;

  client.setInsecure();

  HTTPClient http;

  http.setTimeout(5000);

  if (!http.begin(client, adresaCompleta))
  {
    Serial.println("[METEO] Eroare begin()");
    return;
  }

  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK)
  {
    String payload = http.getString();

    DynamicJsonDocument doc(1536);

    DeserializationError error =
        deserializeJson(doc, payload);

    if (!error)
    {
      temperatura =
          doc["current"]["temperature_2m"];

      umiditate =
          doc["current"]["relative_humidity_2m"];

// TM1637 are doar 4 digiți.
// Dacă Open-Meteo raportează 100%,
// afișăm 99%.
if (umiditate > 99)
{
    umiditate = 99;
}

      presiune_hpa =
          doc["current"]["surface_pressure"];

      presiune_mmhg =
          round(presiune_hpa * 0.75006156)+10.;

      dateMeteoValide = true;

      ultimaVerificareMeteo = millis();

      Serial.println();
      Serial.println("===== METEO =====");

      Serial.printf(
          "Temperatura : %.1f C\n",
          temperatura);

      Serial.printf(
          "Umiditate   : %d %%\n",
          umiditate);

      Serial.printf(
          "Presiune    : %.1f hPa\n",
          presiune_hpa);

      Serial.printf(
          "Presiune    : %d mmHg\n",
          presiune_mmhg);

      Serial.println("=================");
    }
    else
    {
      Serial.println(
          "[METEO] Eroare JSON");

      dateMeteoValide = false;
    }
  }
  else
  {
    Serial.printf(
        "[METEO] HTTP error %d : %s\n",
        httpCode,
        http.errorToString(httpCode).c_str());

    dateMeteoValide = false;
  }

  http.end();
}
