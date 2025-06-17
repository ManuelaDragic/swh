// ------
// WordClock mit Kalenderfunktion
// ------

// Benötigte Libraries
#include <Wire.h>
#include <RTClib.h>   
#include <FastLED.h>

#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>

#include <Bounce2.h>

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Arduino.h>

#include "Parola_Fonts_data.h"
#include "WordClockFont.h"

// WLAN
const char* ssid = "welcome";
const char* password = "";

// const char* ssid = "Turbofritz2";
// const char* password = "*************";


// LED Stripe
#define DATA_PIN    D3
#define LED_TYPE    WS2812
#define COLOR_ORDER GRB
#define NUM_LEDS    145  

// Matrizen
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 6

// matrix1
#define M1_CLK_PIN   D5
#define M1_CS_PIN    D6
#define M1_DATA_PIN  D7

// matrix2
#define M2_CLK_PIN   TX 
#define M2_CS_PIN    D4 
#define M2_DATA_PIN  D0

// Buttons 
#define BUTTON_PIN  D8
#define ANALOG_PIN  A0


// RTC object
RTC_DS3231 rtc;

// Button Variablen
// digital
Bounce debouncer = Bounce();        // Bounce-Objekt zum Entprellen des Buttons
// analog
bool buttonBacklightOff = false;
bool buttonMatrizenStatus = true;
bool brightnessDown = false;

bool matrizenStatus = false;
unsigned long matrizenTimer = 0;

// LED's Variablen
int brightnessLEDs;                 // Default Helligkeit 0 bis 255
int activeColorID = 0;  
CRGB color;
CRGB leds[NUM_LEDS];
bool cleared = false;

// Matrizen Variablen
int brightnessMatrix;             // Default Helligkeit 0 bis 15
MD_Parola matrix1 = MD_Parola(HARDWARE_TYPE, M1_DATA_PIN, M1_CLK_PIN, M1_CS_PIN, MAX_DEVICES);
MD_Parola matrix2 = MD_Parola(HARDWARE_TYPE, M2_DATA_PIN, M2_CLK_PIN, M2_CS_PIN, MAX_DEVICES);
bool matrix1Fertig = false;
bool matrix2Fertig = false;
uint8_t i = 0;

// Kalender Daten Variablen
String payload = "";
int StringCount = 0;
String strs[30];
char events[60][60];
int eventcounter = 0;
int speed = 0;

void setup() {
  
  // ----- LED Strip -------
  //
  Serial.begin(74880);
  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS);
  Wire.begin();

  // ----- RTC Modul -------
  //
  rtc.begin();

  // Falls das DS3231-Modul nicht korrekt erkannt wird
  if (!rtc.begin()) {
    Serial.println("Konnte RTC nicht finden!");
    while (1);
  }

  // Falls das DS3231-Modul keine Stromversorgung hatte und dadurch die Zeit verloren hat, kann die Zeit hiermit festgelegt werden
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  // Oder die genaue Zeit kann hier im Format (JJJJ, MM, TT, hh, mm, ss) angegeben werden.
  // Wenn genau zu dieser Zeit reset am ESP gedrückt wird, so wird die Uhr genau auf diesen anfangswert gestellt
  // rtc.adjust(DateTime(2023, 6, 25, 11, 59, 0));

  // Helligkeit
  brightnessLEDs = 180;                                   // LEDS's default auf 180
  color = CRGB(brightnessLEDs, 0, 0);                     // Matrizen default auf rot

  showTime(); 

  // ----- Matrizen -------
  //
  matrix1.begin();
  matrix2.begin();

  // WordClockFont setzen (NUR GROßBUCHSTABEN)
  matrix1.setFont(WordClockFont);
  matrix2.setFont(WordClockFont);

  // Standardfont:
  // matrix1.setFont(NULL);
  // matrix2.setFont(NULL);

  speed = 6 * matrix1.getSpeed(); 


  // ----- Button -------
  //
  // Debouncer für Farbwechsel
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  debouncer.attach(BUTTON_PIN);
  debouncer.interval(50); // Entprellungsintervall in Millisekunden


  // ----- Kalender Daten -------
  //
  // Warte auf Verbindung
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Verbindung zum WLAN herstellen...");
  }
  // Verbunden
  Serial.println("Verbunden mit WLAN");
  Serial.println("IP-Adresse: ");
  Serial.println(WiFi.localIP());

  getKalenderData();
}


void getKalenderData() {

  // HTTP-Anfrage senden
  WiFiClientSecure client;
  HTTPClient http;
  client.setInsecure();
  client.connect("script.googleusercontent.com", 443);
  String url = "https://script.googleusercontent.com/macros/echo?user_content_key=UVD2c0fU4HIhlTXUbRy2SPD-POldnF4WqYBk-lC4QJ-DxAsUHTDRI-_ln3QRAYbp3X0m2KhzVZ7MtOAKDfAUeai5pGDHS3I5m5_BxDlH2jW0nuo2oDemN9CCS2h10ox_1xSncGQajx_ryfhECjZEnKA7A_lHrrX9Az2gE-Gb46mWXrX2xUXuwSIQtVGKvIX0ziVJ0SibTZqXWknVA5qw4Y1tPBshKWLn6s576l97UydO-Is66Eda2w&lib=MmSqlZGkjUZzizKVJNOp1s4cy7qauPi9E";
  http.setTimeout(20000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.begin(client, url); 
  int httpCode = http.GET();
  Serial.println(httpCode);

  // Daten in payload String speichern
  if (httpCode == 200) {
    Serial.println("Getting calendar");
    payload = http.getString();
  }

  // String in Substrings splitten
  while (payload.length() > 0)
  {
    // an jede Zeile trennen und als Substring speichern
    int index = payload.indexOf('\n');

    if (index == -1) 
    {
      strs[StringCount] = payload;
      strs[StringCount].toUpperCase();
      strs[StringCount].toCharArray(events[StringCount], 60);
      Serial.println(strs[StringCount]);
      Serial.println(events[StringCount]);
      StringCount++;
      break;
    }

    else
    {
      strs[StringCount] = payload.substring(0, index);
      strs[StringCount].toUpperCase();
      strs[StringCount].toCharArray(events[StringCount], 60);
      Serial.println(strs[StringCount]);
      Serial.println(events[StringCount]);
      StringCount++;
      payload = payload.substring(index+1);
    }
  }
  
  http.end();
}


// passt Nachts die Helligkeit an
void adjustBrightness(){

  // Wenn es dunkel wird Helligkeit runter
  if (brightnessDown) {
    brightnessLEDs = 20;
    brightnessMatrix = 0;
  }

  // Wenn es hell wird Helligkeit hoch 
  else {
    brightnessLEDs = 150;
    brightnessMatrix = 7;
  }

  switch (activeColorID) {
    case 0:
      color = CRGB(brightnessLEDs, 0, 0);                             // rot
      break;
    case 1:
      color = CRGB(0, brightnessLEDs, 0);                             // grün
      break;
    case 2:
      color = CRGB(0, 0, brightnessLEDs);                             // blau
      break;
    case 3:
      color = CRGB(brightnessLEDs, brightnessLEDs, 0);                // gelb
      break;
    case 4:
      color = CRGB(0, brightnessLEDs, brightnessLEDs);                // türkis
      break;
    case 5:
      color = CRGB(brightnessLEDs, 0, brightnessLEDs);                // lila
      break;
    case 6:
      color = CRGB(brightnessLEDs, brightnessLEDs, brightnessLEDs);   // weiß
      break;
  }

}


// wechselt nach Knopfdruck die LED-Farbe der Word-Clock
void changeColor() {
  
  activeColorID = (activeColorID + 1) % 7; 
  
  switch (activeColorID) {
    case 0:
      color = CRGB(brightnessLEDs, 0, 0);                             // rot
      break;
    case 1:
      color = CRGB(0, brightnessLEDs, 0);                             // grün
      break;
    case 2:
      color = CRGB(0, 0, brightnessLEDs);                             // blau
      break;
    case 3:
      color = CRGB(brightnessLEDs, brightnessLEDs, 0);                // gelb
      break;
    case 4:
      color = CRGB(0, brightnessLEDs, brightnessLEDs);                // türkis
      break;
    case 5:
      color = CRGB(brightnessLEDs, 0, brightnessLEDs);                // lila
      break;
    case 6:
      color = CRGB(brightnessLEDs, brightnessLEDs, brightnessLEDs);   // weiß
      break;
  }

  FastLED.show();
}


// schaltet das Ambient light ein 
void backLight(){

  if (buttonBacklightOff == false) {
    leds[0] = color;
    leds[12] = color;
    leds[24] = color;
    leds[36] = color;
    leds[48] = color;
    leds[60] = color;
    leds[72] = color;
    leds[84] = color;
    leds[96] = color;
    leds[108] = color;
    leds[120] = color;
    leds[132] = color;
    leds[144] = color;
    FastLED.show();
  }
  else {
    leds[0] = CRGB::Black;
    leds[12] = CRGB::Black;
    leds[24] = CRGB::Black;
    leds[36] = CRGB::Black;
    leds[48] = CRGB::Black;
    leds[60] = CRGB::Black;
    leds[72] = CRGB::Black;
    leds[84] = CRGB::Black;
    leds[96] = CRGB::Black;
    leds[108] = CRGB::Black;
    leds[120] = CRGB::Black;
    leds[132] = CRGB::Black;
    leds[144] = CRGB::Black;
    FastLED.show();
  }

}

// zeigt die Uhrzeit an
void showTime() {

  DateTime jetzt = rtc.now();
  
  int Jahr = jetzt.year();
  int Monat = jetzt.month();
  int Tag = jetzt.day();
  int Stunde = jetzt.hour();
  int Minute = jetzt.minute();
  int Sekunde = jetzt.second();
  int Wochentag = jetzt.dayOfTheWeek();
  float Temperatur = rtc.getTemperature();

  // in das passende 12 Stunden Format bringen
  if(Stunde >= 12)
  {
      Stunde -= 12;
  }
  if(Minute >= 25)
  {
      Stunde++;
  }
  if(Stunde == 12)
  {
      Stunde = 0;
  }

  // "Es ist" ausgeben
    leds[1] = color;
    leds[2] = color;
    leds[4] = color;
    leds[5] = color;
    leds[6] = color;

  // "Uhr" anzeigen
  if(Minute < 5)
  {
    Serial.print("UHR ");
    leds[133] = color;
    leds[134] = color;
    leds[135] = color;
  }

  // Wochentag anzeigen
  Serial.print("Wochentag: ");
  Serial.print(Wochentag);
  switch (Wochentag) {
    // Montag
    case 1:
      leds[8] = color;
      leds[9] = color;
      break;
    // Dienstag
    case 2:
      leds[10] = color;
      leds[11] = color;
      break;
    // Mittwoch
    case 3:
      leds[22] = color;
      leds[23] = color;
      break;
    // Donnerstag
    case 4:
      leds[20] = color;
      leds[21] = color;
      break;
    // Freitag
    case 5:
      leds[18] = color;
      leds[19] = color;
      break;
    // Samstag
    case 6:
      leds[16] = color;
      leds[17] = color;
      break;
    // Sonntag
    case 0:
      leds[14] = color;
      leds[15] = color;
      break;
    default:
      Serial.println("Ungültiger Wochentag");
      break;
  }

  // Stunde anzeigen
  switch(Stunde) {
    case 0:
      //Zwölf
      Serial.print("ZWÖLF ");
      leds[127] = color;
      leds[128] = color;
      leds[129] = color;
      leds[130] = color;
      leds[131] = color;
      break;
    case 1:
      //EIN(S)
      Serial.print("EIN");
      leds[93] = color;
      leds[94] = color;
      leds[95] = color;
      // sobald es 5 nach 1 ist muss das s mitleuchten
      if(Minute > 4){
        leds[92] = color;
      }
      break;
    case 2:
      //Zwei
      Serial.print("ZWEI ");
      leds[85] = color;
      leds[86] = color;
      leds[87] = color;
      leds[88] = color;
      break;
    case 3:
      //Drei
      Serial.print("DREI ");
      leds[97] = color;
      leds[98] = color;
      leds[99] = color;
      leds[100] = color;
      break;
    case 4:
      //Vier
      Serial.print("VIER ");
      leds[104] = color;
      leds[105] = color;
      leds[106] = color;
      leds[107] = color;
      break;
    case 5:
      //Fünf
      Serial.print("FÜNF ");
      leds[80] = color;
      leds[81] = color;
      leds[82] = color;
      leds[83] = color;
      break;
    case 6:
      //Sechs
      Serial.print("SECHS ");
      leds[115] = color;
      leds[116] = color;
      leds[117] = color;
      leds[118] = color;
      leds[119] = color;
      break;
    case 7:
      //Sieben
      Serial.print("SIEBEN ");
      leds[121] = color;
      leds[122] = color;
      leds[123] = color;
      leds[124] = color;
      leds[125] = color;
      leds[126] = color;
      break;
    case 8:
      //Acht
      Serial.print("ACHT ");
      leds[109] = color;
      leds[110] = color;
      leds[111] = color;
      leds[112] = color;
      break;
    case 9:
      //Neun
      Serial.print("NEUN ");
      leds[137] = color;
      leds[138] = color;
      leds[139] = color;
      leds[140] = color;
      break;
    case 10:
      //Zehn
      Serial.print("ZEHN ");
      leds[140] = color;
      leds[141] = color;
      leds[142] = color;
      leds[143] = color;
      break;
    case 11:
      //Elf
      Serial.print("ELF ");
      leds[78] = color;
      leds[79] = color;
      leds[80] = color;
      break;
  }

  // Minuten anzeigen
  if(Minute >= 5 && Minute < 10)
  {
      // Fünf
      Serial.print("FÜNF ");
      leds[27] = color;
      leds[28] = color;
      leds[29] = color;
      leds[30] = color;
      // Nach
      Serial.print("NACH ");
      leds[61] = color;
      leds[62] = color;
      leds[63] = color;
      leds[64] = color;

  }
  else if(Minute >= 10 && Minute < 15)
  {
      // Zehn
      Serial.print("ZEHN ");
      leds[44] = color;
      leds[45] = color;
      leds[46] = color;
      leds[47] = color;
      // Nach
      Serial.print("NACH ");
      leds[61] = color;
      leds[62] = color;
      leds[63] = color;
      leds[64] = color;
  }
  else if(Minute >= 15 && Minute < 20)
  {
      // Viertel
      Serial.print("VIERTEL ");
      leds[53] = color;
      leds[54] = color;
      leds[55] = color;
      leds[56] = color;
      leds[57] = color;
      leds[58] = color;
      leds[59] = color;
      // Nach
      Serial.print("NACH ");
      leds[61] = color;
      leds[62] = color;
      leds[63] = color;
      leds[64] = color;
  }
  else if(Minute >= 20 && Minute < 25)
  {
      // Zwanzig
      Serial.print("ZWANZIG ");
      leds[37] = color;
      leds[38] = color;
      leds[39] = color;
      leds[40] = color;
      leds[41] = color;
      leds[42] = color;
      leds[43] = color;
      // Nach
      Serial.print("NACH ");
      leds[61] = color;
      leds[62] = color;
      leds[63] = color;
      leds[64] = color;
  }
  else if(Minute >= 25 && Minute < 30)
  {
      // Fünf
      Serial.print("FÜNF ");
      leds[27] = color;
      leds[28] = color;
      leds[29] = color;
      leds[30] = color;
      // Vor
      Serial.print("VOR ");
      leds[69] = color;
      leds[70] = color;
      leds[71] = color;
      // Halb
      Serial.print("HALB ");
      leds[73] = color;
      leds[74] = color;
      leds[75] = color;
      leds[76] = color;

  }
  else if(Minute >= 30 && Minute < 35)
  {
      // Halb
      Serial.print("HALB ");
      leds[73] = color;
      leds[74] = color;
      leds[75] = color;
      leds[76] = color;

  }
  else if(Minute >= 35 && Minute < 40)
  {
      // Fünf
      Serial.print("FÜNF ");
      leds[27] = color;
      leds[28] = color;
      leds[29] = color;
      leds[30] = color;
      // Nach
      Serial.print("NACH ");
      leds[61] = color;
      leds[62] = color;
      leds[63] = color;
      leds[64] = color;
      // Halb
      Serial.print("HALB ");
      leds[73] = color;
      leds[74] = color;
      leds[75] = color;
      leds[76] = color;

  }
  else if(Minute >= 40 && Minute < 45)
  {
      // Zwanzig
      Serial.print("ZWANZIG ");
      leds[37] = color;
      leds[38] = color;
      leds[39] = color;
      leds[40] = color;
      leds[41] = color;
      leds[42] = color;
      leds[43] = color;
      // Vor
      Serial.print("VOR ");
      leds[69] = color;
      leds[70] = color;
      leds[71] = color;
  }
  else if(Minute >= 45 && Minute < 50)
  {
      // Viertel
      Serial.print("VIERTEL ");
      leds[53] = color;
      leds[54] = color;
      leds[55] = color;
      leds[56] = color;
      leds[57] = color;
      leds[58] = color;
      leds[59] = color;
      // Vor
      Serial.print("VOR ");
      leds[69] = color;
      leds[70] = color;
      leds[71] = color;
  }
  else if(Minute >= 50 && Minute < 55)
  {
      // Zehn
      Serial.print("ZEHN ");
      leds[44] = color;
      leds[45] = color;
      leds[46] = color;
      leds[47] = color;
      // Vor
      Serial.print("VOR ");
      leds[69] = color;
      leds[70] = color;
      leds[71] = color;
  
  }
  else if(Minute >= 55 && Minute < 60)
  {
      // Fünf
      Serial.print("FÜNF ");
      leds[27] = color;
      leds[28] = color;
      leds[29] = color;
      leds[30] = color;
      // Vor
      Serial.print("VOR ");
      leds[69] = color;
      leds[70] = color;
      leds[71] = color;
  }

  // Innentemperatur ausgeben
  Serial.print("Temperatur: ");
  Serial.print(Temperatur);
  Serial.println(" C");
  Serial.print(" ");

  // Datum ausgeben
  Serial.print("Datum: ");
  Serial.print(Tag);
  Serial.print(".");
  Serial.print(Monat);
  Serial.print(".");
  Serial.print(Jahr);
  Serial.print(" ");
  
  FastLED.show();
  
}


void runMatrix() {
  
  // Display Text anzeigen
  matrix1.setIntensity(brightnessMatrix); 
  matrix2.setIntensity(brightnessMatrix);

  // Wenn matrix1 fertig auf true setzen
  if (matrix1.displayAnimate()) {
    matrix1Fertig = true;
  }

  // Wenn matrix2 fertig auf true setzen
  if (matrix2.displayAnimate()) {
    matrix2Fertig = true;
  }

  // Wenn beide fertig sind, neuen Text anzeigen
  if (matrix1Fertig && matrix2Fertig) {
    
    // (Zum anpassen: erster Wert: Text, zweiter Wert = Start Position, Dritter Wert = Geschwindigkeit, Vierter Wert = Pause am Ende, Fünfter Wert = Animation Rein, Sechster Wert = Animation Raus)
    // Positionen: PA_LEFT, PA_RIGHT, PA_CENTER
    // Animationsliste: PA_PRINT, PA_SCROLL_UP, PA_SCROLL_DOWN, PA_SCROLL_LEFT, PA_SCROLL_RIGHT, PA_SCROLL_UP_LEFT, PA_SCROLL_UP_RIGHT, PA_SCROLL_DOWN_LEFT, PA_SCROLL_DOWN_RIGHT,
    //                  PA_SPRITE, PA_SLICE, PA_MESH, PA_FADE, PA_DISSOLVE, PA_BLINDS, PA_RANDOM, PA_WIPE, PA_WIPE_CURSOR, 
    //                  PA_SCAN_HORIZ, PA_SCAN_HORIZX, PA_SCAN_VERT, PA_SCAN_VERTX, PA_OPENING, PA_OPENING_CURSOR, PA_CLOSING, PA_CLOSING_CURSOR, PA_GROW_UP, PA_GROW_DOWN 
    matrix1.displayText (events[i], PA_LEFT, speed, 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
    //  set up for next text
    i++; 

    if (i == StringCount) {
      i = 0;
    } 

    matrix2.displayText (events[i], PA_LEFT, speed, 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
      
    matrix1Fertig = false;
    matrix2Fertig = false;

    delay (1000);
  }
}


void loop() {

  showTime();
  DateTime now = rtc.now();
  // alle 5 Minuten Uhrzeit updaten
  if ((now.minute() == 0 || now.minute() == 5 || now.minute() == 10 || now.minute() == 15 || now.minute() == 20 || now.minute() == 25 || now.minute() == 30 || 
  now.minute() == 35 || now.minute() == 40 || now.minute() == 45 || now.minute() == 50 || now.minute() == 55) && now.second() == 0) {
    if (cleared == false) {
      FastLED.clear();
      cleared = true; 
    }
  }
  else {
    cleared = false; 
  }

  backLight();
  adjustBrightness();

  // ------ Farbwechsel Button ------
  //
  debouncer.update();       // Entprellten Wert aktualisieren
  if (debouncer.rose()) {
    changeColor();
  }

  // ------ Helligkeit anpassen ------
  //
  // Licht dunkler 
  if (analogRead(ANALOG_PIN) < 300) {
    brightnessDown = true;
  }

  // Licht heller
  if (analogRead(ANALOG_PIN) < 500 && analogRead(ANALOG_PIN) > 300) {
    brightnessDown = false;
  }


  // ------ Matrizen Button ------
  //
  // AN/AUS
  if(buttonMatrizenStatus) {
    runMatrix();
  }

  if (!matrizenStatus) {
    if (analogRead(ANALOG_PIN) > 1000) {
      buttonMatrizenStatus = !buttonMatrizenStatus;
      if(!buttonMatrizenStatus) {
        matrix1.displayClear();
        matrix2.displayClear();
        matrix1.displayReset();
        matrix2.displayReset();
        i = 0;
      }
      matrizenStatus = true;
      matrizenTimer = millis();
    }

    // ------ Backlight Button ------
    //
    // AN/AUS
    else if (analogRead(ANALOG_PIN) > 600) {
      buttonBacklightOff = !buttonBacklightOff;
      matrizenStatus = true;
      matrizenTimer = millis();
    }

  } 

  // nur auf einen Druck reagieren
  else {
    if(analogRead(ANALOG_PIN) < 500 && ((millis() - matrizenTimer) > 500)) {
      matrizenStatus = false;
    }
  }


  // Kalenderdaten um 2 und 12 Uhr aktualisieren
  if (now.hour() == 1 && now.minute() == 0 && now.second() == 0 || now.hour() == 12 && now.minute() == 0 && now.second() == 0) {
    getKalenderData();
  }
  
}


