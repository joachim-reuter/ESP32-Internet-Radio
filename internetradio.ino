/* Version 3.0 – ESP32 Mini Internet Radio
   Mini Display, Akku mit Ladefunktion
   Batterie Anzeige Füllstand auf Display
   Senderliste und WiFi über LittleFS
   Eigener Access Point mit WEB- Seite
   Ändern WiFi auf WEB- Seite
   Ändern Senderliste auf den WEB- Seite
*/

/* ----------------------------------------------------------------------------
//  user_setup.h  (C:\Users\joach\Documents\Arduino\libraries\TFT_eSPI)
// =====================================================
// =====================================================
//  ESP32 + ST7789 2.25" SPI DISPLAY SETUP
//  für TFT_eSPI Library
// =====================================================

// ---------- DISPLAY DRIVER ----------
#define ST7789_DRIVER

// ---------- DISPLAY SIZE ----------
#define TFT_WIDTH  240
#define TFT_HEIGHT 320   // ggf. drehen mit setRotation()

// =====================================================
// SPI PIN CONFIG
// =====================================================

#define TFT_MISO -1
#define TFT_MOSI 23
#define TFT_SCLK 18

// ⚠️ WICHTIG: GPIO15 entfernt (Boot-Strap Problem!)
#define TFT_CS   5       // stabiler Ersatz für GPIO15
#define TFT_DC   2
#define TFT_RST  4

// =====================================================
// OPTIONAL: Backlight (falls vorhanden)
// =====================================================
// #define TFT_BL 13
// #define TFT_BACKLIGHT_ON HIGH

// =====================================================
// SPI SPEED
// =====================================================

#define SPI_FREQUENCY       40000000
#define SPI_READ_FREQUENCY   20000000
#define SPI_TOUCH_FREQUENCY  2500000

// =====================================================
// FONTS
// =====================================================

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF

#define SMOOTH_FONT

// =====================================================
// OPTIONAL FEATURES
// =====================================================

// Wenn dein ST7789 Bild verschoben ist (häufig!)
// dann UNBEDINGT aktivieren:
#define CGRAM_OFFSET

// Optional: Farbtiefe (Standard reicht)
#define COLOR_DEPTH 16


-------------------------------------------------------------------------------- */

/*  Board: ESP32 Base V 2.0.14
    Audio-Library: ESP8266Audio 1.9.7
*/

/*      ┌─────────────────────────┐
        │        ESP32 DEVKIT     │
        │                         │
        │ GPIO18 ──────► TFT_SCK  SCL │
        │ GPIO23 ──────► TFT_MOSI SDA │
        │ GPIO5 ──────► TFT_CS   │
        │ GPIO2  ──────► TFT_DC   │
        │ GPIO4  ──────► TFT_RST  │
        │ GPIO25 ──────► MAX_LRC  │
        │ GPIO26 ──────► MAX_BCLK │
        │ GPIO22 ──────► MAX_DIN  │
        │ GPIO17 ──────► ENC_A CLK│ 
        │ GPIO16 ──────► ENC_B DT │ 
        │ GPIO27 ──────► ENC_BTN SW │
        │ 3V3 ─────────► TFT_VCC / ENC_VCC / MAX_VIN │
        │ GND ─────────► alle GND                    │
        └─────────────────────────┘
*/




#include <WiFi.h>
#include <WebServer.h>
#include <TFT_eSPI.h>
#include "AudioFileSourceICYStream.h"
#include "AudioFileSourceBuffer.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"
#include <SPI.h>
#include <FS.h>
#include <LittleFS.h>

// Akku Anzeige
#define BATTERY_PIN 34

WebServer server(80);

// ---------------- WIFI ----------------
/*
const char* ssid = "FRITZ!Box 7590 FI";
const char* pass = "57426211004645725795";
*/
String wifiSSID = "";
String wifiPASS = "";

#define MAX_STATIONS 20



// ---------------- STATIONS ----------------
struct Station {
  const char* name;
  const char* url;
};

Station stations[MAX_STATIONS] = {
  {"Radio MDR",       "http://mdr-284290-2.sslcast.mdr.de/mdr/284290/2/mp3/high/stream.mp3"},
  {"Radio SAW",       "http://stream.radiosaw.de/saw-anhalt-wittenberg/mp3-192/"},
  {"MDR Jump",        "http://mdr-284320-0.cast.mdr.de/mdr/284320/0/mp3/high/stream.mp3"},
  {"MDR SPUTNIK",     "http://mdr-284330-0.cast.mdr.de/mdr/284330/0/mp3/high/stream.mp3"},
  {"MDR KLASSIK",     "http://mdr-284350-0.cast.mdr.de/mdr/284350/0/mp3/high/stream.mp3"},
  {"R.SA",            "http://streams.rsa-sachsen.de/rsa-oldies/mp3-192/mediaplayerrsa"},
  {"Deutschlandfunk", "http://st01.dlf.de/dlf/01/128/mp3/stream.mp3"}
};

int stationCount = 0;
int currentStation = 0;

TFT_eSPI tft = TFT_eSPI();
AudioGeneratorMP3 *mp3 = nullptr;
AudioFileSourceICYStream *file = nullptr;
AudioFileSourceBuffer *buff = nullptr;
AudioOutputI2S *out = nullptr;

// ---------------- ENCODER ----------------
// YOUR ORIGINAL HARDWARE PINS
const int ENC_A = 17;
const int ENC_B = 16;
const int ENC_BTN = 27;

int lastA = HIGH;
int lastButton = HIGH;

//==== UI STATE ====
float volume = 0.5;   
String currentTitle = "Start...";
String currentStationName = "";
volatile bool titleDirty = false;
volatile bool stationDirty = false;

int scrollX = 240;
unsigned long lastScroll = 0;

unsigned long lastButtonTime = 0;
unsigned long lastEncTime = 0;

const int ENC_DEBOUNCE = 2;
const float VOL_STEP = 0.02;




// ---------------- WIFI SETUP ----------------
void setupWiFi() {
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(20, 85);
  tft.println("Connecting WiFi...");


  // WiFi.begin(ssid, pass);
  
  WiFi.begin(
    wifiSSID.c_str(),
    wifiPASS.c_str()
  );



  int tries = 0;

  while (WiFi.status() != WL_CONNECTED && tries++ < 40) {
    delay(250);
    tft.print(".");
  }
  
  tft.fillScreen(TFT_BLACK);
  if (WiFi.status() == WL_CONNECTED) tft.println("WiFi OK");
  else tft.println("WiFi FAILED");
}

// ---------------- AUDIO START ----------------
void startStation(int index)
{
  if (mp3) { mp3->stop(); delete mp3; mp3 = nullptr; }
  if (buff) { delete buff; buff = nullptr; }
  if (file) { delete file; file = nullptr; }

  file = new AudioFileSourceICYStream(stations[index].url);
  file->RegisterMetadataCB(MDCallback, nullptr);

  buff = new AudioFileSourceBuffer(file, 16384);

  mp3 = new AudioGeneratorMP3();

  out->SetGain(volume);
  mp3->begin(buff, out);

  currentStationName = stations[index].name;
  stationDirty = true;
}

// ---------------- ENCODER HANDLER ----------------
void handleEncoder() {
  int A = digitalRead(ENC_A);
  if (A != lastA && A == LOW) {

    if (millis() - lastEncTime > ENC_DEBOUNCE) {
      if (digitalRead(ENC_B) == HIGH) volume += VOL_STEP;
      else volume -= VOL_STEP;

      if (volume < 0.0) volume = 0.0;
      if (volume > 1.0) volume = 1.0;

      out->SetGain(volume);
      drawWifi();
      drawVolumeBar();

      lastEncTime = millis();
    }
  }
  lastA = A;
}

// ---------------- BUTTON ----------------
void handleButton() {
  int btn = digitalRead(ENC_BTN);

  if (btn == LOW && lastButton == HIGH && millis() - lastButtonTime > 300) {
    lastButtonTime = millis();
    currentStation = (currentStation + 1) % stationCount;
    startStation(currentStation);
  }
  lastButton = btn;
}


// ---- Save Metadata Callback
void MDCallback(void *cbData,
                const char *type,
                bool isUnicode,
                const char *str)
{
  if (!str) return;

  if (strcmp(type, "StreamTitle") == 0)
  {
    currentTitle = String(str);

    if (currentTitle.length() < 2)
      currentTitle = "Keine Titelinfo";

    titleDirty = true;   // ❗ NUR FLAG
  }
}

//  ---- UI Engine ----
void handleUI()
{
  if (stationDirty)
  {
    drawHeader();
    stationDirty = false;
  }

  if (titleDirty)
  {
    drawTitle(currentTitle);
    titleDirty = false;
  }

  
  drawWifi();
  //drawBattery();

}

// ---- draw Header ----
void drawHeader()
{
  tft.fillRect(20, 85, 280, 20, TFT_RED); // TFT_NAVY

  tft.setTextColor(TFT_WHITE, TFT_RED); // TFT_NAVY
  tft.setCursor(25, 90);
  tft.print(currentStationName);

  tft.setCursor(125,90);
  tft.print(wifiSSID.c_str());

}


// --- Draw Title ----
void drawTitle(String txt)
{
  tft.fillRect(20, 110, 280, 30, TFT_BLACK);

  int pos = txt.indexOf(" - ");

  if (pos > 0)
  {
    String artist = txt.substring(0, pos);
    String title  = txt.substring(pos + 3);

    tft.setTextColor(TFT_CYAN);
    tft.setCursor(25, 115);
    tft.print(artist);

    tft.setTextColor(TFT_WHITE);
    tft.setCursor(25, 125);
    tft.print(title);
  }
  else
  {
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(25, 120);
    tft.print(txt);
  }
  
}


// ----  Status Bar ----
void drawVolumeBar()
{
  tft.fillRect(20, 135, 200, 20, TFT_NAVY);  // TFT_BLACK // 280

  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);

  tft.setCursor(25, 140);
  tft.printf("VOL %d%%", int(volume * 100));
  
  tft.drawRect(100, 140, 100, 10, TFT_WHITE); // 160

  tft.fillRect(
      100,
      140,
      volume * 100,   // 160
      10,
      TFT_MAGENTA
  ); 
}

// --- Wifi Status ---
void drawWifi()
{
  int rssi = WiFi.RSSI();

  int x = 262;
  int y = 100;

  tft.setTextColor(TFT_WHITE, TFT_RED); // TFT_NAVY
  tft.setTextSize(1);
  tft.setCursor(240, 90);
  tft.printf("%d", rssi);

  int bars = 1;

  if (rssi > -80) bars = 2;
  if (rssi > -70) bars = 3;
  if (rssi > -60) bars = 4;
  
  for(int i = 0; i < bars; i++)
  {
    tft.fillRect(
      x + i * 8,
      y - (i + 1) * 4,
      5,
      (i + 1) * 4,
      TFT_GREEN   
    );
  } 
}

//--- File System starten ----
void initFS()
{
  if (!LittleFS.begin(true))
  {
    Serial.println("LittleFS Mount fehlgeschlagen");
    return;
  }
  Serial.println("LittleFS OK");
}


// --- LittleFS Loader Stationen ----
void loadStations()
{
  fs::File f = LittleFS.open("/stations.txt", "r");
  if (!f)
  {
    Serial.println("stations.txt fehlt -> fallback");
    return;
  }

  stationCount = 0;

  while (f.available() && stationCount < 20)
  {
    String line = f.readStringUntil('\n');
    line.trim();

    int sep = line.indexOf(';');
    if (sep <= 0) continue;

    String name = line.substring(0, sep);
    String url  = line.substring(sep + 1);

    stations[stationCount].name = strdup(name.c_str());
    stations[stationCount].url  = strdup(url.c_str());

    stationCount++;
  }

  f.close();

  Serial.printf("Stations geladen: %d\n", stationCount);
}

// --- LittleFS Loader WiFi ----
bool loadWiFi()
{
  fs::File f = LittleFS.open("/wifi.txt", "r");

  if (!f)
  {
    Serial.println("wifi.txt fehlt");
    return false;
  }

  while (f.available())
  {
    String line = f.readStringUntil('\n');
    line.trim();

    if (line.startsWith("ssid="))
    {
      wifiSSID = line.substring(5);
    }

    if (line.startsWith("password="))
    {
      wifiPASS = line.substring(9);
    }
  }

  f.close();

  Serial.println("WLAN geladen");
  Serial.println(wifiSSID);

  return true;
}

// ---- WEB- Seite ---
void handleRoot()
{
  String html;

  html += "<html><head>";
  html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<style>";
  html += "body{font-family:Arial;margin:20px;}";
  html += "a.btn{display:block;padding:12px;margin:10px 0;background:#1976d2;color:white;text-decoration:none;text-align:center;border-radius:5px;}";
  html += "</style>";
  html += "</head><body>";

  html += "<h2>ESP32 Internetradio</h2>";

  html += "<h3>Status</h3>";

  html += "SSID: ";
  html += wifiSSID;
  html += "<br>";

  html += "IP: ";
  html += WiFi.localIP().toString();
  html += "<br>";

  html += "Sender: ";
  html += currentStationName;
  html += "<br><br>";

  html += "<a class='btn' href='/wifi'>WLAN</a>";
  html += "<a class='btn' href='/stations'>Sender</a>";
  html += "<a class='btn' href='/next'>";
  html += "Naechster Sender";
  html += "</a>";

  html += "<hr>";
  html += "<form action='/volume' method='post'>";
  html += "Lautstaerke<br>";
  html += "<input type='range' ";
  html += "min='0' max='100' ";
  html += "name='vol' value='";
  html += String(int(volume * 100));
  html += "'>";
  html += "<br><br>";
  html += "<input type='submit' value='Setzen'>";
  html += "</form>";


  html += "<a class='btn' href='/restart'>Neustart</a>";

  html += "</body></html>";

  server.send(200,"text/html",html);
}

//--- Neustart Seite ---
void handleRestart()
{
  server.send(
    200,
    "text/html",
    "<html><body>"
    "<h2>ESP32 startet neu...</h2>"
    "</body></html>"
  );

  delay(2000);
  ESP.restart();
}

//--- Neuer Sender ---
void handleNext()
{
  currentStation++;

  if(currentStation >= stationCount)
    currentStation = 0;

  startStation(currentStation);

  server.sendHeader("Location","/");
  server.send(302,"text/plain","");
}

// --- Lautstärke ---
void handleVolume()
{
  int v = server.arg("vol").toInt();

  if(v < 0) v = 0;
  if(v > 100) v = 100;

  volume = v / 100.0;

  out->SetGain(volume);

  drawVolumeBar();
  
  server.sendHeader("Location","/");
  server.send(302,"text/plain","");
}

// --- Handle WLAN- Seite ---
void handleWifi()
{
  String html;

  html += "<html><head>";
  html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>ESP32 Radio WLAN</title>";

  html += "</head><body>";

  html += "<h2>WLAN Einstellungen</h2>";

  html += "<form action='/savewifi' method='post'>";

  html += "SSID:<br>";
  html += "<input type='text' name='ssid' value='" + wifiSSID + "'><br><br>";

  html += "Passwort:<br>";
  html += "<input type='text' name='pass' value='" + wifiPASS + "'><br><br>";

  html += "<input type='submit' value='Speichern'>";

  html += "</form>";

  html += "<br><a href='/'>Zurueck</a>";

  html += "</body></html>";

  server.send(200, "text/html", html);
}

// --- Hadle WLAN Speichern ---
void handleSaveWifi()
{
  String newSSID = server.arg("ssid");
  String newPASS = server.arg("pass");

  fs::File f = LittleFS.open("/wifi.txt", "w");

  if (f)
  {
    f.println("ssid=" + newSSID);
    f.println("password=" + newPASS);
    f.close();

    wifiSSID = newSSID;
    wifiPASS = newPASS;
  }

  String html;

  html += "<html><body>";
  html += "<h2>Gespeichert</h2>";
  html += "<p>ESP32 wird neu gestartet...</p>";
  html += "</body></html>";

  server.send(200, "text/html", html);

  delay(2000);
  ESP.restart();
}

// --- Handle Station- Seite ---
void handleStations()
{
  String html;

  html += "<style>";
  html += "body{font-family:Arial;margin:20px;}";
  html += "input[type=text]{width:100%;padding:8px;}";
  html += "input[type=submit]{padding:12px 20px;}";
  html += "</style>";

  html += "<html><body>";
  html += "<h2>Senderliste</h2>";

  html += "<form action='/savestations' method='post'>";

  for(int i=0;i<stationCount;i++)
  {
    html += "<b>Sender ";
    html += String(i+1);
    html += "</b><br>";

    html += "Name:<br>";
    html += "<input name='name";
    html += String(i);
    html += "' value='";
    html += stations[i].name;
    html += "'><br>";

    html += "URL:<br>";
    html += "<input size='60' name='url";
    html += String(i);
    html += "' value='";
    html += stations[i].url;
    html += "'><br><br>";

    // Lösch-Haken
    html += "<input type='checkbox' name='del";
    html += String(i);
    html += "'>";
    html += " Sender loeschen";

    html += "<br><br>";


  }

  html += "<hr>";
  html += "<h3>Neuer Sender</h3>";

  html += "Name:<br>";
  html += "<input name='newname'><br>";

  html += "URL:<br>";
  html += "<input size='60' name='newurl'><br><br>";


  html += "<input type='submit' value='Speichern'>";
  html += "</form>";

  html += "<br><a href='/'>Zurueck</a>";

  html += "</body></html>";

  server.send(200,"text/html",html);
}

// --- Hadle Station Speichern ---
void handleSaveStations()
{
  fs::File f = LittleFS.open("/stations.txt","w");



  if(f)
  {
    for(int i=0;i<stationCount;i++)
    {

      if(server.hasArg("del" + String(i)))
      {
        continue;   // diesen Sender überspringen
      }

      String name =
        server.arg("name" + String(i));

      String url =
        server.arg("url" + String(i));

      if(name.length() > 0 &&
         url.length() > 10)
      {
        f.print(name);
        f.print(";");
        f.println(url);
      }

    }


    String newName = server.arg("newname");
    String newUrl  = server.arg("newurl");

    if(newName.length() > 0 &&
      newUrl.length() > 10)
    {
      f.print(newName);
      f.print(";");
      f.println(newUrl);
    }

    f.close();
  }

  server.send(
    200,
    "text/html",
    "<html><body>"
    "<h2>Gespeichert</h2>"
    "Neustart..."
    "</body></html>"
  );

  delay(1500);
  ESP.restart();
}


// -------- Akku Statusanzeige -----------

float readBatteryVoltage() {

  long sum = 0;

  for (int i = 0; i < 20; i++) {
    sum += analogRead(BATTERY_PIN);
    delay(2);
  }

  float raw = sum / 20.0;

  // KALIBRIERFAKTOR
  float voltage = (raw / 4095.0) * 3.9;

  // Spannungsteiler
  voltage *= 3.2;

  return voltage;
}

int batteryPercent(float v) {

  if (v >= 8.40) return 100;
  if (v >= 8.20) return 90;
  if (v >= 8.00) return 80;
  if (v >= 7.80) return 70;
  if (v >= 7.60) return 60;
  if (v >= 7.40) return 50;
  if (v >= 7.20) return 40;
  if (v >= 7.00) return 30;
  if (v >= 6.80) return 20;
  if (v >= 6.60) return 10;

  return 0;
}


void drawBattery() {

  float voltage = readBatteryVoltage();
  
  int percent = batteryPercent(voltage);

  int x = 230;
  int y = 140;

  int w = 50;
  int h = 15;

  // Rahmen
  tft.drawRect(x, y, w, h, TFT_WHITE);

  // Batteriepol
  tft.fillRect(x + w, y + 6, 1, 4, TFT_WHITE);

  // Füllstand
  int fill = map(percent, 0, 100, 0, w - 4);

  uint16_t color = TFT_MAGENTA;

  if (percent < 50) color = TFT_NAVY;
  if (percent < 20) color = TFT_GREEN;

  tft.fillRect(x + 2, y + 2, fill, h - 4, color);

  // Prozenttext
  //tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  //tft.setTextSize(1);

  tft.drawCentreString(
    String(percent) + "%",
    x + w / 2,
    y + 3,
    1
  );
}





// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);
  delay(500);
  pinMode(4, OUTPUT);
  digitalWrite(4, LOW);
  delay(50);
  digitalWrite(4, HIGH);
  delay(200);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);

  initFS();

  if(!loadWiFi())
  {
    wifiSSID = "";
    wifiPASS = "";
  }

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(
    "ESP32-Radio",
    "12345678"
  );
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());


  server.on("/", handleRoot);
  server.on("/wifi", handleWifi);
  server.on("/savewifi", HTTP_POST, handleSaveWifi);
  server.on("/stations", handleStations);
  server.on("/savestations", HTTP_POST, handleSaveStations);
  server.on("/restart", handleRestart);
  server.on("/next", handleNext);
  server.on("/volume", HTTP_POST, handleVolume);
  server.begin();
  Serial.println("Webserver gestartet");

  if(wifiSSID.length() > 0)
  {
    setupWiFi();
  }

  loadStations();

  if (stationCount == 0)
  {
    Serial.println("Fallback aktiv");
    // Default Stations hier
  }

  out = new AudioOutputI2S();
  out->SetPinout(26, 25, 22);
  out->SetGain(volume);

  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);
  pinMode(ENC_BTN, INPUT_PULLUP);

  startStation(currentStation);
  drawBattery();
}

// ---------------- LOOP ----------------
void loop()
{
  if (mp3 && mp3->isRunning())
  {
    if (!mp3->loop())
      mp3->stop();
  }

  server.handleClient();

  handleEncoder();
  handleButton();

  handleUI();   // ⭐ WICHTIG: alles UI hier
}