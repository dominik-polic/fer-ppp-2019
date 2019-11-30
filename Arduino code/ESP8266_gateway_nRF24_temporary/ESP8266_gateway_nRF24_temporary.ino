/*
 Copyright (C) 2012 James Coliz, Jr. <maniacbug@ymail.com>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 version 2 as published by the Free Software Foundation.
 
 Update 2014 - TMRh20
 */

/**
 * Simplest possible example of using RF24Network,
 *
 * RECEIVER NODE
 * Listens for messages from the transmitter and prints them out.
 */

#include <RF24Network.h>
#include <RF24.h>
#include <SPI.h>


RF24 radio(2,4);                // nRF24L01(+) radio attached using Getting Started board 

RF24Network network(radio);      // Network uses that radio
const uint16_t this_node = 00;    // Address of our node in Octal format ( 04,031, etc)
const uint16_t other_node = 01;   // Address of the other node in Octal format

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

//Standard packet structure - BEWARE the int format difference between AT328 and ESP8266!!
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


void setup(void)
{
  Serial.begin(115200);
  Serial.println("RF24Network/examples/helloworld_rx/");
 
  SPI.begin();
  radio.begin();
  network.begin(/*channel*/ 90, /*node address*/ this_node);
}

void loop(void){
  
  network.update();                  // Check the network regularly

  
  while ( network.available() ) {     // Is there anything ready for us?
    
    RF24NetworkHeader header;        // If so, grab it and print it out
    
    network.read(header,&sendData,sizeof(sendData));
    Serial.print("Received type :");
    Serial.print(sendData.type);
    Serial.print(", prio: ");
    Serial.print(sendData.priority);
    Serial.print(", temp: ");
    Serial.print(sendData.temperature);
    Serial.print(", hum: ");
    Serial.print(sendData.humidity);
    Serial.print(", pres: ");
    Serial.print(sendData.pressure);
    Serial.print(", lig: ");
    Serial.print(sendData.light);
    Serial.print(", SW: ");
    Serial.print(sendData.switchActive);
    Serial.print(", BTN: ");
    Serial.print(sendData.buttonPressed);
    Serial.print(", MOT: ");
    Serial.print(sendData.motionDetected);
    Serial.print(", from: ");
    Serial.println(header.from_node);
  }
}
