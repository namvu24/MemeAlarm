#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

#define SLACK_SSL_FINGERPRINT "xxx" 
#define SLACK_BOT_TOKEN "xoxxx"
#define THING_SPEAK_SERVER "api.thingspeak.com"

String sendNotificationAPI = "XXX";

char ssid[] = "xxx";
char pass[] = "xxx";
WiFiClient client;
const char * server = "api.thingspeak.com";
ESP8266WiFiMulti WiFiMulti;
WebSocketsClient webSocket;

int pirPin = D4; // Input for HC-S501
int ledPin = D1;
int ledPin2 = D2;
const int buzzer = D6; //buzzer to arduino pin 9

int ledOn = 1;
int buzzerOn = 1;

unsigned long lastConnectAttempt = 0;

void setup() {
    Serial.begin(115200); 
    pinMode(buzzer, OUTPUT);

    pinMode(BUILTIN_LED, OUTPUT);
    pinMode(ledPin, OUTPUT);
    pinMode(ledPin2, OUTPUT);
    pinMode(pirPin, INPUT);

    //initial leds OFF
    digitalWrite(BUILTIN_LED, LOW);
    ledOn = 1;
    buzzerOn = 1;

    //START CONNECTION TO WIFI
    WiFi.begin(ssid, pass);
    Serial.println("Start");

    //Connecting
    while (WiFi.status() != WL_CONNECTED) 
    { 
        Serial.println("Waiting for connection");
        digitalWrite(ledPin, HIGH);
        tone(buzzer, 2000);
        delay(250);
        digitalWrite(ledPin, LOW);
        noTone(buzzer);
        delay(250);
    }

    //Connected succesfuly
    digitalWrite(ledPin, HIGH);
    digitalWrite(ledPin2, HIGH);
    tone(buzzer, 2000);
    delay(1000);
    digitalWrite(ledPin, LOW);
    digitalWrite(ledPin2, LOW);
    noTone(buzzer);
    
    connectToSlack();
}

unsigned long lastPing = 0;
unsigned long lastNoti = 0;
unsigned long lastCall = 0;
int foundMotion = 0;

void loop() 
{
   webSocket.loop();
   int pirValue = digitalRead(pirPin);

    if (WiFi.status() == WL_CONNECTED)  
    {
        if (millis() - lastPing > 5000) 
        {
            sendPing();
            lastPing = millis();
        }
    }
    else
    {
      connectToSlack();
    }
   
   if(pirValue == HIGH)
   {
      if (millis() - lastCall > 15000) 
      {
          callThingSpeakAPI(sendNotificationAPI);
          lastCall = millis();
      }
      foundMotion = 1;
   }
   else
   {
        if (foundMotion == 1 && (millis() - lastNoti > 15000))
        {
            foundMotion = 0;
            lastNoti = millis();
            digitalWrite(ledPin, LOW);
            digitalWrite(ledPin2, LOW);
            noTone(buzzer);
        }
   }

   if(foundMotion)
      notify();
}

unsigned long lastLedNoti = 0;
void notify()
{
    if(ledOn)
    {
      digitalWrite(ledPin, HIGH);
      digitalWrite(ledPin2, LOW);
    }
    if(buzzerOn)
      tone(buzzer, 1000);
    delay(500);
      
    if(ledOn)
    {
      digitalWrite(ledPin, LOW);
      digitalWrite(ledPin2, HIGH);
    }
    if(buzzerOn)
      noTone(buzzer);
    lastLedNoti = millis();
    delay(500);
}

void callThingSpeakAPI(String apiStr)
{   
  if (WiFi.status() == WL_CONNECTED) 
   {      
      if (client.connect(THING_SPEAK_SERVER, 80)) 
      { 
        Serial.println("Calling: " + apiStr);       
        client.println("GET /apps/thinghttp/send_request?api_key=" + apiStr + " HTTP/1.1");
        client.println("Host: api.thingspeak.com");
        client.println("Connection: close");
        client.println();
        client.stop();
        Serial.println("Request sent!");
      }
    }
}


void webSocketEvent(WStype_t type, uint8_t *payload, size_t len) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[WebSocket] Disconnected :( \n");
      connectToSlack();
      break;

    case WStype_CONNECTED:
      Serial.printf("[WebSocket] Connected to: %s\n", payload);
      sendPing();
      break;

    case WStype_TEXT:
      Serial.printf("[WebSocket] Message: %s\n", payload);
      String msg = readSlackMsg((char*)payload);
      processSlackMsg(msg);
      break;
  }
}

bool connectToSlack() {
  // Step 1: Find WebSocket address via RTM API (https://api.slack.com/methods/rtm.start)
  HTTPClient http;
  http.begin("https://slack.com/api/rtm.start?token=" SLACK_BOT_TOKEN, SLACK_SSL_FINGERPRINT);
  int httpCode = http.GET();

  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("HTTP GET failed with code %d\n", httpCode);
    return false;
  }

  WiFiClient *client2 = http.getStreamPtr();
  client2->find("wss:\\/\\/");
  String host = client2->readStringUntil('\\');
  String path = client2->readStringUntil('"');
  path.replace("\\/", "/");

  // Step 2: Open WebSocket connection and register event handler
  Serial.println("WebSocket Host=" + host + " Path=" + path);
  webSocket.beginSSL(host, 443, path, "", "");
  webSocket.onEvent(webSocketEvent);
  return true;
}

String readSlackMsg(char* jsonText)
{
  DynamicJsonBuffer jsonBuffer;

  // Parse JSON object
  JsonObject& root = jsonBuffer.parseObject(jsonText);
  if (!root.success()) {
    Serial.println(F("Parsing failed!"));
    return "";
  }
  String msgType = root["type"];
  if(msgType == "message")
  {
    String msgText = root["text"];
    Serial.println("[Slack] Message: " + msgText);
    return msgText;
  }
  
  return "";
}

void processSlackMsg(String msg)
{
  msg.toLowerCase();
  if(msg.indexOf("led on") >= 0)
  {
    ledOn = 1;
    sendMsg("Led is on", "CAS6V4J87");
  }
  else if(msg.indexOf("led off") >= 0)
  {
    ledOn = 0;
    sendMsg("Led is off", "CAS6V4J87");
  }
  else if(msg.indexOf("buzzer on") >= 0)
  {
    buzzerOn = 1;
    sendMsg("Buzzer is on", "CAS6V4J87");
  }
  else if(msg.indexOf("buzzer off") >= 0)
  {
    buzzerOn = 0;
    sendMsg("Buzzer is off", "CAS6V4J87");
  }
  else if(msg.indexOf("status") >= 0)
  {
    sendMsg("Everything is working", "CAS6V4J87");
  }
}
unsigned long nextMsg = 0;
void sendMsg(String msg, String channel) 
{
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["type"] = "message";
  root["id"] = nextMsg++;
  root["channel"] = channel;
  root["text"] = msg;
  String json;
  root.printTo(json);
  webSocket.sendTXT(json);
}

unsigned long nextCmdId = 0;
void sendPing() 
{
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["type"] = "ping";
  root["id"] = nextCmdId++;
  String json;
  root.printTo(json);
  webSocket.sendTXT(json);
}

