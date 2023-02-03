#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_BMP280.h>
#include <Wire.h>
#include <EEPROM.h>

#define HEATER 5
#define LED 2
#define BUTTON 12
#define ADDR 0
#define TEMP_CHECK_INTERVAL 3000
#define TEMP_SEND_INTERVAL 300000


const char* SSID = "SSID";
const char* PASSWORD = "PASSWORD";
const char* URL = "http://YOUR_IP/temp/";

float temp;
uint8_t temp_target;
uint32_t temp_check_timer = 0;
uint32_t temp_send_timer = 0;

Adafruit_BMP280 bmp;
HTTPClient http;


void setup() {
  pinMode(HEATER, OUTPUT);
  pinMode(LED, OUTPUT);
  pinMode(BUTTON, INPUT_PULLUP);
  
  Serial.begin(115200);
  EEPROM.begin(ADDR + 1);
  temp_target = EEPROM.read(ADDR);

  if (!bmp.begin(0x76)) {
    Serial.println("Couldn't find a valid BMP280 sensor, check wiring!");
    while (true);
  }
  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,Adafruit_BMP280::SAMPLING_X2,Adafruit_BMP280::SAMPLING_X16,Adafruit_BMP280::FILTER_X16,Adafruit_BMP280::STANDBY_MS_500);

  temp_update();
  network_update();
}

void loop() {
  if (millis() < temp_send_timer || millis() < temp_check_timer) {
    temp_send_timer = millis();
    temp_check_timer = millis();
  }

  if (millis() - temp_send_timer >= TEMP_SEND_INTERVAL || digitalRead(BUTTON) == LOW) {
    temp_send_timer += TEMP_SEND_INTERVAL;
    delay(50);
    while(digitalRead(BUTTON) == LOW) {
      digitalWrite(LED, HIGH);
    }
    digitalWrite(LED, LOW);
    network_update();
  }

  if (millis() - temp_check_timer >= TEMP_CHECK_INTERVAL) {
    temp_check_timer += TEMP_CHECK_INTERVAL;
    temp_update();
  }
}


void temp_update() {
  digitalWrite(LED, HIGH);
  delay(100);
  digitalWrite(LED, LOW);

  temp = bmp.readTemperature();

  Serial.print("\nTemp: ");
  Serial.print(temp);
  Serial.println(" °C");
  Serial.print("Temp target: ");
  Serial.print(temp_target);
  Serial.println(" °C\n");
  
  if (temp_target > 25) {
    temp_target = 25;
  }
  if (temp < temp_target) {
    digitalWrite(HEATER, LOW); // ON
  }
  else if (temp > temp_target + 1) {
    digitalWrite(HEATER, HIGH); // OFF
  }  
}


void network_update() {
  if(connect()) {
    http.begin(URL + String(temp));
    int response_code = http.GET();
    Serial.print("HTTP Response code: ");
    Serial.println(response_code);

    if (response_code > 0) {
      String data = http.getString();
      temp_target = data.toInt();
      if (EEPROM.read(ADDR) != temp_target) {
        EEPROM.write(ADDR, temp_target);
        EEPROM.commit();
      }
    }
    else {
      Serial.println("Connection couldn't be established");
    }
    http.end();
  }
}


bool connect() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }
  Serial.print("\nConnecting to ");
  Serial.println(SSID);
  WiFi.begin(SSID, PASSWORD);
  uint8_t counter = 0;  
  while (WiFi.status() != WL_CONNECTED && counter < 50) {
    digitalWrite(LED, HIGH);
    delay(100);
    digitalWrite(LED, LOW);
    delay(100);
    counter++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.println("\n");
    return true;
  }
  else {
    Serial.println("Failed to connect!");
    return false;
  }
}