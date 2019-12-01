# fer-ppp-2019
This repository contains source code for my Software Design Project class at Faculty of Electrical Engineering and Computing at University of Zagreb (2019/2020).

Project task is creating an embedded gateway for connecting low-power sensors to the internet. I used ESP8266 with the nRF24 sensor network. The gateway also stores packets in SPIFFS when no connection is available and when connection becomes available sends stored packets according to predefined priority. It also utilizes NTP to sync the time with the network and uses this value as a timestamp for packets. 

The proof of concept system consists of 3 devices: 
 - **Gateway** 
   - NodeMCU 1.0 (ESP8266)
   - nRF24 radio (address-00)
 - **NODE1:** only used to send data
   - Arduino Nano
   - nRF24 radio (address-01)
   - DHT11 (temperature and humidity sensor)
 - **NODE2:** used to send and receive data
   - Arduino Leonardo
   - nRF24 radio (address-02)
   - pushbutton
   - PWM controlled LED

