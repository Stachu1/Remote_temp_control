# Remote tmperature control with ESP32


## Configuration
1. Clone the repo
```bash
git clone https://github.com/Stachu1/Remote_temp_control.git
```

2. Set up the WiFi and relay pin
Server
```C
const char* SSID = "SSID";
const char* PASSWORD = "PASSWORD";
```
Client
```C
#define HEATER 5
const char* SSID = "SSID";
const char* PASSWORD = "PASSWORD";
const ch
ar* URL = "http://ServerIP/temp/"
```
3. Upload the code to your ESP32s

5. Check if everything works by typing the IP addres of the server ESP32 in your webbrowser

## Usage
Set the temperature target 
```
http://SERVERIP/tempTarget/VALUE
```

Current temperature and other information
```
http://SERVERIP/
```

## Server's UI
<img width="1391" alt="rtc_gui" src="https://github.com/Stachu1/Remote_temp_control/assets/77758413/8e506278-09d3-415e-91ee-5adcb50d873e">
