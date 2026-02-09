# ESP8266 Wi-Fi temperature controller

with a web interface and settings memory.

## Features

- Wi-Fi setup via WiFiManager (access point).
- Temperature maintenance (0.3°C hysteresis).
- Target temperature storage in LittleFS.

## Assembly

1. Open the project in VS Code + PlatformIO.
2. Flash the ESP8266.
3. Connect to the `SmartThermostatAP` network to set up Wi-Fi.

![Schema](Schematic_wifi_temperature_controller.png)
