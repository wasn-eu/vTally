/*
  vTally for vMix
  Copyright 2021 CaliHC
*/

#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiClient.h>
#include <Adafruit_NeoPixel.h>
#include <SoftwareSerial.h>
#include "ESPAsyncUDP.h"
#include "FS.h"

// Constants
const float vers = 1.6; 

const int SsidMaxLength = 24;
const int PassMaxLength = 24;
const int HostNameMaxLength = 24;
const int TallyNumberMaxValue = 64;

// LED setting
#define LED_PIN D2
#define LED_NUM 1
Adafruit_NeoPixel leds = Adafruit_NeoPixel(LED_NUM, LED_PIN, NEO_GRB + NEO_KHZ800);

// Settings object
struct Settings
{
  char ssid[SsidMaxLength];
  char pass[PassMaxLength];
  char hostName[HostNameMaxLength];
  int tallyNumber;
  int intensFull;
  int intensDim;
  int prgred;
  int prggreen;
  int prgblue;
  int prvred;
  int prvgreen;
  int prvblue;
  int offred;
  int offgreen;
  int offblue;
  unsigned int viscabaud;
  unsigned int viscaport;
};

// Default settings object
Settings defaultSettings = {
  "SSID",
  "PASSWORD",
  "vmix_hostname",
  1,
  255,
  128,
  0,
  255,
  0,
  255,
  128,
  0,
  0,
  0,
  255,
  9600,
  52381
};

Settings settings;

// HTTP Server settings
ESP8266WebServer httpServer(80);
char deviceName[32];
int status = WL_IDLE_STATUS;
bool apEnabled = false;
char apPass[64];

// vMix settings
int port = 8099;

//// Tally info
char currentState = -1;
char oldState = -1;
const char tallyStateProgram = 1;
const char tallyStatePreview = 2;

// The WiFi client
WiFiClient client;
const int timeout = 10;
const int delayTime = 10000;
int vmixcon = 0;

// Time measure
const int interval = 5000;
unsigned long lastCheck = 0;

// VISCAoIP 2 serial
SoftwareSerial viscaSerial;
int udpstate = 0;

//// RS232 Serial Settings
const int txpin = D5;
const int rxpin = D6;

//// Use the following constants and functions to modify the speed of PTZ commands
const double ZOOMMULT = 0.3;      // speed multiplier for the zoom functions
const double ZOOMEXP = 1.5;       // exponential curve for the speed modification
const double PTZMULT = 0.3;       // speed multiplier for the pan and tilt functions
const double PTZEXP = 1.0;        // exponential curve for the speed modification

//// STATE VARIABLES
AsyncUDP udp;
int lastclientport = 0;
IPAddress lastclientip;
bool pwr_is_on = false;

//// memory buffers for VISCA commands
size_t lastudp_len = 0;
uint8_t lastudp_in[16];
size_t lastser_len = 0;
uint8_t lastser_in[16];

//// quick use VISCA commands
const uint8_t pwr_on[] = {0x81, 0x01, 0x04, 0x00, 0x02, 0xff};
const uint8_t pwr_off[] = {0x81, 0x01, 0x04, 0x00, 0x03, 0xff};
const uint8_t addr_set[] = {0x88, 0x30, 0x01, 0xff};            // address set
const uint8_t if_clear[] = {0x88, 0x01, 0x00, 0x01, 0xff};      // if clear
const uint8_t ifClear[] = {0x88, 0x01, 0x00, 0x01, 0xff}; // Checks to see if communication line is clear
const uint8_t irOff[] = {0x81, 0x01, 0x06, 0x09, 0x03, 0xff}; // Turn off IR control (required for speed control of Pan/Tilt on TelePresence cameras)
const uint8_t callLedOn[] = {0x81, 0x01, 0x33, 0x01, 0x01, 0xff};
const uint8_t callLedOff[] = {0x81, 0x01, 0x33, 0x01, 0x00, 0xff};
const uint8_t callLedBlink[] = {0x81, 0x01, 0x33, 0x01, 0x02, 0xff};
/*
 * Video formats values:
 * Value    HDMI    SDI
 * 0x00     1080p25 1080p25
 * 0x01     1080p30 1080p30
 * 0x02     1080p50 720p50
 * 0x03     1080p60 720p60
 * 0x04     720p25  720p25
 * 0x05     720p30  720p30
 * 0x06     720p50  720p50
 * 0x07     720p60  720p60
 */
const uint8_t format = 0x01;
const uint8_t videoFormat[] = { 0x81, 0x01, 0x35, 0x00, format, 0x00, 0xff }; // 8x 01 35 0p 0q 0r ff p = reserved, q = video mode, r = Used in PrecisionHD 720p camera.



// Load settings from EEPROM
void loadSettings()
{
  Serial.println(F("+--------------------+"));
  Serial.println(F("|  Loading settings  |"));
  Serial.println(F("+--------------------+"));

  long ptr = 0;

  for (int i = 0; i < SsidMaxLength; i++)
  {
    settings.ssid[i] = EEPROM.read(ptr);
    ptr++;
  }

  for (int i = 0; i < PassMaxLength; i++)
  {
    settings.pass[i] = EEPROM.read(ptr);
    ptr++;
  }

  for (int i = 0; i < HostNameMaxLength; i++)
  {
    settings.hostName[i] = EEPROM.read(ptr);
    ptr++;
  }

  settings.tallyNumber = EEPROM.read(ptr);

  ptr++;
  settings.intensFull = EEPROM.read(ptr);

  ptr++;
  settings.intensDim = EEPROM.read(ptr);

  ptr++;
  settings.prgred = EEPROM.read(ptr);
  ptr++;
  settings.prggreen = EEPROM.read(ptr);
  ptr++;
  settings.prgblue = EEPROM.read(ptr);

  ptr++;
  settings.prvred = EEPROM.read(ptr);
  ptr++;
  settings.prvgreen = EEPROM.read(ptr);
  ptr++;
  settings.prvblue = EEPROM.read(ptr);
  
  ptr++;
  settings.offred = EEPROM.read(ptr);
  ptr++;
  settings.offgreen = EEPROM.read(ptr);
  ptr++;
  settings.offblue = EEPROM.read(ptr);

  ptr++;
  byte low = EEPROM.read(ptr);
  ptr++;
  byte high = EEPROM.read(ptr);
  settings.viscabaud = low + ((high << 8) & 0xFF00);
  ptr++;
  low = EEPROM.read(ptr);
  ptr++;
  high = EEPROM.read(ptr);
  settings.viscaport = low + ((high << 8) & 0xFF00);
  

  if (strlen(settings.ssid) == 0 || strlen(settings.pass) == 0 || strlen(settings.hostName) == 0 || settings.tallyNumber == 0 || settings.intensFull == 0 || settings.intensDim == 0 || settings.viscabaud == 0 || settings.viscaport == 0)
  {
    Serial.println(F("| No settings found"));
    Serial.println(F("| Loading default settings"));
    settings = defaultSettings;
    saveSettings();
    restart();
  }
  else
  {
    Serial.println(F("| Settings loaded"));
    printSettings();
    Serial.println(F("+---------------------"));
  }
}

// Save settings to EEPROM
void saveSettings()
{
  Serial.println(F("+--------------------+"));
  Serial.println(F("|  Saving settings   |"));
  Serial.println(F("+--------------------+"));

  long ptr = 0;

  for (int i = 0; i < 512; i++)
  {
    EEPROM.write(i, 0);
  }

  for (int i = 0; i < SsidMaxLength; i++)
  {
    EEPROM.write(ptr, settings.ssid[i]);
    ptr++;
  }

  for (int i = 0; i < PassMaxLength; i++)
  {
    EEPROM.write(ptr, settings.pass[i]);
    ptr++;
  }

  for (int i = 0; i < HostNameMaxLength; i++)
  {
    EEPROM.write(ptr, settings.hostName[i]);
    ptr++;
  }

  EEPROM.write(ptr, settings.tallyNumber);

  ptr++;
  EEPROM.write(ptr, settings.intensFull);

  ptr++;
  EEPROM.write(ptr, settings.intensDim);

  ptr++;
  EEPROM.write(ptr, settings.prgred);
  ptr++;
  EEPROM.write(ptr, settings.prggreen);
  ptr++;
  EEPROM.write(ptr, settings.prgblue);

  ptr++;
  EEPROM.write(ptr, settings.prvred);
  ptr++;
  EEPROM.write(ptr, settings.prvgreen);
  ptr++;
  EEPROM.write(ptr, settings.prvblue);

  ptr++;
  EEPROM.write(ptr, settings.offred);
  ptr++;
  EEPROM.write(ptr, settings.offgreen);
  ptr++;
  EEPROM.write(ptr, settings.offblue);

  ptr++;
  EEPROM.write(ptr, settings.viscabaud & 0xFF);
  ptr++;
  EEPROM.write(ptr, (settings.viscabaud >> 8) & 0xFF);

  ptr++;
  EEPROM.write(ptr, settings.viscaport & 0xFF);
  ptr++;
  EEPROM.write(ptr, (settings.viscaport >> 8) & 0xFF);
  
  EEPROM.commit();

  Serial.println(F("| Settings saved"));
  printSettings();
  Serial.println(F("+---------------------"));
}

// Print settings
void printSettings()
{
  Serial.print(F("| SSID            : "));
  Serial.println(settings.ssid);
  Serial.print(F("| SSID Password   : "));
  Serial.println(settings.pass);
  Serial.print(F("| vMix Hostname/IP: "));
  Serial.println(settings.hostName);
  Serial.print(F("| Tally number    : "));
  Serial.println(settings.tallyNumber);
  Serial.print(F("| Intensity Full  : "));
  Serial.println(settings.intensFull);
  Serial.print(F("| Intensity Dim   : "));
  Serial.println(settings.intensDim);
  Serial.print(F("| Program-Color   : "));
  Serial.print(settings.prgred);
  Serial.print(F(","));
  Serial.print(settings.prggreen);
  Serial.print(F(","));
  Serial.println(settings.prgblue);
  Serial.print(F("| Preview-Color   : "));
  Serial.print(settings.prvred);
  Serial.print(F(","));
  Serial.print(settings.prvgreen);
  Serial.print(F(","));
  Serial.println(settings.prvblue);
  Serial.print(F("| Off-Color       : "));
  Serial.print(settings.offred);
  Serial.print(F(","));
  Serial.print(settings.offgreen);
  Serial.print(F(","));
  Serial.println(settings.offblue);
  Serial.print(F("| VISCA baud      : "));
  Serial.println(settings.viscabaud);
  Serial.print(F("| VISCA port      : "));
  Serial.println(settings.viscaport);
}

// Set led intensity from 0 to 255
void ledSetIntensity(int intensity)
{
  leds.setBrightness(intensity);
}

// Set LED's off
void ledSetOff()
{
  leds.setPixelColor(0, leds.Color(0,0,0));
  ledSetIntensity(0);
  leds.show();
}

// Draw corner dots
void ledSetCornerDots()
{
  leds.setPixelColor(0, leds.Color(settings.offred,settings.offgreen,settings.offblue));
  ledSetIntensity(settings.intensDim);
  leds.show();
}

// Draw L(ive) with LED's
void ledSetProgram()
{
  leds.setPixelColor(0, leds.Color(settings.prgred,settings.prggreen,settings.prgblue));
  ledSetIntensity(settings.intensFull);
  leds.show();
}

// Draw P(review) with LED's
void ledSetPreview()
{
  leds.setPixelColor(0, leds.Color(settings.prvred,settings.prvgreen,settings.prvblue));
  ledSetIntensity(settings.intensFull);
  leds.show();
}

// Draw C(onnecting) with LED's
void ledSetConnecting()
{
  leds.setPixelColor(0, leds.Color(0,255,255));
  ledSetIntensity(settings.intensDim);
  leds.show();
}

// Draw S(ettings) with LED's
void ledSetSettings()
{
  leds.setPixelColor(0, leds.Color(255,0,255));
  ledSetIntensity(settings.intensDim);
  leds.show();
}

// Set tally to off
void tallySetOff()
{
  Serial.println(F("| Tally off"));

  ledSetCornerDots();
  send_visca(callLedOff);
}

// Set tally to program
void tallySetProgram()
{
  Serial.println(F("| Tally program"));

  ledSetProgram();
  send_visca(callLedOn);
}

// Set tally to preview
void tallySetPreview()
{
  Serial.println(F("| Tally preview"));

  ledSetPreview();
  send_visca(callLedBlink);
}

// Set tally to connecting
void tallySetConnecting()
{
  ledSetConnecting();
}

// Handle incoming data
void handleData(String data)
{
  // Check if server data is tally data
  if (data.indexOf("TALLY OK") == 0)
  {
    vmixcon = 1;
    char newState = data.charAt(settings.tallyNumber + 8);

    // Check if tally state has changed
    if (currentState != newState)
    {
      currentState = newState;

      switch (currentState)
      {
        case '0':
          tallySetOff();
          break;
        case '1':
          tallySetProgram();
          break;
        case '2':
          tallySetPreview();
          break;
        default:
          tallySetOff();
      }
    }
  }
  else
  {
    vmixcon = 1;
    Serial.print(F("| Response from vMix: "));
    Serial.println(data);
    
    //Serial.print(F("| FreeHeap: "));
    //Serial.println(ESP.getFreeHeap(),DEC);
  }
}

// Start access point
void apStart()
{
  ledSetSettings();
  Serial.println(F("(+--------------------+"));
  Serial.println(F("(|       AP Start     |"));
  Serial.println(F("(+--------------------+"));
  Serial.print(F("| AP SSID         : "));
  Serial.println(deviceName);
  Serial.print(F("| AP password     : "));
  Serial.println(apPass);

  WiFi.mode(WIFI_AP);
  WiFi.hostname(deviceName);
  WiFi.softAP(deviceName, apPass);
  delay(100);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print(F("| IP address      : "));
  Serial.println(myIP);
  Serial.println(F("+---------------------"));


  apEnabled = true;
}

// Handle http server Tally request
void tallyPageHandler()
{
  String response_message = F("<!DOCTYPE html>");
  
  response_message += F("<html lang='en'>");
  response_message += F("<head>");
  response_message += F("<title>vTally by CaliHC - ") + String(deviceName) + F("</title>");
  response_message += F("<meta name='viewport' content='width=device-width, initial-scale=1, shrink-to-fit=no'>");
  response_message += F("<meta charset='utf-8'>");
  response_message += F("<link rel='icon' type='image/x-icon' href='favicon.ico'>");
  response_message += F("<link rel='stylesheet' href='styles.css'>");
  response_message += F("<style>body {width: 100%;height: 100%;padding: 0px;}</style>");
  response_message += F("</head>");

  response_message += F("<body class='bg-light'>");

  response_message += F("<table class='table'><tbody  style='border-radius: 0px 0px 10px 10px;background-color:#d5dadd;'>");
  
  response_message += F("<tr><th style='background-color:rgb(");
  
  if (vmixcon == 0) { currentState = '3'; }
  if (vmixcon == 1 && (currentState != '0' && currentState != '1' && currentState != '2')) { currentState = 4; }
  
  switch (currentState)
      {
        case '0':
          response_message += String(settings.offred) + F(",") + String(settings.offgreen) + F(",") + String(settings.offblue); //off
          break;
        case '1':
          response_message += String(settings.prgred) + F(",") + String(settings.prggreen) + F(",") + String(settings.prgblue); //prg
          break;
        case '2':
          response_message += String(settings.prvred) + F(",") + String(settings.prvgreen) + F(",") + String(settings.prvblue); //prv
          break;
        case '3':
          response_message += F("255,0,0"); // no vMix Server
          break;
        case '4':
          response_message += F("255,255,0"); // no vMix Server
          break;
        default:
          response_message += String(settings.offred) + F(",") + String(settings.offgreen) + F(",") + String(settings.offblue); //default off
      }
  response_message += F(");'>&nbsp;</th><tr>");
  
  
  response_message += F("<tr><th style='text-align:center;background-color:rgb(");
  switch (currentState)
      {
        case '0':
          response_message += String(settings.offred) + F(",") + String(settings.offgreen) + F(",") + String(settings.offblue); //off
          break;
        case '1':
          response_message += String(settings.prgred) + F(",") + String(settings.prggreen) + F(",") + String(settings.prgblue); //prg
          break;
        case '2':
          response_message += String(settings.prvred) + F(",") + String(settings.prvgreen) + F(",") + String(settings.prvblue); //prv
          break;
        case '3':
          response_message += F("255,0,0"); // no vMix Server
          break;
        case '4':
          response_message += F("255,255,0"); // no vMix Server
          break;
        default:
          response_message += String(settings.offred) + F(",") + String(settings.offgreen) + F(",") + String(settings.offblue); //default off
      }
  response_message += F(");color:");
  switch (currentState)
      {
        case '0':
          response_message += F("#ffffff"); //off
          break;
        case '1':
          response_message += F("#ffffff"); //prg
          break;
        case '2':
          response_message += F("#ffffff"); //prv
          break;
        case '3':
          response_message += F("#ffffff"); // no vMix Server
          break;
        case '4':
          response_message += F("#ffffff"); // no vMix Server
          break;
        default:
          response_message += F("#ffffff"); //default off
      }
  response_message += F("'>");
  switch (currentState)
      {
        case '0':
          response_message += F("OFF"); //off
          break;
        case '1':
          response_message += F("PROGRAM"); //prg
          break;
        case '2':
          response_message += F("PREVIEW"); //prv
          break;
        case '3':
          response_message += F("vMix Server not found!"); // no vMix Server
          break;
        case '4':
          response_message += F("connected to vMix Server, waiting for data."); // no vMix Server
          break;
        default:
          response_message += F("OFF"); //default off
      }
  response_message += F("</th></tr>");

  
  response_message += F("<tr><th style='background-color:rgb(");
  switch (currentState)
      {
        case '0':
          response_message += String(settings.offred) + F(",") + String(settings.offgreen) + F(",") + String(settings.offblue); //off
          break;
        case '1':
          response_message += String(settings.prgred) + F(",") + String(settings.prggreen) + F(",") + String(settings.prgblue); //prg
          break;
        case '2':
          response_message += String(settings.prvred) + F(",") + String(settings.prvgreen) + F(",") + String(settings.prvblue); //prv
          break;
        case '3':
          response_message += F("255,0,0"); // no vMix Server
          break;
        case '4':
          response_message += F("255,255,0"); // no vMix Server
          break;
        default:
          response_message += String(settings.offred) + F(",") + String(settings.offgreen) + F(",") + String(settings.offblue); //default off
      }
  response_message += F(");'>&nbsp;</th><tr>");
  
  response_message += F("<tr><th style='border-radius: 0px 0px 10px 10px;background-color:rgb(");
  switch (currentState)
      {
        case '0':
          response_message += String(settings.offred) + F(",") + String(settings.offgreen) + F(",") + String(settings.offblue); //off
          break;
        case '1':
          response_message += String(settings.prgred) + F(",") + String(settings.prggreen) + F(",") + String(settings.prgblue); //prg
          break;
        case '2':
          response_message += String(settings.prvred) + F(",") + String(settings.prvgreen) + F(",") + String(settings.prvblue); //prv
          break;
        case '3':
          response_message += F("255,0,0"); // no vMix Server
          break;
        case '4':
          response_message += F("255,255,0"); // no vMix Server
          break;
        default:
          response_message += String(settings.offred) + F(",") + String(settings.offgreen) + F(",") + String(settings.offblue); //default off
      }
  response_message += F(");'>&nbsp;</th><tr>");
  
  response_message += F("</tbody></table>");
  
  response_message += F("</body></html>");

  httpServer.sendHeader("Connection", "close");
  httpServer.send(200, "text/html", String(response_message));

  //Serial.print(F("| FreeHeap: "));
  //Serial.println(ESP.getFreeHeap(),DEC);
}

// Handle http server root request
void rootPageHandler()
{
  String response_message = F("<!DOCTYPE html>");
  response_message += F("<html lang='en'>");
  response_message += F("<head>");
  response_message += F("<title>vTally by CaliHC - ") + String(deviceName) + F("</title>");
  response_message += F("<meta name='viewport' content='width=device-width, initial-scale=1, shrink-to-fit=no'>");
  response_message += F("<meta charset='utf-8'>");
  response_message += F("<link rel='icon' type='image/x-icon' href='favicon.ico'>");
  response_message += F("<link rel='stylesheet' href='styles.css'>");
  response_message += F("<script src='jquery.slim.min.js'></script>");
  response_message += F("<script type='text/javascript'>");
  response_message += F("var ESPurl = 'http://")+ WiFi.localIP().toString() + F("/zend';");
  response_message += F("if(typeof(EventSource) !== 'undefined') {");
  response_message += F("  var source = new EventSource(ESPurl);");
  response_message += F("  source.onmessage = function(event) {");
  response_message += F("    if(event.data == 'refresh0' || event.data == 'refresh1') {");
  response_message += F("      $('#tally').html(\"<object type='text/html' data='/tally' style='height:100%;width:100%;'></oject>\");");
  response_message += F("      if(event.data == 'refresh1') {");
  response_message += F("        $('#vmixstatus').html(\"Connected\");");
  response_message += F("        $('#vmixstatus').css(\"background-color\",\"green\");");
  response_message += F("      }");
  response_message += F("      if(event.data == 'refresh0') {");
  response_message += F("        $('#vmixstatus').html(\"Disconnected\");");
  response_message += F("        $('#vmixstatus').css(\"background-color\",\"red\");");
  response_message += F("      }");
  response_message += F("    }"); 
  response_message += F("  }");
  response_message += F("} else {");
  response_message += F("  document.getElementById('tally').innerHTML('Your browser does not support EventSource!')");
  response_message += F("}");
  
  response_message += F("</script>");
  response_message += F("<style>body {width: 100%;height: 100%;padding: 25px;}</style>");
  response_message += F("</head>");

  response_message += F("<body class='bg-light'>");

  response_message += F("<h1 style='border-radius: 10px 10px 0px 0px;background-color:#0066ff;color:#FFFFFF;text-align:center;padding:5px;margin-bottom:0px;'><span style='vertical-align:text-top;'>vTally by </span><a href=https://www.calihc.de target=_new><img src='logo.png' style='vertical-align:bottom;'></a></h1>");

  response_message += F("<h1 style='border-radius:0px 0px 10px 10px;background-color:#c6cdd2;text-align:center;'>vTally ID: ") + String(settings.tallyNumber) + F("</h1>");

  response_message += F("<form action='/save' method='post' enctype='multipart/form-data' data-ajax='false'>");
  
  response_message += F("<div data-role='content' class='row' style='margin-bottom:.5rem;'>");

  response_message += F("<div class='col-md-6'>");
  response_message += F("<h2 style='border-radius: 10px 10px 0px 0px;margin-bottom:0px;background-color:#99c2ff;padding:2px;'>&nbsp; Network/vMix/VISCA Settings</h2>");

  response_message += F("<table class='table' style='border-radius: 0px 0px 10px 10px;'><tbody  style='border-radius: 0px 0px 10px 10px;background-color:#d5dadd;'><tr style='border-radius: 0px 0px 10px 10px;'><td style='border-radius: 0px 0px 10px 10px;'>");

  response_message += F("<div class='form-group row'>");
  response_message += F("<label for='ssid' class='col-sm-4 col-form-label'>WiFi SSID</label>");
  response_message += F("<div class='col-sm-8'>");
  response_message += F("<input id='ssid' class='form-control' type='text' size='64' maxlength='32' name='ssid' value='") + String(settings.ssid) + F("'>");
  response_message += F("</div></div>");

  response_message += F("<div class='form-group row'>");
  response_message += F("<label for='ssidpass' class='col-sm-4 col-form-label'>WiFi Passphrase</label>");
  response_message += F("<div class='col-sm-8'>");
  response_message += F("<input id='ssidpass' class='form-control' type='password' autocomplete='current-password' size='64' maxlength='32' name='ssidpass' value='") + String(settings.pass) + F("'>");
  response_message += F("</div></div>");

  response_message += F("<hr>");

  response_message += F("<div class='form-group row'>");
  response_message += F("<label for='hostname' class='col-sm-4 col-form-label'>vMix Server Hostname/IP</label>");
  response_message += F("<div class='col-sm-8'>");
  response_message += F("<input id='hostname' class='form-control' type='text' size='64' maxlength='32' name='hostname' value='") + String(settings.hostName) + F("'>");
  response_message += F("</div></div>");

  response_message += F("<div class='form-group row'>");
  response_message += F("<label for='inputnumber' class='col-sm-4 col-form-label'>Tally number (1-") + String(TallyNumberMaxValue) + F(")</label>");
  response_message += F("<div class='col-sm-8'>");
  response_message += F("<input id='inputnumber' class='form-control' type='number' size='64' min='0' max='") + String(TallyNumberMaxValue) + F("' name='inputnumber' value='") + String(settings.tallyNumber) + F("'>");
  response_message += F("</div></div>");

  response_message += F("<hr>");

  response_message += F("<div class='form-group row'>");
  response_message += F("<label for='inputnumber' class='col-sm-4 col-form-label'>VISCA Baudrate (9600)</label>");
  response_message += F("<div class='col-sm-8'>");
  response_message += F("<select id='viscabaud' name='viscabaud' class='form-control'>");
  response_message += F("<option value='4800' ");
  (String(settings.viscabaud) == "4800") ? response_message +="selected" : response_message +="";
  response_message += F(">4800</option>");
  response_message += F("<option value='9600' ");
  (String(settings.viscabaud) == "9600") ? response_message +="selected" : response_message +="";
  response_message += F(">9600</option>");
  response_message += F("<option value='14400' ");
  (String(settings.viscabaud) == "14400") ? response_message +="selected" : response_message +="";
  response_message += F(">14400</option>");
  response_message += F("<option value='19200' ");
  (String(settings.viscabaud) == "19200") ? response_message +="selected" : response_message +="";
  response_message += F(">19200</option>");
  response_message += F("<option value='57600' ");
  (String(settings.viscabaud) == "57600") ? response_message +="selected" : response_message +=""; 
  response_message += F(">57600</option>");
  response_message += F("<option value='115200' ");
  (String(settings.viscabaud) == "115200") ? response_message +="selected" : response_message +="";
  response_message += F(">115200</option>");
  response_message += F("</select>");
  //response_message += F("<input id='viscabaud' class='form-control' type='number' size='64' min='2400' max='115200' name='viscabaud' value='") + String(settings.viscabaud) + F("'>");
  response_message += F("</div></div>");

  response_message += F("<div class='form-group row'>");
  response_message += F("<label for='viscaport' class='col-sm-4 col-form-label'>VISCA UDP Port (52381)</label>");
  response_message += F("<div class='col-sm-8'>");
  response_message += F("<input id='viscaport' class='form-control' type='number' size='64' min='1024' max='65554' name='viscaport' value='") + String(settings.viscaport) + F("'>");
  response_message += F("</div></div>");

  response_message += F("<div class='form-group row' style='border-radius:0px 0px 0px 10px;background-color:c6cdd2;'>");
  response_message += F("<label for='save' class='col-sm-4 col-form-label'> </label>");
  response_message += F("<div class='col-sm-8' style='border-radius:0px 0px 10px 0px;text-align:center;'>");
  response_message += F("<input type='submit' value='SAVE' class='btn btn-primary'>");
  response_message += F("</div></div>");

  response_message += F("</td></tr></tbody></table>");
  
  response_message += F("</div>");

  response_message += F("<div class='col-md-6'>");
  response_message += F("<h2 style='border-radius: 10px 10px 0px 0px;margin-bottom:0px;background-color:#99c2ff;padding:2px;'>&nbsp;   vMix Tally Settings</h2>");

  response_message += F("<table class='table' style='height:90%;border-radius: 0px 0px 10px 10px;'><tbody  style='border-radius: 0px 0px 10px 10px;background-color:#d5dadd;'><tr style='border-radius: 0px 0px 10px 10px;'><td style='border-radius: 0px 0px 10px 10px;'>");

  response_message += F("<div class='form-group row'>");
  response_message += F("<label for='prg' class='col-sm-4 col-form-label'>Program Color (0-254)</label>");
  response_message += F("<div class='col-sm-8'>");
  response_message += F("<div class='form-group row' style='margin-bottom:0px;'>");
  response_message += F("<div style='display: -webkit-flex;display: flex;align-items: center'>&nbsp;R&nbsp;</div><div>");
  response_message += F("<input id='prgred' class='form-control' type='number' size='21' min='0' max='254' style='width:5em;' name='prgred' value='") + String(settings.prgred) + F("' onchange=\"document.getElementById('pprg').style.backgroundColor = 'rgb('+document.getElementById('prgred').value+','+document.getElementById('prggreen').value+','+document.getElementById('prgblue').value+')'\">");
  response_message += F("</div>");
  response_message += F("<div style='display: -webkit-flex;display: flex;align-items: center'>&nbsp;G&nbsp;</div><div>");
  response_message += F("<input id='prggreen' class='form-control' type='number' size='21' min='0' max='254' style='width:5em;' name='prggreen' value='") + String(settings.prggreen) + F("'onchange=\"document.getElementById('pprg').style.backgroundColor = 'rgb('+document.getElementById('prgred').value+','+document.getElementById('prggreen').value+','+document.getElementById('prgblue').value+')'\">");
  response_message += F("</div>");
  response_message += F("<div style='display: -webkit-flex;display: flex;align-items: center'>&nbsp;B&nbsp;</div><div>");
  response_message += F("<input id='prgblue' class='form-control' type='number' size='21' min='0' max='254' style='width:5em;' name='prgblue' value='") + String(settings.prgblue) + F("'onchange=\"document.getElementById('pprg').style.backgroundColor = 'rgb('+document.getElementById('prgred').value+','+document.getElementById('prggreen').value+','+document.getElementById('prgblue').value+')'\">");
  response_message += F("</div>");
  response_message += F("<div>&nbsp;&nbsp;</div>");
  response_message += F("<div id='pprg' style='display: -webkit-flex;display: flex;align-items: center;color:#ffffff;background-color:rgb(") + String(settings.prgred) + F(",") + String(settings.prggreen) + F(",") + String(settings.prgblue) + F(")'>&nbsp;Program&nbsp;</div>");
  response_message += F("</div></div></div>");

  response_message += F("<div class='form-group row'>");
  response_message += F("<label for='prv' class='col-sm-4 col-form-label'>Preview Color (0-254)</label>");
  response_message += F("<div class='col-sm-8'>");
  response_message += F("<div class='form-group row' style='margin-bottom:0px;'>");
  response_message += F("<div style='display: -webkit-flex;display: flex;align-items: center'>&nbsp;R&nbsp;</div><div>");
  response_message += F("<input id='prvred' class='form-control' type='number' size='21' min='0' max='254' style='width:5em;' name='prvred' value='") + String(settings.prvred) + F("' onchange=\"document.getElementById('pprv').style.backgroundColor = 'rgb('+document.getElementById('prvred').value+','+document.getElementById('prvgreen').value+','+document.getElementById('prvblue').value+')'\">");
  response_message += F("</div>");
  response_message += F("<div style='display: -webkit-flex;display: flex;align-items: center'>&nbsp;G&nbsp;</div><div>");
  response_message += F("<input id='prvgreen' class='form-control' type='number' size='21' min='0' max='254' style='width:5em;' name='prvgreen' value='") + String(settings.prvgreen) + F("' onchange=\"document.getElementById('pprv').style.backgroundColor = 'rgb('+document.getElementById('prvred').value+','+document.getElementById('prvgreen').value+','+document.getElementById('prvblue').value+')'\">");
  response_message += F("</div>");
  response_message += F("<div style='display: -webkit-flex;display: flex;align-items: center'>&nbsp;B&nbsp;</div><div>");
  response_message += F("<input id='prvblue' class='form-control' type='number' size='21' min='0' max='254' style='width:5em;' name='prvblue' value='") + String(settings.prvblue) + F("' onchange=\"document.getElementById('pprv').style.backgroundColor = 'rgb('+document.getElementById('prvred').value+','+document.getElementById('prvgreen').value+','+document.getElementById('prvblue').value+')'\">");
  response_message += F("</div>");
  response_message += F("<div>&nbsp;&nbsp;</div>");
  response_message += F("<div id='pprv' style='display: -webkit-flex;display: flex;align-items: center;color:#ffffff;background-color:rgb(") + String(settings.prvred) + F(",") + String(settings.prvgreen) + F(",") + String(settings.prvblue) + F(")'>&nbsp;&nbsp;Preview&nbsp;&nbsp;</div>");
  response_message += F("</div></div></div>");
  
  response_message += F("<div class='form-group row'>");
  response_message += F("<label for='off' class='col-sm-4 col-form-label'>Off Color (0-254)</label>");
  response_message += F("<div class='col-sm-8'>");
  response_message += F("<div class='form-group row' style='margin-bottom:0px;'>");
  response_message += F("<div style='display: -webkit-flex;display: flex;align-items: center'>&nbsp;R&nbsp;</div><div>");
  response_message += F("<input id='offred' class='form-control' type='number' size='21' min='0' max='254' style='width:5em;' name='offred' value='") + String(settings.offred) + F("' onchange=\"document.getElementById('poff').style.backgroundColor = 'rgb('+document.getElementById('offred').value+','+document.getElementById('offgreen').value+','+document.getElementById('offblue').value+')'\">");
  response_message += F("</div>");
  response_message += F("<div style='display: -webkit-flex;display: flex;align-items: center'>&nbsp;G&nbsp;</div><div>");
  response_message += F("<input id='offgreen' class='form-control' type='number' size='21' min='0' max='254' style='width:5em;' name='offgreen' value='") + String(settings.offgreen) + F("' onchange=\"document.getElementById('poff').style.backgroundColor = 'rgb('+document.getElementById('offred').value+','+document.getElementById('offgreen').value+','+document.getElementById('offblue').value+')'\">");
  response_message += F("</div>");
  response_message += F("<div style='display: -webkit-flex;display: flex;align-items: center'>&nbsp;B&nbsp;</div><div>");
  response_message += F("<input id='offblue' class='form-control' type='number' size='21' min='0' max='254' style='width:5em;' name='offblue' value='") + String(settings.offblue) + F("' onchange=\"document.getElementById('poff').style.backgroundColor = 'rgb('+document.getElementById('offred').value+','+document.getElementById('offgreen').value+','+document.getElementById('offblue').value+')'\">");
  response_message += F("</div>");
  response_message += F("<div>&nbsp;&nbsp;</div>");
  response_message += F("<div id='poff' style='display: -webkit-flex;display: flex;align-items: center;color:#ffffff;background-color:rgb(") + String(settings.offred) + F(",") + String(settings.offgreen) + F(",") + String(settings.offblue) + F(")'>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Off&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</div>");
  response_message += F("</div></div></div>");

  response_message += F("<hr>");

  response_message += F("<div class='form-group row' style='background-color=#dcdcdc'>");
  response_message += F("<label for='intensFull' class='col-sm-4 col-form-label'>Brightness Program/Preview (0-254)</label>");
  response_message += F("<div class='col-sm-8'>");
  response_message += F("<input id='intensFull' class='form-control' type='number' style='width:5em;' size='8' min='0' max='254' name='intensFull' value='") + String(settings.intensFull) + F("'>");
  response_message += F("</div></div>");

  response_message += F("<div class='form-group row'>");
  response_message += F("<label for='intensDim' class='col-sm-4 col-form-label'>Brightness Off (0-254)</label>");
  response_message += F("<div class='col-sm-8'>");
  response_message += F("<input id='intensDim' class='form-control' type='number' style='width:5em;' size='8' min='0' max='254' name='intensDim' value='")+ String(settings.intensDim) + F("'>");
  response_message += F("</div></div>");
  
  response_message += F("<hr>");

  response_message += F("<div class='form-group row'>");
  response_message += F("<label class='col-sm-4 col-form-label'>&nbsp;</label>");
  response_message += F("<div class='col-sm-8'>&nbsp;</div></div>");

  response_message += F("<div class='form-group row' style='background-color:c6cdd2;'>");
  response_message += F("<label for='save' class='col-sm-4 col-form-label'> </label>");
  response_message += F("<div class='col-sm-8' style='text-align:center;'>");
  response_message += F("<input type='submit' value='SAVE' class='btn btn-primary'>");
  response_message += F("</div></div>");

  response_message += F("</td></tr></tbody></table>");
  
  response_message += F("</div></div></div>");
  response_message += F("</form>");

  response_message += F("<div data-role='content' class='row'>");
  response_message += F("&nbsp;</div>");
  
  response_message += F("<div data-role='content' class='row'>");

  response_message += F("<div class='col-md-6'>");
  response_message += F("<h2 style='border-radius: 10px 10px 0px 0px;margin-bottom:0px;background-color:#99c2ff;padding:2px;'>&nbsp; Device Information</h2>");
  response_message += F("<table class='table'><tbody  style='border-radius: 0px 0px 10px 10px;background-color:#d5dadd;'>");

  char ip[13];
  sprintf(ip, "%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
  response_message += F("<tr><th>IP</th><td>") + String(ip) + F("</td><th>WiFi Signal Strength</th><td style='background-color:");

  String color = "#ff0000"; 

  if (WiFi.RSSI() > -80) { color = "#ff0000"; }
  if (WiFi.RSSI() > -67) { color = "#ffff00"; }
  if (WiFi.RSSI() > -50) { color = "#00ff00"; }

  response_message += color;
  
  response_message += F(";'>") + String(WiFi.RSSI()) + F(" dBm</td></tr>");

  response_message += F("<tr><th>MAC</th><td>") + String(WiFi.macAddress()) + F("</td>");

  response_message += F("<th>WiFi AP</th>");
  if (apEnabled)
  {
    sprintf(ip, "%d.%d.%d.%d", WiFi.softAPIP()[0], WiFi.softAPIP()[1], WiFi.softAPIP()[2], WiFi.softAPIP()[3]);
    response_message += F("<td style='background-color:green;color:white;'>Active (") + String(ip) + F(")");
  }
  else
  {
    response_message += F("<td style='background-color:yellow;'>Inactive");
  }
  response_message += F("</td>");
  
  response_message += F("</tr>");
  
  response_message += F("<tr><th>Device Name</th><td>") + String(deviceName) + F("</td>");
  response_message += F("<th>vMix Status</th><td id='vmixstatus'");
  if (vmixcon == 1)
  {
    response_message += F("style='background-color:green;color:white;'>Connected");
  }
  else
  {
    response_message += F("style='background-color:red;color:white;'>Disconnected");
  }
  response_message += F("</td>");
  
  response_message += F("</tr>");

  response_message += F("<tr><th style='border-radius: 0px 0px 0px 10px;'>&nbsp;</th><td>&nbsp;</td>");
  response_message += F("<th>VISCA IP2SERIAL Status</th><td id='viscastatus'");
  if (udpstate == 1)
  {
    response_message += F("style='border-radius: 0px 0px 10px 0px;background-color:green;color:white;'>Running");
  }
  else
  {
    response_message += F("style='border-radius: 0px 0px 10px 0px;background-color:red;color:white;'>Not Running");
  }
  response_message += F("</td>");
  
  response_message += F("</tr>");
  
  response_message += F("</tbody></table>");
  response_message += F("</div>");



  response_message += F("<div class='col-md-6'>");
  response_message += F("<h2 style='border-radius: 10px 10px 0px 0px;margin-bottom:0px;background-color:#99c2ff;padding:2px;'>&nbsp; vMix Tally</h2>");

  response_message += F("<div id='tally' style='height:90%;'></div>");
  response_message += F("<script>document.getElementById('tally').innerHTML=\"<object type='text/html' data='/tally' style='height:100%;width:100%;'></object>\";</script>");
  
  response_message += F("</div>");


  
  response_message += F("</div>");

  response_message += F("<h4 style='border-radius: 10px 10px 10px 10px;background-color:#c8c8c8;text-align:center;margin-bottom:0px;'>vTally v") + String(vers) + F(" &nbsp;&nbsp;&nbsp; &copy; 2021 by <a href=https://www.calihc.de target=_new>CaliHC</a></h4>");

  response_message += F("</body>");
  response_message += F("</html>");

  httpServer.sendHeader("Connection", "close");
  httpServer.send(200, "text/html", String(response_message));

  //Serial.print(F("| FreeHeap: "));
  //Serial.println(ESP.getFreeHeap(),DEC);
}

// Settings POST handler
void handleSave()
{
  bool doRestart = false;

  httpServer.sendHeader("Location", String("/"), true);
  httpServer.send(302, "text/plain", "Redirected to: /");

  if (httpServer.hasArg("ssid"))
  {
    if (httpServer.arg("ssid").length() <= SsidMaxLength)
    {
      httpServer.arg("ssid").toCharArray(settings.ssid, SsidMaxLength);
      doRestart = true;
    }
  }

  if (httpServer.hasArg("ssidpass"))
  {
    if (httpServer.arg("ssidpass").length() <= PassMaxLength)
    {
      httpServer.arg("ssidpass").toCharArray(settings.pass, PassMaxLength);
      doRestart = true;
    }
  }

  if (httpServer.hasArg("hostname"))
  {
    if (httpServer.arg("hostname").length() <= HostNameMaxLength)
    {
      httpServer.arg("hostname").toCharArray(settings.hostName, HostNameMaxLength);
      doRestart = true;
    }
  }

  if (httpServer.hasArg("inputnumber"))
  {
    if (httpServer.arg("inputnumber").toInt() > 0 and httpServer.arg("inputnumber").toInt() <= TallyNumberMaxValue)
    {
      settings.tallyNumber = httpServer.arg("inputnumber").toInt();
      doRestart = true;
    }
  }

  if (httpServer.hasArg("intensFull"))
  {
    if (httpServer.arg("intensFull").toInt() >= 0 and httpServer.arg("intensFull").toInt() < 255)
    {
      settings.intensFull = httpServer.arg("intensFull").toInt();
      doRestart = true;
    }
  }

  if (httpServer.hasArg("intensDim"))
  {
    if (httpServer.arg("intensDim").toInt() >= 0 and httpServer.arg("intensDim").toInt() < 255)
    {
      settings.intensDim = httpServer.arg("intensDim").toInt();
      doRestart = true;
    }
  }
  
  if (httpServer.hasArg("prgred"))
  {
    if (httpServer.arg("prgred").toInt() >= 0 and httpServer.arg("prgred").toInt() < 255)
    {
      settings.prgred = httpServer.arg("prgred").toInt();
      doRestart = true;
    }
  }
  if (httpServer.hasArg("prggreen"))
  {
    if (httpServer.arg("prggreen").toInt() >= 0 and httpServer.arg("prggreen").toInt() < 255)
    {
      settings.prggreen = httpServer.arg("prggreen").toInt();
      doRestart = true;
    }
  }
  if (httpServer.hasArg("prgblue"))
  {
    if (httpServer.arg("prgblue").toInt() >= 0 and httpServer.arg("prgblue").toInt() < 255)
    {
      settings.prgblue = httpServer.arg("prgblue").toInt();
      doRestart = true;
    }
  }

  if (httpServer.hasArg("prvred"))
  {
    if (httpServer.arg("prvred").toInt() >= 0 and httpServer.arg("prvred").toInt() < 255)
    {
      settings.prvred = httpServer.arg("prvred").toInt();
      doRestart = true;
    }
  }
  if (httpServer.hasArg("prvgreen"))
  {
    if (httpServer.arg("prvgreen").toInt() >= 0 and httpServer.arg("prvgreen").toInt() < 255)
    {
      settings.prvgreen = httpServer.arg("prvgreen").toInt();
      doRestart = true;
    }
  }
  if (httpServer.hasArg("prvblue"))
  {
    if (httpServer.arg("prvblue").toInt() >= 0 and httpServer.arg("prvblue").toInt() < 255)
    {
      settings.prvblue = httpServer.arg("prvblue").toInt();
      doRestart = true;
    }
  }
  
  if (httpServer.hasArg("offred"))
  {
    if (httpServer.arg("offred").toInt() >= 0 and httpServer.arg("offred").toInt() < 255)
    {
      settings.offred = httpServer.arg("offred").toInt();
      doRestart = true;
    }
  }
  if (httpServer.hasArg("offgreen"))
  {
    if (httpServer.arg("offgreen").toInt() >= 0 and httpServer.arg("offgreen").toInt() < 255)
    {
      settings.offgreen = httpServer.arg("offgreen").toInt();
      doRestart = true;
    }
  }
  if (httpServer.hasArg("offblue"))
  {
    if (httpServer.arg("offblue").toInt() >= 0 and httpServer.arg("offblue").toInt() < 255)
    {
      settings.offblue = httpServer.arg("offblue").toInt();
      doRestart = true;
    }
  }

  if (httpServer.hasArg("viscabaud"))
  {
    if (httpServer.arg("viscabaud").toInt() >= 2400 and httpServer.arg("viscabaud").toInt() <= 115200)
    {
      settings.viscabaud = httpServer.arg("viscabaud").toInt();
      doRestart = true;
    }
  }

  if (httpServer.hasArg("viscaport"))
  {
    if (httpServer.arg("viscaport").toInt() >= 1024 and httpServer.arg("viscaport").toInt() <= 65554)
    {
      settings.viscaport = httpServer.arg("viscaport").toInt();
      doRestart = true;
    }
  }

  if (doRestart == true)
  {
    restart();
  }
}

// Connect to WiFi
void connectToWifi()
{
  Serial.println(F("+--------------------+"));
  Serial.println(F("| Connecting to WiFi |"));
  Serial.println(F("+--------------------+"));
  Serial.print(F("| SSID            : "));
  Serial.println(settings.ssid);
  Serial.print(F("| Passphrase      : "));
  Serial.println(settings.pass);
  Serial.println(F("|"));

  int timeout = 15;

  WiFi.mode(WIFI_STA);
  WiFi.hostname(deviceName);
  WiFi.begin(settings.ssid, settings.pass);

  Serial.print(F("| Waiting for connection ."));
  while (WiFi.status() != WL_CONNECTED and timeout > 0)
  {
    delay(1000);
    timeout--;
    Serial.print(F("."));
  }

  Serial.println(F(""));

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println(F("| WiFi connected"));
    Serial.println(F("|"));
    Serial.print(F("| IP address      : "));
    Serial.println(WiFi.localIP());
    Serial.print(F("| Device name     : "));
    Serial.println(deviceName);
    Serial.println(F("+---------------------"));
    Serial.println(F(""));
  }
  else
  {
    if (WiFi.status() == WL_IDLE_STATUS)
      Serial.println(F("| Idle"));
    else if (WiFi.status() == WL_NO_SSID_AVAIL)
      Serial.println(F("| No SSID Available"));
    else if (WiFi.status() == WL_SCAN_COMPLETED)
      Serial.println(F("| Scan Completed"));
    else if (WiFi.status() == WL_CONNECT_FAILED)
      Serial.println(F("| Connection Failed"));
    else if (WiFi.status() == WL_CONNECTION_LOST)
      Serial.println(F("| Connection Lost"));
    else if (WiFi.status() == WL_DISCONNECTED)
      Serial.println(F("| Disconnected"));
    else
      Serial.println(F("| Unknown Failure"));

    Serial.println(F("+---------------------"));
    apStart();
  }
}

// Connect to vMix instance
void connectTovMix()
{
  Serial.println(F("+--------------------+"));
  Serial.println(F("| Connecting to vMix |"));
  Serial.println(F("+--------------------+"));
  Serial.print(F("| Connecting to "));
  Serial.println(settings.hostName);

  if (client.connect(settings.hostName, port))
  {
    Serial.println(F("| Connected  to vMix"));

    Serial.println(F("+---------------------"));
    Serial.println(F(""));

    Serial.println(F("+--------------------+"));
    Serial.println(F("|  vMix Message Log  |"));
    Serial.println(F("+--------------------+"));
    
    tallySetOff();

    // Subscribe to the tally events
    client.println(F("SUBSCRIBE TALLY"));
  }
  else
  {
    vmixcon = 0;
    currentState = '3';
    Serial.println(F("| vMix Server not found!"));
    Serial.println(F("+---------------------"));
    Serial.println(F(""));
  }
}

void restart()
{
  saveSettings();

  Serial.println(F(""));
  Serial.println(F("+--------------------+"));
  Serial.println(F("|       RESTART      |"));
  Serial.println(F("+--------------------+"));
  Serial.println(F(""));

  ESP.restart();
  //start();
}

void banner()
{
  Serial.println(F(""));
  Serial.println(F(""));
  Serial.println(F("+--------------------+"));
  Serial.print(F("|   vTally    v"));
  Serial.print(String(vers));
  Serial.println(F("  |"));
  Serial.println(F("| (c)2021 by  CaliHC |"));
  Serial.println(F("+--------------------+"));
  Serial.println(F(""));
}

void start()
{
  tallySetConnecting();
  
  loadSettings();
  
  Serial.println(F(""));
  sprintf(deviceName, "vTally_%d", settings.tallyNumber);
  sprintf(apPass, "%s%s", deviceName, "_pwd");

  connectToWifi();

  if (WiFi.status() == WL_CONNECTED)
  {
    viscaSerial.begin(settings.viscabaud, SWSERIAL_8N1, rxpin, txpin, false, 200);
    start_visca();
    connectTovMix();
  }
}

double zoomcurve(int v)
{
  return ZOOMMULT * pow(v, ZOOMEXP);
}

double ptzcurve(int v)
{
  return PTZMULT * pow(v, PTZEXP);
}

void debug(char c)
{
  Serial.print(c);
}

void debug(int n, int base)
{
  Serial.print(n, base);
  Serial.print(F(""));
}

void debug(uint8_t *buf, int len)
{
  for (uint8_t i = 0; i < len; i++)
  {
    uint8_t elem = buf[i];
    debug(elem, HEX);
  }
}

void send_bytes(uint8_t *b, int len)
{
  for (int i = 0; i < len; i++)
  {
    uint8_t elem = b[i];
    viscaSerial.println(elem);
  }
}

void send_visca(uint8_t *c, size_t len)
{
  int i = 0;
  uint8_t elem;
  do
  {
    elem = c[i++];
    viscaSerial.print(elem);
  } while (i < len && elem != 0xff);
  Serial.println(F("| VISCA IP->SER"));
}

void send_visca(const uint8_t *c)
{
  int i = 0;
  uint8_t elem;
  do
  {
    elem = c[i++];
    viscaSerial.print(elem);
  } while (elem != 0xff);
  Serial.println(F("| VISCA IP->SER"));
}

void visca_power(bool turnon)
{
  if (turnon)
  {
    Serial.println(F("| VISCA Power On"));
    send_visca(addr_set);
    delay(500);
    send_visca(pwr_on);
    delay(2000);
    send_visca(if_clear);
    delay(500);
    send_visca(irOff);
  }
  else
  {
    Serial.println(F("| VISCA Power Off"));
    send_visca(if_clear);
    delay(2000);
    send_visca(pwr_off);
  }
  pwr_is_on = turnon;
}

void handle_visca(uint8_t *buf, size_t len)
{
  uint8_t modified[16];
  size_t lastelement = 0;
  for (int i = 0; (i < len && i < 16); i++)
  {
    modified[i] = buf[i];
    lastelement = i;
  }

  // is this a PTZ?
  if (modified[1] == 0x01 && modified[2] == 0x06 && modified[3] == 0x01)
  {
    Serial.println(F("| PTZ CONTROL DETECTED... ADJUSTING SPEED"));
    modified[4] = (int)ptzcurve(modified[4]);
    modified[5] = (int)ptzcurve(modified[5]);
  }
  if (modified[1] == 0x01 && modified[2] == 0x04 && modified[3] == 0x07)
  {
    Serial.println(F("| ZOOM CONTROL DETECTED, ADJUSTING SPEED"));
    int zoomspeed = modified[4] & 0b00001111;
    zoomspeed = (int)zoomcurve(zoomspeed);
    int zoomval = (modified[4] & 0b11110000) + zoomspeed;
    modified[4] = zoomval;
  }

  Serial1.write(modified, lastelement + 1);
}

void start_visca()
{
  Serial.println(F("+--------------------+"));
  Serial.println(F("|    VISCA server    |"));
  Serial.println(F("+--------------------+"));
  Serial.print(F("| starting on UDP port "));
  Serial.println(settings.viscaport);
  udp.close(); // will close only if needed
  if (udp.listen(settings.viscaport))
  {
    udpstate = 1;
    Serial.println(F("| Server is Running!"));
    udp.onPacket([](AsyncUDPPacket packet) {
      // debug(packet);
      lastclientip = packet.remoteIP();
      lastclientport = packet.remotePort();

      Serial.print(F("| Type of UDP datagram: "));
      Serial.println(packet.isBroadcast() ? "Broadcast" : packet.isMulticast() ? "Multicast"
                                                                             : "Unicast");
      Serial.print(F("| Sender: "));
      Serial.print(lastclientip);
      Serial.print(F(":"));
      Serial.println(lastclientport);
      Serial.print(F("| Receiver: "));
      Serial.print(packet.localIP());
      Serial.print(F(":"));
      Serial.println(packet.localPort());
      Serial.print(F("| Message length: "));
      Serial.println(packet.length());
      Serial.print(F("| Payload (hex):"));
      debug(packet.data(), packet.length());
      Serial.println(F(""));

      handle_visca(packet.data(), packet.length());
    });
  }
  else
  {
    udpstate = 0;
    Serial.println(F("| Server failed to start"));
  }
  Serial.println(F("+---------------------"));
  Serial.println(F(""));
}

void check_serial()
{
  int available = viscaSerial.available();
  while (available > 0)
  {
    Serial.println(F("| VISCA SER -> IP"));
    int actual = viscaSerial.readBytesUntil(0xff, lastser_in, available); // does not include the terminator char
    if (actual < 16)
    {
      lastser_in[actual] = 0xff;
      actual++;
    }
    debug(lastser_in, actual);
    if (lastclientport > 0)
      udp.writeTo(lastser_in, actual, lastclientip, lastclientport);
      
    available = viscaSerial.available();
  }
}


void setup()
{
  Serial.begin(9600);
  EEPROM.begin(512);
  SPIFFS.begin();
  leds.begin();
  leds.setBrightness(50);
  leds.show();

  httpServer.on("/", HTTP_GET, rootPageHandler);
  httpServer.on("/save", HTTP_POST, handleSave);
  httpServer.on("/tally", HTTP_GET, tallyPageHandler);
  httpServer.on("/zend", [](){
    //httpServer.sendHeader("Connection", "close");
    httpServer.sendHeader("Cache-Control", "no-cache");
    httpServer.sendHeader("Access-Control-Allow-Origin", "*");
    
    if (currentState != oldState) {
      httpServer.send(200, "text/event-stream", "event: message\ndata: refresh"+String(vmixcon)+"\nretry: 500\n\n");
      oldState = currentState;
    } else {
      httpServer.send(200, "text/event-stream", "event: message\ndata: norefresh\nretry: 500\n\n");
    }
    
    //Serial.print(F("| FreeHeap: "));
    //Serial.println(ESP.getFreeHeap(),DEC);
  });
  httpServer.serveStatic("/", SPIFFS, "/", "max-age=315360000");
  httpServer.begin();

  banner();
  
  start();
}

void loop()
{
  httpServer.handleClient();

  while (client.available())
  {
    String data = client.readStringUntil('\r\n');
    handleData(data);
  }
  if (!client.connected() && !apEnabled && millis() > lastCheck + interval)
  {
    tallySetConnecting();

    client.stop();

    connectTovMix();
    lastCheck = millis();
  }

  check_serial();
}
