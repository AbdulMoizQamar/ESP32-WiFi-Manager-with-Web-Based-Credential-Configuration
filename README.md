# ESP32-WiFi-Manager-with-Web-Based-Credential-Configuration
This project implements a WiFi Manager for the ESP32 using Access Point (AP) mode and an embedded HTTP server. 

📝 Description:
This project implements a WiFi Manager for the ESP32 using Access Point (AP) mode and an embedded HTTP server. When the device starts, it:
Creates a WiFi Access Point (ESP32_Config)
Hosts a web page at 192.168.4.1 to enter SSID and Password
Receives credentials via a POST request
Connects to the specified WiFi network
Uses Non-Volatile Storage (NVS) to retain WiFi credentials after reboot

✅ Features:
SoftAP mode for initial configuration
HTML form for entering WiFi credentials
Handles HTTP GET and POST requests
Logs SSID and password for debugging
Connects to STA mode after receiving credentials
Easy way to update credentials without re-flashing

📦 Requirements:
ESP32 board
ESP-IDF development environment (or PlatformIO)
WiFi connection
Serial Monitor for debug logs
