/***************************************************
  Adafruit MQTT Library ESP8266 Example

  Must use ESP8266 Arduino from:
    https://github.com/esp8266/Arduino

  Works great with Adafruit's Huzzah ESP board & Feather
  ----> https://www.adafruit.com/product/2471
  ----> https://www.adafruit.com/products/2821

  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  Written by Tony DiCola for Adafruit Industries.
  MIT license, all text above must be included in any redistribution
 ****************************************************/
#include <ESP8266WiFi.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include "Adafruit_SSD1306.h"
#include <WEMOS_SHT3X.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

#define OLED_RESET 0  // GPIO0
Adafruit_SSD1306 display(OLED_RESET);
SHT3X sht30(0x45);  // sht30 address

float humidity = 0.00 ;
float temperature = 0.00 ;
String dataString;
char charBuf[100];
unsigned long previousMillis = 0; 
const long interval = 2000; 


/************************* WiFi Access Point *********************************/

#define WLAN_SSID       "WLAN_41"
#define WLAN_PASS       "XXXXXXXX"  // Access Point password

/************************* Adafruit.io Setup *********************************/

#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883                   // use 8883 for SSL
#define AIO_USERNAME    "1nformatica"
#define AIO_KEY         "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX" // Put Your adafruit ID here

/************ Global State (you don't need to change this!) ******************/

// Create an ESP8266 WiFiClient class to connect to the MQTT server.
WiFiClient client;
// or... use WiFiFlientSecure for SSL
//WiFiClientSecure client;

// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

/****************************** Feeds ***************************************/

// Setup a feed called 'temp' for publishing.
Adafruit_MQTT_Publish temp = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/Temp");

// Setup a feed called 'hum' for publishing.
Adafruit_MQTT_Publish hum = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/Humidity");

// Setup a feed called 'onoff' for subscribing to changes.
Adafruit_MQTT_Subscribe onoffbutton = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/Switch");

/*************************** Sketch Code ************************************/

// Bug workaround for Arduino 1.6.6, it seems to need a function declaration
// for some reason (only affects ESP8266, likely an arduino-builder bug).
void MQTT_connect();

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
  digitalWrite(LED_BUILTIN, HIGH);  // LED off
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x32 OLED)
  display.display(); 
  delay(10);

   // Connect to WiFi access point.
  Serial.println(); Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WLAN_SSID);

  WiFi.begin(WLAN_SSID, WLAN_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  Serial.println("WiFi connected");
  Serial.println("IP address: "); Serial.println(WiFi.localIP());

  // Setup MQTT subscription for onoff feed.
  mqtt.subscribe(&onoffbutton);
}

void getTemperature() {
  unsigned long currentMillis = millis();
  if(currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    sht30.get();
    humidity = (sht30.humidity);  
    temperature = (sht30.cTemp);
    }
}

void loop() {
  // Ensure the connection to the MQTT server is alive (this will make the first
  // connection and automatically reconnect when disconnected).  See the MQTT_connect
  // function definition further below.
  MQTT_connect();
  getTemperature(); // Call routine to get data from SHT30

  // this is our 'wait for incoming subscription packets' busy subloop
  // try to spend your time here

  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(5000))) {
    if (subscription == &onoffbutton) {
      Serial.print(F("Got: "));
      Serial.println((char *)onoffbutton.lastread);
       
    if (strcmp((char *)onoffbutton.lastread, "ON") == 0) {
        digitalWrite(LED_BUILTIN, LOW); 
      }
    if (strcmp((char *)onoffbutton.lastread, "OFF") == 0) {
        digitalWrite(LED_BUILTIN, HIGH); 
      }
    }
  }

  // Now we can publish temperature
  Serial.print(F("\nSending Temperature "));
  Serial.print(temperature);
  Serial.print("...");
  if (! temp.publish(temperature)) {
    Serial.println(F("Failed"));
  } else {
    Serial.println(F("OK!"));
  }

// Now we can publish humidity
  Serial.print(F("\nSending Humidity "));
  Serial.print(humidity);
  Serial.print("...");
  if (! hum.publish(humidity)) {
    Serial.println(F("Failed"));
  } else {
    Serial.println(F("OK!"));
  }
  // Show Temp and Humidity on OLED
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);

  display.print("Temperature ");
  display.print(sht30.cTemp);
  display.println(" *C");
  display.print("Humidity    ");
  display.print(sht30.humidity);
  display.println(" %");
  display.println("");
  display.setTextSize(1);
  display.println("1nformatica");
  display.display();

  // ping the server to keep the mqtt connection alive
  // NOT required if you are publishing once every KEEPALIVE seconds
  /*
  if(! mqtt.ping()) {
    mqtt.disconnect();
  }
  */
}

// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
       Serial.println(mqtt.connectErrorString(ret));
       Serial.println("Retrying MQTT connection in 5 seconds...");
       mqtt.disconnect();
       delay(5000);  // wait 5 seconds
       retries--;
       if (retries == 0) {
         // basically die and wait for WDT to reset me
         while (1);
       }
  }
  Serial.println("MQTT Connected!");
}
