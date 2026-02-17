#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <LittleFS.h>
#include <PubSubClient.h>
#include "DHT.h"

#define DHTPIN 4
#define RELAYPIN 5
#define DHTTYPE DHT11
#define MAX_RECORDS 100

DHT dht(DHTPIN, DHTTYPE);
WiFiClient espClient;
PubSubClient mqttClient(espClient);
ESP8266WebServer server(80);

const char* mqtt_server = "broker.emqx.io";
const int mqtt_port = 1883;

unsigned long lastRead = 0;
unsigned long lastMqttRetry = 0;
unsigned long lastSave = 0;

float targetTemp = 23.0;
float currentTemp = 0;
float currentHum = 0;
bool relayState = false;

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

void saveTemperature(float temp) {
  File file = LittleFS.open("/history.csv", "a");
  if (file) {
    file.println(String(millis()) + "," + String(temp));
    file.close();
  }
}

void trimHistory() {
  File file = LittleFS.open("/history.csv", "r");
  if (!file) return;

  String lines[MAX_RECORDS];
  int count = 0;

  while (file.available() && count < MAX_RECORDS) {
    lines[count++] = file.readStringUntil('\n');
  }
  file.close();

  if (count >= MAX_RECORDS) {
    File newFile = LittleFS.open("/history.csv", "w");
    for (int i = count - 50; i < count; i++) {
      newFile.println(lines[i]);
    }
    newFile.close();
  }
}

void reconnectMQTT() {

if (!mqttClient.connected() && (millis() - lastMqttRetry > 5000)) {
    lastMqttRetry = millis();
    Serial.println("Attempting MQTT connection...");
    if (mqttClient.connect("ESP8266_Thermostat_Client")) {
      Serial.println("MQTT Connected");
    }
  }
}

void handleHistory() {
  File file = LittleFS.open("/history.csv", "r");
  if (!file) {
    server.send(200, "text/plain", "");
    return;
  }
  String content = file.readString();
  file.close();
  server.send(200, "text/plain", content);
}

void checkHistorySize() {
  File file = LittleFS.open("/history.csv", "r");
  if (file && file.size() > 5000) { 
    file.close();
    LittleFS.remove("/history.csv");
  }
}


void handleRoot()
{
  currentTemp = dht.readTemperature();

  String html = "<html><head><meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<script src='https://www.gstatic.com/charts/loader.js'></script>";
  html += "<script src='https://unpkg.com/mqtt/dist/mqtt.min.js'></script>";
  html += "<style>";
  html += "body{text-align:center;font-family:sans-serif;background:#e0f7fa;}";
  html += ".card{background:white;padding:20px;border-radius:15px;display:inline-block;margin-top:20px;border:2px solid #00acc1;}";
  html += "</style></head><body>";

  html += "<div class='card'>";
  html += "<h1>WiFi Temperature Controller</h1>";
  html += "<h2>Current: " + String(currentTemp) + " °C</h2>";
  html += "<h3>Target: " + String(targetTemp) + " °C</h3>";
  bool isOn = digitalRead(RELAYPIN);
  html += "<p>Relay: <b style='color:" + String(digitalRead(RELAYPIN) ? "green" : "red") + "'>";
  html += String(digitalRead(RELAYPIN) ? "ON" : "OFF") + "</b></p>";
  html += "<a href='/up'><button style='font-size:20px;'> + </button></a> ";
  html += "<a href='/down'><button style='font-size:20px;'> - </button></a>";
  html += "<br><br><small><a href='/reset'>Reset WiFi</a></small>";
  html += "</div>";

  html += "<div id='chart_div' style='width:100%; height:400px;'></div>";

  html += "<script>";
  html += "let target=" + String(targetTemp) + ";";
  html += "google.charts.load('current',{packages:['corechart']});";
  html += "google.charts.setOnLoadCallback(initChart);";
  html += "let chart; let data;";
  html += "function initChart(){";
  html += "data=new google.visualization.DataTable();";
  html += "data.addColumn('string','Time');";
  html += "data.addColumn('number','Temp');";
  html += "data.addColumn('number','Target');";
  html += "chart=new google.visualization.LineChart(document.getElementById('chart_div'));";

  html += "fetch('/history').then(r=>r.text()).then(t=>{";
  html += "let rows=t.split('\\n');";
  html += "rows.forEach(r=>{if(r){let p=r.split(',');";
  html += "let time=new Date(parseInt(p[0])).toLocaleTimeString();";
  html += "data.addRow([time,parseFloat(p[1]),target]);}});";
  html += "drawChart();});}";

  html += "function drawChart(){";
  html += "chart.draw(data,{title:'Temperature History',curveType:'function',legend:{position:'bottom'}});}";

  html += "const client=mqtt.connect('wss://broker.emqx.io:8884/mqtt');";
  html += "client.on('connect',()=>{client.subscribe('home/temperature');});";
  html += "client.on('message',(topic,message)=>{";
  html += "let temp=parseFloat(message.toString());";
  html += "let time=new Date().toLocaleTimeString();";
  html += "data.addRow([time,temp,target]);";
  html += "if(data.getNumberOfRows()>30){data.removeRow(0);} drawChart();});";

  html += "</script></body></html>";
  server.send(200, "text/html", html);
}

void setup()
{
  Serial.begin(115200);
  pinMode(RELAYPIN, OUTPUT);
  digitalWrite(RELAYPIN, LOW);
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

  mqttClient.setServer(mqtt_server, mqtt_port);

  server.on("/", handleRoot);
  server.on("/history", handleHistory);

  server.on("/up", []() {
    targetTemp += 0.5;
    saveConfig();
    server.sendHeader("Location", "/");
    server.send(303);
  });

  server.on("/down", []() {
    targetTemp -= 0.5;
    saveConfig();
    server.sendHeader("Location", "/");
    server.send(303);
  });

  server.on("/reset", [&wm]() {
    wm.resetSettings();
    server.send(200, "text/plain", "Reset...");
    delay(2000);
    ESP.restart();
  });


  server.begin();
}

void loop()
{
server.handleClient();
  reconnectMQTT();
  mqttClient.loop();

  unsigned long now = millis();
  if (now - lastRead > 2000) { 
    lastRead = now;
    float t = dht.readTemperature();

    if (!isnan(t)) {
      currentTemp = t;

      // Публикация MQTT
      char buf[8];
      dtostrf(currentTemp, 1, 1, buf);
      mqttClient.publish("home/temperature", buf);

      if (now - lastSave > 600000 || lastSave == 0) {
        lastSave = now;
        saveTemperature(currentTemp);
        checkHistorySize();
      }

      
      if (currentTemp < targetTemp) {
        digitalWrite(RELAYPIN, HIGH); 
      } else if (currentTemp > (targetTemp + 0.3)) {
        digitalWrite(RELAYPIN, LOW);
      }
    }
  }  
  
}
