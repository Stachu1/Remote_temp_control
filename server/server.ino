#include <WiFi.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include <EEPROM.h>

#define EEPROM_OFFSET 0
#define LED 2

const char* SSID = "SSID";
const char* PASSWORD = "PASSWORD";
const char DEG = 186;

const char* TIME_SERVER_URL = "http://worldtimeapi.org/api/timezone/Europe/Warsaw";

const int TIMEOUT = 2000;
const int GET_TIME_DELAY = 3600000;

WiFiServer server(80);
HTTPClient http;


uint32_t time_update_millis;
uint32_t updated_time;
uint32_t current_time;


struct Data {
  float temp;
  float temp_target;
  float temp_min;
  float temp_max;
  uint32_t temp_updated_time;
};

Data data;


void setup() {
  pinMode(LED, OUTPUT);

  Serial.begin(115200);
  EEPROM.begin(sizeof(struct Data) + EEPROM_OFFSET);
  connect();
  server.begin();

  EEPROM.get(EEPROM_OFFSET, data);
  updated_time = get_current_time();
  time_update_millis = millis();
  current_time = updated_time;
}


void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client = server.available();
    if (client) {
      handle_client(client);
    }
  }
  else {
    connect();
  }

  if ((millis() - time_update_millis) > GET_TIME_DELAY) {
    int32_t t = get_current_time();
    if (t >= 0) {
      updated_time = t;
      time_update_millis = millis();
      current_time = updated_time;
    }
    else {
      updated_time = current_time;
      time_update_millis = millis();
    }
  }
  else {
    current_time = updated_time + (millis() - time_update_millis) / 1000;
  }

  if (current_time == 86400) {
    data.temp_max = data.temp;
    data.temp_min = data.temp;
    updated_time = 0;
    current_time = updated_time;
  }
}


int32_t get_current_time() {
  http.begin(TIME_SERVER_URL);
  int httpResponseCode = http.GET();
  if (httpResponseCode > 0) {
    Serial.print("\nTime API HTTP response code: ");
    Serial.println(httpResponseCode);
    String payload = http.getString();

    JSONVar json = JSON.parse(payload);
    if (JSON.typeof(json) == "undefined") {
      Serial.println("Parsing input failed!");
      return -1;
    }

    String datetime = JSON.stringify(json["datetime"]);
    String time = datetime.substring(12, 20);
    Serial.print("Current time: ");
    Serial.println(time);

    uint32_t time_seconds = time_string_to_seconds(time);
    return time_seconds;
  }
  else {
    Serial.print("\nTime API HTTP response code: ");
    Serial.println(httpResponseCode);
    return -1;
  }
}


String seconds_to_time_string(int seconds) {
  int minutes = seconds / 60;
  int hours = minutes / 60;
  seconds %= 60;
  minutes %= 60;
  hours %= 24;
  String time = "";
  if (hours < 10) {
    time += "0";
  }
  time += String(hours, DEC);
  time += ":";
  if (minutes < 10) {
    time += "0";
  }
  time += String(minutes, DEC);
  time += ":";
  if (seconds < 10) {
    time += "0";
  }
  time += String(seconds, DEC);
  return time;
}


uint32_t time_string_to_seconds(String time) {
  uint8_t hours = time.substring(0,2).toInt();
  uint16_t minutes = time.substring(3,5).toInt();
  uint16_t seconds = time.substring(6,8).toInt();
  uint32_t total_sec = hours * 3600 + minutes * 60 + seconds;
  return total_sec;
}


void connect() {
  Serial.print("\nConnecting to ");
  Serial.println(SSID);
  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED, HIGH);
    delay(100);
    digitalWrite(LED, LOW);
    delay(100);
  }
  Serial.println("Connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("\n");
}


void handle_client(WiFiClient client) {
  int current_millis = millis();
  int previous_millis = current_millis;
  String header;
  String currentLine = "";
  char c;
  while (client.connected() && current_millis - previous_millis <= TIMEOUT) {
    current_millis = millis();
    if (client.available()) {
      c = client.read();
      header += c;
      if (c == '\n') {
        if (currentLine.length() == 0) {
          client.println("HTTP/1.1 200 OK");
          client.println("Content-type:text/html");
          client.println("Connection: close");
          client.println();
          
          if (header.indexOf("favicon") >= 0) {
            client.println();
            break;
          }

          Serial.print("\nNew Client IP: ");
          Serial.println(client.remoteIP());

          if (header.indexOf("tempTarget") >= 0) {
            String temp_string = header.substring(header.indexOf("tempTarget") + 11, header.length());
            temp_string = temp_string.substring(0, temp_string.indexOf(" "));
            data.temp_target = temp_string.toFloat();
            EEPROM.put(EEPROM_OFFSET, data);
            EEPROM.commit();

            client.print(data.temp_target);
            Serial.print("Temp target set to: ");
            Serial.println(data.temp_target);
          }

          else if (header.indexOf("temp") >= 0 ) {
            String temp_string = header.substring(header.indexOf("temp") + 5, header.length());
            temp_string = temp_string.substring(0, temp_string.indexOf(" "));
            data.temp = temp_string.toFloat();
            data.temp_updated_time = current_time;
            if (data.temp > data.temp_max) {
              data.temp_max = data.temp;
            }
            if (data.temp < data.temp_min) {
              data.temp_min = data.temp;
            }
            EEPROM.put(EEPROM_OFFSET, data);
            EEPROM.commit();

            client.print(data.temp_target);
            Serial.print("Current temp: ");
            Serial.println(data.temp);
          }

          else {
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");

            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println("text-decoration: none; font-size: 60px; margin: 2px; cursor: pointer;");
            client.println("</style></head>");
            
            client.println("<body style='background-color:#1c1c1c;'><h1 style='color:white'>Temp control</h1>");
            
            
            client.println("<p style='color:white'>Current temp: " + String(data.temp) + " " + DEG + "C</p>");
            client.println("<p style='color:red'>Temp max: " + String(data.temp_max) + " " + DEG + "C</p>");
            client.println("<p style='color:cyan'>Temp min: " + String(data.temp_min) + " " + DEG + "C</p>");
            client.println("<p style='color:white'>Updated: " + seconds_to_time_string(data.temp_updated_time) + "</p>");
            client.println("<br>");
            client.println("<p style='color:white'>Temp target: " + String(data.temp_target) + " " + DEG + "C</p>");
            client.println("</body></html>");
          }
          
          client.println();
          break;
        }
        else {
          currentLine = "";
        }
      }
      else if (c != '\r') {
      currentLine += c;
      }
    }
  }
  
  header = "";
  client.stop();
}