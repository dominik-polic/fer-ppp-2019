
//Include required libraries
//DHT11
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
//Sensor network
#include <RF24Network.h>
#include <RF24.h>
#include <SPI.h>


//Hardcoded values
//Sensor to gateway types
#define TYPE_DHT11 1 //Temperature and humidity sensor
#define TYPE_TEMPERATURE 2 //Temperature only
#define TYPE_HUMIDITY 3 //Humidity only
#define TYPE_LIGHT 4 //Light sensor
#define TYPE_BMP280 5 //Temperature and pressure sensor
#define TYPE_SWITCH 6 //Digital switch
#define TYPE_BUTTON 7 //Pushbutton
#define TYPE_MOTION 8 //Motion sensor
#define TYPE_LIGHT_BTN 9 //Pushbutton and incoming command light

//Priority definitions
#define PRIORITY_LOW 1
#define PRIORITY_NORMAL 2
#define PRIORITY_HIGH 3
#define PRIORITY_INSTANT_ONLY 4 //WIth this priority data is only sent if the connection is currently available, otherwise it's discarded. Useful for button presses that are only relevant at one specific moment.

#define GATEWAY_ADDRESS 00 //nRF24Network address of the gateway
#define CHANNEL 90
#define UNAVAILABLE_VALUE 9999;

#define DEBUG true

//Standard packet structure
struct {                  
  uint16_t type = UNAVAILABLE_VALUE;
  uint16_t priority = UNAVAILABLE_VALUE;
  float temperature = UNAVAILABLE_VALUE;
  float humidity = UNAVAILABLE_VALUE;
  float pressure = UNAVAILABLE_VALUE;
  float light = UNAVAILABLE_VALUE;
  boolean switchActive = false;
  boolean buttonPressed = false;
  boolean motionDetected = false;  
} sendData;


//Define this device
const uint16_t this_priority = PRIORITY_NORMAL;
const uint16_t this_type = TYPE_DHT11;
const uint16_t this_node = 01;   //Address of this node    
const uint16_t other_node = GATEWAY_ADDRESS;  
const int measure_interval = 20000; //This is in ms

//nRF24 module config
RF24 radio(9,10);
RF24Network network(radio);

//Measurments config
DHT_Unified dht(3, DHT11);


//Temporary variables
long lastSent = 0;


void setup(void)
{
  //Configure outgoing packets
  sendData.type = this_type;
  sendData.priority = this_priority;

  //Configure debug stuff  
  if(DEBUG)Serial.begin(115200);
  if(DEBUG)Serial.println("DHT11-01...");

  //Config nRF24 network
  SPI.begin();
  radio.begin();
  network.begin(CHANNEL, this_node);

  //Config DHT11
  dht.begin();
}

void loop() {
  //Update nRF24 network
  network.update();                       

  unsigned long now = millis();              // If it's time to send a message, send it!
  if ( now - lastSent >= measure_interval  )
  {
    lastSent = now;
    boolean error = false; //Won't send data ih DHT11 malfunctions...
    if(DEBUG)Serial.print("Sending...");
    RF24NetworkHeader header(other_node);

    //Measure using DHT11
    sensors_event_t event;
    dht.temperature().getEvent(&event);
    if (isnan(event.temperature)) {
      if(DEBUG)Serial.println(F("Error reading temperature!"));
      error = true;
    }
    else {
      sendData.temperature = event.temperature;
      if(DEBUG)Serial.print(F("Temperature: "));
      if(DEBUG)Serial.print(sendData.temperature);
      if(DEBUG)Serial.println(F("°C"));
    }
    dht.humidity().getEvent(&event);
    if (isnan(event.relative_humidity)) {
    if(DEBUG)Serial.println(F("Error reading humidity!"));
    }
    else {
      if(DEBUG)Serial.print(F("Humidity: "));
      if(DEBUG)Serial.print(event.relative_humidity);
      if(DEBUG)Serial.println(F("%"));
      sendData.humidity = event.relative_humidity;
    }
    
    bool ok = !error?network.write(header,&sendData,sizeof(sendData)):false;
    if(DEBUG){
      if (ok)
        Serial.println("ok.");
      else
        Serial.println("failed.");
    }
  }
}
