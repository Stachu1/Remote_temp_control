#include <WiFi.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include <EEPROM.h>

#define EEPROM_OFFSET 0
#define LED 2
#define TEMP_TARGET_LOCK_SWITCH 15

const char* SSID = "SSID";
const char* PASSWORD = "PASSWORD";
const char* TIME_SERVER_URL = "http://worldtimeapi.org/api/timezone/Europe/Warsaw";
const char* FAVICON_URL = "https://www.cprogramming.com/favicon.ico";
const char DEG = 186;

const int TIMEOUT = 2000;
const int UPDATE_TIME_INTERVAL = 43200000;

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
  IPAddress target_set_IP;
};

Data data;

float delta_temp = -999;


void setup() {
  pinMode(LED, OUTPUT);
  pinMode(TEMP_TARGET_LOCK_SWITCH, INPUT_PULLUP);

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
      digitalWrite(LED, HIGH);
      handle_client(client);
      digitalWrite(LED, LOW);
    }
  }
  else {
    connect();
  }

  timer_update();
}


void timer_update() {
  if (millis() < time_update_millis) {
    time_update_millis = millis();
  }

  if ((millis() - time_update_millis) > UPDATE_TIME_INTERVAL) {
    time_update_millis += UPDATE_TIME_INTERVAL;
    int32_t t = get_current_time();
    if (t >= 0) {
      updated_time = t;
      current_time = t;
    }
    else {
      updated_time = current_time;
    }
  }
  else {
    current_time = updated_time + (millis() - time_update_millis) / 1000;
  }

  if (current_time == 86400) {
    data.temp_max = data.temp;
    data.temp_min = data.temp;
    updated_time = 0;
    current_time = 0;
    time_update_millis = millis();
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

void save_temp_target(String header, WiFiClient client) {
  String temp_string = header.substring(header.indexOf("tempTarget") + 11, header.length());
  temp_string = temp_string.substring(0, temp_string.indexOf(" "));
  data.temp_target = temp_string.toFloat();
  data.target_set_IP = client.remoteIP();
  EEPROM.put(EEPROM_OFFSET, data);
  EEPROM.commit();
}


void save_temp(String header) {
  delta_temp = -data.temp;
  String temp_string = header.substring(header.indexOf("temp") + 5, header.length());
  temp_string = temp_string.substring(0, temp_string.indexOf(" "));
  data.temp = temp_string.toFloat();
  delta_temp += data.temp;

  data.temp_updated_time = current_time;
  if (data.temp > data.temp_max) {
    data.temp_max = data.temp;
  }
  if (data.temp < data.temp_min) {
    data.temp_min = data.temp;
  }
  EEPROM.put(EEPROM_OFFSET, data);
  EEPROM.commit();
}


void handle_client(WiFiClient client) {
  uint32_t connection_start_millis = millis();
  String header = "";
  String currentLine = "";
  char c;
  while (client.connected() && millis() - connection_start_millis <= TIMEOUT) {
    if (client.available()) {
      c = client.read();
      header += c;
      if (c == '\n') {
        if (currentLine.length() == 0) {
          client.println("HTTP/1.1 200 OK");
          client.println("Content-type:text/html");
          client.println("Connection: close");
          client.println();

          Serial.print("\nNew Client\nIP: ");
          Serial.println(client.remoteIP());

          if (header.indexOf("tempTarget") >= 0) {
            if (digitalRead(TEMP_TARGET_LOCK_SWITCH) == HIGH) {
              save_temp_target(header, client);

              client.print(data.temp_target);
              Serial.print("Temp target set to: ");
              Serial.println(data.temp_target);
            }
            else {
              client.print("Temp target is locked at ");
              client.print(data.temp_target);
              Serial.print("Temp target locked at: ");
              Serial.println(data.temp_target);
            }
          }

          else if (header.indexOf("temp") >= 0 ) {
            save_temp(header);

            client.print(data.temp_target);
            Serial.print("Current temp: ");
            Serial.println(data.temp);
          }

          else {
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=2\">");
            client.print("<link rel=\"icon\" href=\"");
            client.print(FAVICON_URL);
            client.println("\" sizes=\"16x16\" type=\"image/png\">");
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println("</style></head>");
            client.println("<body style='background-color:#1c1c1c;'><h1 style='color:white'>Temp control</h1>");

            client.println("<br>");
            client.println("<h3 style='color:white'>Current temp: " + String(data.temp) + " " + DEG + "C</h3>");
            if (delta_temp != -999) {
              if (delta_temp >= 0.0) {
                client.println("<h3 style='color:red'>Temp diff: " + String(delta_temp) + " " + DEG + "C</h3>");
              }
              else {
                client.println("<h3 style='color:#008cff'>Temp diff: " + String(delta_temp) + " " + DEG + "C</h3>");
              }
            }
            client.println("<br>");
            client.println("<h3 style='color:red'>Temp max: " + String(data.temp_max) + " " + DEG + "C</h3>");
            client.println("<h3 style='color:#008cff'>Temp min: " + String(data.temp_min) + " " + DEG + "C</h3>");
            client.println("<br>");
            client.println("<h3 style='color:white'>Updated: " + seconds_to_time_string(data.temp_updated_time) + "</h3>");
            client.println("<br>");
            client.println("<h3 style='color:white'>Temp target: " + String(data.temp_target) + " " + DEG + "C</h3>");
            client.println("<h3 style='color:white'>Set from: " + String(data.target_set_IP[0]) + "." + String(data.target_set_IP[1]) + "." + String(data.target_set_IP[2]) + "." + String(data.target_set_IP[3]) + "</h3>");
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
  client.stop();
}
