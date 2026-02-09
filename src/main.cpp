#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <LittleFS.h>
#include "DHT.h"

#define DHTPIN 4
#define RELAYPIN 5
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);
ESP8266WebServer server(80);

float targetTemp = 23.0;
float currentTemp = 0;

void loadConfig()
{
  if (LittleFS.begin())
  {
    if (LittleFS.exists("/config.txt"))
    {
      File configFile = LittleFS.open("/config.txt", "r");
      if (configFile)
      {
        targetTemp = configFile.readString().toFloat();
        configFile.close();
      }
    }
  }
}

void saveConfig()
{
  File configFile = LittleFS.open("/config.txt", "w");
  if (configFile)
  {
    configFile.print(targetTemp);
    configFile.close();
  }
}

void handleRoot()
{
  currentTemp = dht.readTemperature();
  String html = "<html><head><meta charset='UTF-8' name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>body{text-align:center; font-family:sans-serif; background:#e0f7fa;} .card{background:white; padding:20px; border-radius:15px; display:inline-block; margin-top:50px; border: 2px solid #00acc1;}</style></head>";
  html += "<body><div class='card'><h1>wifi temperature controller: SmartConfig</h1>";
  html += "<h2>Temperature: " + String(currentTemp) + " °C</h2>";
  html += "<h3>Target: " + String(targetTemp) + " °C</h3>";
  html += "<p>Relay: <b style='color:" + String(digitalRead(RELAYPIN) ? "green" : "red") + "'>" + String(digitalRead(RELAYPIN) ? "ON" : "OFF") + "</b></p>";
  html += "<a href='/up'><button style='font-size:20px;'> + </button></a> ";
  html += "<a href='/down'><button style='font-size:20px;'> - </button></a>";
  html += "<br><br><small><a href='/reset'>Reset settings Wi-Fi</a></small>";
  html += "</div></body></html>";
  server.send(200, "text/html", html);
}

void setup()
{
  Serial.begin(115200);
  pinMode(RELAYPIN, OUTPUT);
  dht.begin();

  LittleFS.begin();
  loadConfig();

  WiFiManager wm;

  if (!wm.autoConnect("AutoConnectAP"))
  {
    Serial.println("Connection error. Reboot...");
    delay(3000);
    ESP.restart();
  }

  Serial.println("Connected! IP: " + WiFi.localIP().toString());

  server.on("/", handleRoot);
  server.on("/up", []()
            { targetTemp+=0.5;
    saveConfig();
    server.sendHeader("Location", "/");
    server.send(303); });
  server.on("/down", []()
            { targetTemp-=0.5;
    saveConfig();
    server.sendHeader("Location", "/");
    server.send(303); });
  server.on("/reset", [&wm]()
            { 
        wm.resetSettings();
        server.send(200, "text/plain", "Settings reset. Restart...");
        delay(2000);
        ESP.restart(); });

  server.begin();
}

void loop()
{
  server.handleClient();

  static unsigned long prevMillis = 0;
  if (millis() - prevMillis > 2000)
  {
    prevMillis = millis();
    currentTemp = dht.readTemperature();

    if (!isnan(currentTemp))
    {
      if (currentTemp < targetTemp)
      {
        digitalWrite(RELAYPIN, HIGH);
      }
      else if (currentTemp > targetTemp + 0.3)
      {
        digitalWrite(RELAYPIN, LOW);
      }
    }
  }
}
