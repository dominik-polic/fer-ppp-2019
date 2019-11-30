//This sketch only sends data when button is pressed, and receives data to control the LED

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
#define LED_PIN 11
#define BTN_PIN 12
const uint16_t this_priority = PRIORITY_INSTANT_ONLY;
const uint16_t this_type = TYPE_DHT11;
const uint16_t this_node = 02;   //Address of this node    
const uint16_t other_node = GATEWAY_ADDRESS;  

int receiveData = 0;

//nRF24 module config
RF24 radio(9,10);
RF24Network network(radio);

//Temporary variables
boolean sendPress = false;

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

  //Config LED and BTN
  pinMode(LED_PIN, OUTPUT);
  pinMode(BTN_PIN, INPUT_PULLUP);
  digitalWrite(LED_PIN,LOW);
}

void loop() {
  //Update nRF24 network
  network.update();                       


  //Process button press
  if(digitalRead(BTN_PIN) == LOW){
    delay(60);
    while(digitalRead(BTN_PIN) == LOW);
    sendPress = true;
    delay(60);
  }

  //Send button press if needed
  if (sendPress)
  {
    sendPress = false;
    if(DEBUG)Serial.print("Sending...");
    RF24NetworkHeader header(other_node);
    sendData.buttonPressed = true;   
    bool ok = network.write(header,&sendData,sizeof(sendData));
    sendData.buttonPressed = false;  
    if(DEBUG){
      if (ok)
        Serial.println("ok.");
      else
        Serial.println("failed.");
    }
  }

  //Receive LED brightness
  while ( network.available() ) {    
    RF24NetworkHeader header;    
    network.read(header,&receiveData,sizeof(receiveData));
    if(DEBUG)Serial.println("Adjusting brightness to: "+receiveData);
    analogWrite(LED_PIN,receiveData);
  }
}
