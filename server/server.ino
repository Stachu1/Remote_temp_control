#include <WiFi.h>

#define LED_PIN 2
#define SSID "SSID"
#define PASSWORD "PASSWORD"
#define TIMEOUT_TIME 2000

WiFiServer server(80);



void setup() {
  pinMode(LED_PIN, OUTPUT);
  
  Serial.begin(115200);

  connect();
  server.begin();
}

void loop() {
  clienHandle();
}


void clienHandle() {
  WiFiClient client = server.available();

  if (client) {   
    unsigned long time_start = millis();

    Serial.println("New Client.");

    String header = "";
    String currentLine = "";
    while (client.connected() && millis() - time_start <= TIMEOUT_TIME) {
      if (client.available()) {
        char c = client.read();
        header += c;
        if (c == '\n') {
          if (currentLine.length() == 0) {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();
            
            client.println("Hello world!");
            
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
    Serial.println("Client disconnected.");
    Serial.println("");
  }
}


void connect() {
  WiFi.begin(SSID, PASSWORD);

  Serial.print("Connecting to ");
  Serial.println(SSID);
  int counter = 0;
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    delay(100);
    Serial.print(".");
    counter++;
    if (counter >= 150) {
      Serial.println("\nFailure!\nretrying in 10s...");
      delay(10000);
      WiFi.begin(SSID, PASSWORD);
      Serial.print("Connecting");
      counter = 0;
    }
  }
  Serial.print("\nConnected to ");
  Serial.println(SSID);
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  delay(500);
}
