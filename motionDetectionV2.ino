#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         // https://github.com/tzapu/WiFiManager
#include <ESP8266HTTPClient.h>

// Set web server port number to 80
WiFiServer server(80);

// Variable to store the HTTP request
String header;

int pirPin = D4; // Input for HC-S501
int pirPin2 = D5; // Input for HC-S501
int ledPin = D1;
int ledPin2 = D2;
const int buzzer = D6; //buzzer to arduino pin 9

// default value
int ledOn = 1;
int buzzerOn = 1;

void setup() {
	Serial.begin(115200);

	WiFiManager wifiManager;

	// Uncomment and run it once, if you want to erase all the stored information
	//wifiManager.resetSettings();

	// set custom ip for portal
	wifiManager.setAPStaticIPConfig(IPAddress(10,0,1,1), IPAddress(10,0,0,1), IPAddress(255,255,255,0));
	wifiManager.autoConnect("MemeBubble");
	Serial.println("Connected.");

	server.begin();

	// Initialize pins
	pinMode(BUILTIN_LED, OUTPUT);
	pinMode(buzzer, OUTPUT);
	pinMode(ledPin, OUTPUT);
	pinMode(ledPin2, OUTPUT);
	pinMode(pirPin, INPUT);
	pinMode(pirPin2, INPUT);

	//initial leds OFF
	digitalWrite(BUILTIN_LED, LOW);
	ledOn = 1;
	buzzerOn = 1;
}

void loop() {
	// When motion sensor is high, send get request to azure function and blink led/speaker
	// Leds blink for 20secs to avoid sending too many requests
	int pirValue = digitalRead(pirPin);
	int pirValue2 = digitalRead(pirPin2);

	// Lower sensor found a motion > It's meme
	if(pirValue == HIGH && pirValue2 == LOW) {
		sendMsg("opendoor=1");
		notify();

		// Turn off all leds and speaker
		digitalWrite(ledPin, LOW);
		digitalWrite(ledPin2, LOW);
		noTone(buzzer);
	}

	// Both sensors found a motion > It's something else
	else if(pirValue == HIGH && pirValue2 == HIGH) {
		sendMsg("opendoor2=1");
		notify();

		// Turn off all leds and speaker
		digitalWrite(ledPin, LOW);
		digitalWrite(ledPin2, LOW);
		noTone(buzzer);
	}
  
  delay(1000);
}

// Leds blink and buzzer buzz fpr 20 secs
void notify() {
	for (int i = 0; i < 20; i++) {
		if(ledOn) {
			digitalWrite(ledPin, HIGH);
			digitalWrite(ledPin2, LOW);
		}
		if(buzzerOn)
			tone(buzzer, 1000);
		delay(500);
		  
		if(ledOn) {
			digitalWrite(ledPin, LOW);
			digitalWrite(ledPin2, HIGH);
		}
		if(buzzerOn)
			noTone(buzzer);
		delay(500);
	}
}

// Send GET request to azure function
void sendMsg(String msgParam)
{
   if (WiFi.status() == WL_CONNECTED) { //Check WiFi connection status
    HTTPClient http;  //Declare an object of class HTTPClient
    http.begin("http://memebubble.azurewebsites.net/api/SendOpenDoorMsgHttp?" + msgParam);  //Specify request destination
    int httpCode = http.GET();                                                                  //Send the request
 
    if (httpCode > 0) { //Check the returning code 
      String payload = http.getString();   //Get the request response payload
      Serial.println(payload);                     //Print the response payload
    }
 
    http.end();   //Close connection
  }
}
