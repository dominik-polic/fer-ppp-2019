# fer-ppp-2019
This repository contains source code for my Software Design Project class at Faculty of Electrical Engineering and Computing at University of Zagreb (2019/2020).

Project task is creating an embedded gateway for connecting low-power sensors to the internet. I used ESP8266 with the nRF24 sensor network. The gateway also stores packets in SPIFFS when no connection is available and when connection becomes available sends stored packets according to predefined priority. It also utilizes NTP to sync the time with the network and uses this value as a timestamp for packets. 

In order to successfully compile the source you should clone the [libraries](/Arduino%20code/libraries) folder from this repo to you Arduino Sketchbook, or any other place where Arduino IDE looks for libraries. 

**You must use exactly those libraries** because some have been modified in order to save memory and work with this specific project. 

~~**INPORTANT!** There is a bug in the latest official version ov Arduino AVR Boards core, so you must downgrade to v1.6.21 from the Boards Manager in order to compile the ATmega328 code...~~ (This might no longer be true, but I'm too scared of errors to check it out)

# Proof of concept system

The proof of concept system consists of 3 devices: 
 - [**Gateway**](/Arduino%20code/ESP8266_gateway) 
   - NodeMCU 1.0 (ESP8266)
   - nRF24 radio (address-00)
 - [**NODE1:**](/Arduino%20code/AT328_NODE01_DHT11) only used to send data
   - Arduino Nano
   - nRF24 radio (address-01)
   - DHT11 (temperature and humidity sensor)
 - [**NODE2:**](/Arduino%20code/AT328_NODE02_LED_BTN) used to send and receive data
   - Arduino Leonardo
   - nRF24 radio (address-02)
   - pushbutton
   - PWM controlled LED
