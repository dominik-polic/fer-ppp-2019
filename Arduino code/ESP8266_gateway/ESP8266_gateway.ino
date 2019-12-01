//Include required libraries
#include <WiFiClientSecure.h>     //Needed for HTTPS connections
#include <FirebaseArduino.h>      //Needed for receiving stream from firebase
#include <ESP8266WiFi.h>          //Needed to onnect to WiFi
#include <ESP8266HTTPClient.h>    //Needed to send / receive data from DB
#include <ESP8266Ping.h>          //Needed to check internet conectivity
#include <ESP8266WebServer.h>     //Needed for setup mode web server
#include <NTPClient.h>            //Needed for NTP time sync 
#include <WiFiUdp.h>              //Also needed for NTP
#include <EEPROM.h>               //Needed to save / restore data using EEPROM
#include <RF24Network.h>          //Needed for nRF sensor network
#include <RF24.h>                 //Needed for nRF sensor network
#include <SPI.h>                  //Needed for nRF sensor network
#include "FS.h"                   //Needed for SPIFFS data storing

//Define required constants
#define DEBUG true
#define FIREBASE_URL "ppp-2019-dominik-polic.firebaseio.com"
#define FIREBASE_ROOT_PATH "/users"
#define FIREBASE_DOWNSTREAM_PATH "/to_gateway"
#define FIREBASE_UPSTREAM_PATH "/from_gateway"
#define DEVICE_TYPE "SENSOR-GATEWAY"
#define HTTP_TIMEOUT 5    //THIS IS IN SECONDS!!!!!!
#define MAX_FIREBASE_RETRIES 5
#define MAX_TOKEN_RETRIES 5
#define MAX_WIFI_RETRIES 30
#define PING_INTERVAL 20000 //20 seconds
#define FIREBASE_FORCE_TOKEN_RELOAD_INTERVAL 3500000  //a bit less then 1 hour
#define FIREBASE_STREAM_RELOAD_INTERVAL 1800000 //30 minutes
#define DEFAULT_AP_SSID "SensorGW" //MAC address is appended to this to make it unique
#define DEFAULT_AP_PASSWORD "12345678"

//Define Firebase realtime database and authentication information
#define ACCESS_REFRESH_URL "https://securetoken.googleapis.com/v1/token?key=AIzaSyBLvDlBWPdulyt4tHrkjSfPK3twKpPV2DQ" //String after "key=" is the firebase Web API key
#define ACCESS_TOKEN_POST_STRING_BEGIN "{\"grant_type\" : \"refresh_token\", \"refresh_token\" : \""
#define ACCESS_TOKEN_POST_STRING_END "\" }"

//Sensor to gateway types (nRF24 bus)
#define TYPE_DHT11 1 //Temperature and humidity sensor
#define TYPE_TEMPERATURE 2 //Temperature only
#define TYPE_HUMIDITY 3 //Humidity only
#define TYPE_LIGHT 4 //Light sensor
#define TYPE_BMP280 5 //Temperature and pressure sensor
#define TYPE_SWITCH 6 //Digital switch
#define TYPE_BUTTON 7 //Pushbutton
#define TYPE_MOTION 8 //Motion sensor
#define TYPE_LIGHT_BTN 9 //Pushbutton and incoming command light

#define GATEWAY_ADDRESS 00 //nRF24Network address of the gateway
#define CHANNEL 90
#define UNAVAILABLE_VALUE 9999;

//Priority definitions
#define PRIORITY_LOW 1
#define PRIORITY_NORMAL 2
#define PRIORITY_HIGH 3
#define PRIORITY_INSTANT_ONLY 4 //WIth this priority data is only sent if the connection is currently available, otherwise it's discarded. Useful for button presses that are only relevant at one specific moment.

#define MAX_CONSECUTIVE_SENDS 10 //How many entries to send to db in a loop without running other checks
#define MAX_DEVICES 6 //How many addresses are allowed to connect to the gateway, used for parsing data from db on connection established... (from 0 to MAX_DEVICES-1)
//Factory reset button...
#define FACTORY_RESET_BTN 3

//NTP config
#define UTC_OFFSET_IN_SECONDS 0 //This can be changed to any timezone, but I recommend using central time to sync data between timezones
#define NTP_HOST "time.google.com"
//#define NTP_HOST "pool.ntp.org  //Some alternative hosts...
#define NTP_REFRESH_INTERVAL 10000 //10 seconds

//SPIFFS log config
#define NO_FILE "no-record"


//Define EEPROM structure and data
#define EEPROM_ADDRESS 0
struct {
  boolean firstSetupDone = false;
  char wifiSSID[100] = "";
  char wifiPassword[100] = "";
  char ACCESS_REFRESH_TOKEN[2048] = "NONE";
  char ACCESS_UID[200] = "NONE";
  char ACCESS_DEVICE_ID[50] = "NONE";
} EEPROMdata;

//Standard nRF24 packet structure - BEWARE the int format difference between AT328 and ESP8266!!
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

//Ping google's DNS server
IPAddress PING_IP(8, 8, 8, 8);

//NTP setup
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_HOST, UTC_OFFSET_IN_SECONDS, NTP_REFRESH_INTERVAL);

//Global HTTPS clients to allocate fixed memory to improve performance
WiFiClientSecure clientW;
WiFiClientSecure sendClientW;
HTTPClient sendClient;
HTTPClient tokenClient;

//Settings variables
String wifiSSID = "";
String wifiPassword = "";
char ACCESS_ID_TOKEN[2048] = "";

//Temporary counters and variables
uint8_t tokenRetries = 0;
boolean initSetupWifiDataReceived = false;
boolean wifiConnected = false;
boolean internetConnected = false;
boolean tokenConnected = false;
boolean firebaseStarted = false;
unsigned long lastPing = 0;
unsigned long lastFirebaseReload = 0;
unsigned long lastFirebaseTokenTime = 0;
unsigned long tempCounter = 0;
unsigned long lastNTPSync = 0;

//Define setup web server here and close it if possible.... should maybe find an alternative approach(?) found it, too lazy to comment ahahaha
ESP8266WebServer setupServer(80);
FirebaseArduino *firebaseInstance = new FirebaseArduino();

//Start attached nRF radio
RF24 radio(2,4);
RF24Network network(radio);

//nRF24 gateway config (this device)
const uint16_t this_node = GATEWAY_ADDRESS; 

void setup() {  
  //Allow insecure HTTPS
  clientW.setInsecure();
  sendClientW.setInsecure();
  clientW.setBufferSizes(4096, 4096);
  sendClientW.setBufferSizes(512, 512);
  sendClient.setReuse(true);
  tokenClient.setReuse(true);
  
  //Initiate serial communication
  if(DEBUG) Serial.begin(115200);

  //Initialize EEPROM
  EEPROM.begin(3072);  

  //Begin SPIFFS (for saving data)
  SPIFFS.begin();
  
  //load saved settings from memory
  loadSettings();

  

  //If no user data is found, enter initial setup mode
  if (!EEPROMdata.firstSetupDone) {
    if(DEBUG) Serial.println(EEPROMdata.wifiSSID);
    firstSetupMode();
  } else {
    //Factory reset when holding a button on startup...
    pinMode(FACTORY_RESET_BTN,INPUT_PULLUP);
    if(digitalRead(FACTORY_RESET_BTN)==LOW){
      if(DEBUG)Serial.println("Factory reset on boot caused by RESET BUTTON!!!");
      factoryReset(true);
    }
    normalSetup();
  }

  
}

//This function takes care of the initial setup
void firstSetupMode() {
  if(DEBUG) Serial.println(F("ENTERING SETUP MODE!"));

  //Create semi-unique hotspot name:
  String apSSID = String(DEFAULT_AP_SSID) + " " + String(WiFi.macAddress());
  apSSID.replace(':', ' ');
  if(DEBUG) Serial.println(String(F("Created setup hotspot with SSID: "))+apSSID);
  //Create setup AP
  WiFi.softAP(apSSID, DEFAULT_AP_PASSWORD);
  WiFi.mode(WIFI_AP);

  setupServer.on("/", HTTP_GET, handleRoot);
  //This shows the network ESP8266 can see, to be implemented in the Android/web app for the gateway
  setupServer.on("/available-networks", HTTP_GET, []() {

    if(DEBUG) Serial.println("Scanning for networks.......");
    int numberOfNetworks = WiFi.scanNetworks();
    String serverResponse = "{\"networks\":[";
    for (int i = 0; i < numberOfNetworks; i++) {
      if (i > 0)serverResponse += ",";
      serverResponse += "{\"name\":\"";
      serverResponse += WiFi.SSID(i);
      serverResponse += "\",\"rssi\":";
      serverResponse += WiFi.RSSI(i);
      serverResponse += "}";
    }
    serverResponse += "]}";
    setupServer.send(200, "text/plain", serverResponse);
  });
  setupServer.onNotFound(handleNotFound);
  setupServer.on("/device-setup", HTTP_POST, handleSetup);
  setupServer.begin();
  while (!EEPROMdata.firstSetupDone && !initSetupWifiDataReceived) {
    setupServer.handleClient();
  }
  setupServer.handleClient();

  //Handle wifi data received.....
  if(DEBUG) Serial.println(F("Setup received...."));
  WiFi.begin(wifiSSID, wifiPassword);

  uint8_t counter_wifi = 0;
  while (WiFi.status() != WL_CONNECTED && counter_wifi <= MAX_WIFI_RETRIES) {
    setupServer.handleClient();
    if(DEBUG) Serial.println(F("Connecting to WiFi in SETUP mode....."));
    delay(500);
    counter_wifi++;
  }
  
  //If connection succedded, disable initial setup mode and restart
  checkWifiConnection();
  if (wifiConnected) {
    if(DEBUG) Serial.println(F("SETUP SUCCESS!!"));
    EEPROMdata.firstSetupDone = true;
    saveSettings();
    delay(1000);
    ESP.restart();
  } else {
    if(DEBUG) Serial.println(F("SETUP FAILED!!"));
    EEPROMdata.firstSetupDone = false;
    saveSettings();
    delay(1000);
    ESP.restart();
  }
}



//This function prepares the device for use if it has been correctly configured
void normalSetup() {
  if(DEBUG) Serial.println(F("Starting normal startup...."));
  //Connect to WiFi
  WiFi.begin(wifiSSID, wifiPassword);
  WiFi.enableAP(false);
  WiFi.mode(WIFI_STA);
  //Wait for connection, and if it fails after limited time enter offline mode!
  uint8_t counter_wifi = 0;
  while (WiFi.status() != WL_CONNECTED && counter_wifi <= MAX_WIFI_RETRIES) {
    if(DEBUG) Serial.println(F("Connecting to WiFi....."));
    delay(500);
    counter_wifi++;
  }

  //Update wifi connection status
  checkWifiConnection();

  //Print IP address to serial if DEBUG enabled
  if (wifiConnected) {
    if(DEBUG) Serial.println(F("Connected to wifi!"));

    //Check for internet connection on local WiFi
    checkInternetConnection();
    if (internetConnected) {
      if(DEBUG) Serial.println(F("Internet connection is available :D"));
    } else {
      if(DEBUG) Serial.println(F("Internet connection is unavailable :/"));
    }

  }

  //If internet connection was successful
  if (internetConnected) {
    getTokenWithRetry();
  }

  //Connect to firebase and subscribe to this device actions
  if (tokenConnected) {
    reinitFirebase();
  }
  timeClient.begin();

  //nRF radio setup
  SPI.begin();
  radio.begin();
  network.begin(CHANNEL, this_node);

}


void loop() {
  //nRF24 netowrk update state
  network.update();
  
  //Check for incoming data on the nRF24 bus and add it to queue
  nRF24FromProcess();

  //Send data to nRF24 from queue if available
  nRF24ToProcess();
  
//START Network communication and stuff ------------------
  //Sync time
  timeClient.update();

  //Refresh ID token every once in a while
  if (millis() > lastFirebaseTokenTime + FIREBASE_FORCE_TOKEN_RELOAD_INTERVAL && internetConnected && wifiConnected) {
    getTokenWithRetry();
    if (!tokenConnected && internetConnected) {
      if(DEBUG) Serial.println(F("Unresolvable problem with firebase! User account has probably changed. Initiating factory reset...."));
      factoryReset(true);
    }
  }

  //Check WiFi and Internet connection
  pingNetwork(false);

  //Request new token if needed
  if (internetConnected && !tokenConnected) {
    getTokenWithRetry();
  }

  //Reconnect to firebase if needed
  if (tokenConnected && !firebaseStarted) {
    reinitFirebase();
  }

  //Check for data from firebase
  checkFirebaseData();

//END Netowrk communication and stuff ---------------------
  
  //If full connection to the database is available receive data from it and send from queue to it
  if(firebaseStarted){
    
    //Send data from outgoing queue to database
    databaseToProcess();
    
  }

}

//Handle setup POST request and update data in memory
void handleSetup() {
  //If request is not formatted correctly, refuse it....
  if (setupServer.hasArg("wifiSSID") &&  setupServer.hasArg("wifiPassword")) {
    if(DEBUG) Serial.println("Setting up....");

    if (setupServer.hasArg("ACCESS_REFRESH_TOKEN") && setupServer.hasArg("ACCESS_DEVICE_ID") && setupServer.hasArg("ACCESS_UID")) {
      setupServer.send(200, "text/plain", "SETUP-FULL");
      //Accept correct request
      wifiSSID = setupServer.arg("wifiSSID");
      wifiPassword = setupServer.arg("wifiPassword");
      String(ACCESS_TOKEN_POST_STRING_BEGIN + setupServer.arg("ACCESS_REFRESH_TOKEN") + ACCESS_TOKEN_POST_STRING_END).toCharArray(EEPROMdata.ACCESS_REFRESH_TOKEN, 2048);
      setupServer.arg("ACCESS_DEVICE_ID").toCharArray(EEPROMdata.ACCESS_DEVICE_ID, 50);
      setupServer.arg("ACCESS_UID").toCharArray(EEPROMdata.ACCESS_UID, 200);
      if(DEBUG) Serial.println("Setup full");
      saveSettings();
      //Mark received settings
      initSetupWifiDataReceived = true;

    } else {
      if (String(EEPROMdata.ACCESS_REFRESH_TOKEN) == "NONE") {
        setupServer.send(403, "text/plain", "ERROR-WIFI-NO-SAVED-CREDENTIALS");
        return;
      }
      setupServer.send(200, "text/plain", "SETUP-WIFI");
      //Mark received settings
      initSetupWifiDataReceived = true;

      //Accept correct request
      wifiSSID = setupServer.arg("wifiSSID");
      wifiPassword = setupServer.arg("wifiPassword");
      if(DEBUG) Serial.println("Setup wifi only");

    }
  } else {
    setupServer.send(400, "text/plain", "400: Invalid Request");
    return;
  }
}

//This function tries to get new id token from google auth server.
//If it fails with active internet connection that means user account has changed and new initial setup is required.
boolean getTokenWithRetry() {
  boolean gotToken = false;
  for (uint8_t i = 0; i < MAX_TOKEN_RETRIES; i++) {
    if (requestNewToken()) {
      gotToken = true;
      lastFirebaseTokenTime = millis();
      break;
    }
    pingNetwork(true);
    if (!internetConnected) {
      firebaseStarted = false;
      gotToken = false;
      break;
    }
    delay(200);
  }

  if (gotToken) {
    if(DEBUG) Serial.println(F("Successfully got connection token!"));
  } else {
    if(DEBUG) Serial.println(F("Successfully got connection token!"));
  }
  tokenConnected = gotToken;
  return gotToken;
}


//Function to get new ID token from the constant refresh token once the old one has expired
boolean requestNewToken() {
  if(DEBUG) Serial.println(F("Requesting new id token....."));
  boolean tokenSuccess = false; //Default to failed state
  firebaseInstance->endStream();
  firebaseStarted = false;
  //Try to connect to Firebase auth server
  if (tokenClient.begin(clientW, ACCESS_REFRESH_URL)) {
    if(DEBUG) Serial.println(F("Established connection to google auth server"));

    int httpCode = tokenClient.POST(String(EEPROMdata.ACCESS_REFRESH_TOKEN));
    if(DEBUG) Serial.println(String(F("httpCode: "))+String(httpCode));
    //If POST was successful, check HTTP status code and if token was granted store it
    if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      if(DEBUG) Serial.println(String(F("Google auth server responded with code: ")) + String(httpCode));

      // if token has been sent, update it!
      if (httpCode == 200) {
        tokenRetries = 0;
        if(DEBUG) Serial.println(F("SUCCESS! Got new token!"));
        getTokenFromResponse(tokenClient, "id_token\": \"", '"');
        //Update token in firebase library
        firebaseInstance->updateAuth(ACCESS_ID_TOKEN);

        //Mark successfull token extraction
        tokenSuccess = true;
      } else if (httpCode == 400) {
        if(DEBUG) Serial.println("CODE 400 restart....");
        ESP.restart();
      } else {

        if(DEBUG) Serial.println(F("POST did not return 200 SUCCESS, probably authentication problem?"));
      }
    } else {
      if(DEBUG) Serial.println(F("POST to google auth server failed!"));
    }
    tokenClient.end();
  } else {
    if(DEBUG) Serial.println(F("Unable to connect to google auth server!"));
  }
  return tokenSuccess;
}

//This function resets all settings to factory defaults
void resetSettings(boolean full) {
  EEPROMdata.firstSetupDone = false;
  wifiSSID = "";
  wifiPassword = "";
  ACCESS_ID_TOKEN[0] = (char)0;
  //This part removes the unique device ID and user identification...
  if (full) {
    String("NONE").toCharArray(EEPROMdata.ACCESS_REFRESH_TOKEN, 2048);
    String("NONE").toCharArray(EEPROMdata.ACCESS_UID, 200);
    String("NONE").toCharArray(EEPROMdata.ACCESS_DEVICE_ID, 50);
  }
  saveSettings();
}

//This part should load settings from memory
void loadSettings() {
  EEPROM.get(EEPROM_ADDRESS, EEPROMdata);
  //User data
  wifiSSID = EEPROMdata.wifiSSID;
  wifiPassword = EEPROMdata.wifiPassword;
  if(DEBUG) Serial.println("Loaded settings from EEPROM....");
}

//This part should save settings to memory
void saveSettings() {
  if(DEBUG) Serial.println(F("Saving settings to EEPROM...."));

  //User data
  wifiSSID.toCharArray(EEPROMdata.wifiSSID, 100);
  wifiPassword.toCharArray(EEPROMdata.wifiPassword, 100);

  EEPROM.put(EEPROM_ADDRESS, EEPROMdata);
  EEPROM.commit();

}


//Extract tokens and uid from HTTP response. This is a piece of code that somehow finnaly works and we do not want to mess with it!
void getTokenFromResponse(HTTPClient &http, String key, char terminate) {
  bool found = false, look = false;
  int ind = 0;
  int tokenCurrentPos = 0;
  int len = http.getSize();
  char char_buff[1];
  WiFiClient * stream = http.getStreamPtr();
  while (http.connected() && (len > 0 || len == -1)) {
    size_t size = stream->available();
    if (size) {
      int c = stream->readBytes(char_buff, ((size > sizeof(char_buff)) ? sizeof(char_buff) : size));
      if (len > 0)
        len -= c;
      if (found) {
        if (char_buff[0] == terminate) {
          break;
        } else {
          ACCESS_ID_TOKEN[tokenCurrentPos] = char_buff[0];
          tokenCurrentPos++;
        }
      }
      else if ((!look) && (char_buff[0] == key[0])) {
        look = true;
        ind = 1;
      } else if (look && (char_buff[0] == key[ind])) {
        ind ++;
        if (ind == key.length()) found = true;
      } else if (look && (char_buff[0] != key[ind])) {
        ind = 0;
        look = false;
      }
    }
  }
  ACCESS_ID_TOKEN[tokenCurrentPos] = 0; //Terminate token string
  return;
}

//Return formatted string from supplied epoch time
String getTimeString(unsigned long epochTime) {
  time_t rawtime = epochTime;
  struct tm  ts;
  char buf[80];
  ts = *localtime(&rawtime);
  strftime(buf, sizeof(buf), "%a %Y-%m-%d %H:%M:%S %Z", &ts);
  return String(buf);
}

//Handle root GET request
void handleRoot() {
  setupServer.send(200, "text/html", "Please send user credentials and wifi data to the gateway.");
}

//Handle wrong requests
void handleNotFound() {
  setupServer.send(404, "text/plain", "404: Not found");
}

void checkWifiConnection() {
  wifiConnected = (WiFi.status() == WL_CONNECTED ) ? true : false;
}

void checkInternetConnection() {

  if (Ping.ping(PING_IP, 1)) {
    internetConnected = true;
  } else {
    internetConnected = false;
  }
}

//Check for updated data from firebase
void checkFirebaseData() {

  //Reinitialize firebase connection periodically
  if (millis() > lastFirebaseReload + FIREBASE_STREAM_RELOAD_INTERVAL) {
    lastFirebaseReload = millis();
    firebaseStarted = false;
  }

  if (tokenConnected && firebaseStarted == false) {
    reinitFirebase();
  }

  //Check for error
  if (firebaseInstance->failed()) {
    if(DEBUG) Serial.println(String(F("Firebase error: ")) + firebaseInstance->error());
    checkWifiConnection();
    if (wifiConnected) {
      checkInternetConnection();
      if (internetConnected) {
        getTokenWithRetry();
        if (tokenConnected) {
          if(DEBUG) Serial.println(F("Restored token connection!"));
          reinitFirebase();
          if (firebaseStarted) {
            if(DEBUG) Serial.println(F("Restored firebase connection!"));
          } else {
            if(DEBUG) Serial.println(F("FAILED to restore firebase connection!"));
          }
        }
      } else {
        if(DEBUG) Serial.println(F("Lost internet connection!"));
        tokenConnected = false;
      }

    } else {
      if(DEBUG) Serial.println(F("Lost WiFi connection!"));
      internetConnected = false;
      tokenConnected = false;
    }
    delay(500);
  }


  //When new data is available....
  if (firebaseInstance->available()) {
    FirebaseObject event = firebaseInstance->readEvent();
    String eventType = event.getString("type");
    eventType.toLowerCase();
    if(DEBUG) Serial.println("Firebase event: " + eventType);
    //If new data is written to db, process it
    if (eventType == "put") {
      String path = event.getString("path");
      if(DEBUG) Serial.println("Firebase path: " + path);

      //More than one packet is incomming...
      if (path == "/") {
        
        for(int i = 1; i<MAX_DEVICES; i++){
          String currentdata = event.getString("data/"+String(i));
          Serial.println(String("Data for address ")+String(i)+":"+currentdata);
          if(currentdata!="NOT-STRING"){
            addRecordToFile(String(i)+"-"+splitString(currentdata,':',0),splitString(currentdata,':',1).toInt(),false);
          }
        }
        deleteFirebaseTree();
      } else {
        String data = event.getString("data");
        if (data == "NOT-STRING") {
          if(DEBUG) Serial.println(String(F("ERROR PARSING DATA!!!! WRONG FORMAT AT PATH: ")) + path);
        } else {          
          addRecordToFile(splitString(path,'/',1)+"-"+splitString(data,':',0),splitString(data,':',1).toInt(),false);
          deleteFirebaseString(path);
        }
      }
    }
  }
}

//This resets the ESP8266 to factory setting and it enters setup mode
void factoryReset(boolean full) {
  if(DEBUG) Serial.println(F("Initiating factory reset!!!!!"));
  resetSettings(full);
  ESP.restart();
}

//This reinitialises firebase stream and stuff
void reinitFirebase() {
  firebaseInstance->endStream();
  for (uint8_t i = 0; i < MAX_FIREBASE_RETRIES; i++) {
    if(DEBUG) Serial.println(String(F("Heap before firebase:"))+String(ESP.getFreeHeap()));
    if(DEBUG) Serial.println(F("INIT/REINIT firebase connection"));
    firebaseInstance->begin(FIREBASE_URL, String(ACCESS_ID_TOKEN));
    if(DEBUG) Serial.println(String(F("Heap before stream but after begin:"))+String(ESP.getFreeHeap()));
    firebaseInstance->stream(String(FIREBASE_ROOT_PATH) + "/" + String(EEPROMdata.ACCESS_UID) + "/" + String(DEVICE_TYPE) + "/" + String(EEPROMdata.ACCESS_DEVICE_ID) + String(FIREBASE_DOWNSTREAM_PATH));
    long startTime = millis();
    while (millis() <= startTime + 1000 && !firebaseInstance->success());
    if(DEBUG) Serial.println(String(F("Heap after firebase:"))+String(ESP.getFreeHeap()));
    if (firebaseInstance->success()) {
      firebaseStarted = true;
      if(DEBUG) Serial.println(F("SUCCESS! Firebase reinitialised :D"));
      break;
    } else {
      firebaseStarted = false;
      if(DEBUG) Serial.println(F("FAIL! Firebase reinit failed!"));
    }

  }

}

//This function checks network conectivity
void pingNetwork(boolean force) {
  if (millis() > lastPing + PING_INTERVAL || force) {
    lastPing = millis();
    checkWifiConnection();
    if (!wifiConnected) {
      if(DEBUG) Serial.println(F("PING determined no wifi connection!"));
      wifiConnected = false;
      internetConnected = false;
      tokenConnected = false;
      firebaseStarted = false;
      return;
    }
    checkInternetConnection();
    if (!internetConnected) {
      if(DEBUG) Serial.println(F("No network! Mark firebase as offline!"));
      firebaseStarted = false;
      tokenConnected = false;
    }
  }
}


void databaseToProcess(){
  //Check for data to be sent to WiFi bus, send one by one....
  String path;
  String data;
  String filedata;
  int counter = 0;
  //Send priority 1
  while(filedata = readRecordFromFile(PRIORITY_HIGH,true),filedata != NO_FILE){
    if(DEBUG)Serial.println("Priority 1 for FB: "+filedata);
    path = splitString(filedata,'-',0);
    data = splitString(filedata,'-',1);
    sendFirebaseStringNoDataFormatting(path,data);
    counter++;
    if(counter>MAX_CONSECUTIVE_SENDS) return;
  }
  while(filedata = readRecordFromFile(PRIORITY_NORMAL,true),filedata != NO_FILE){
    if(DEBUG)Serial.println("Priority 2 for FB: "+filedata);
    path = splitString(filedata,'-',0);
    data = splitString(filedata,'-',1);
    sendFirebaseStringNoDataFormatting(path,data);
    counter++;
    if(counter>MAX_CONSECUTIVE_SENDS) return;
  }
  while(filedata = readRecordFromFile(PRIORITY_LOW,true),filedata != NO_FILE){
    if(DEBUG)Serial.println("Priority 3 for FB: "+filedata);
    path = splitString(filedata,'-',0);
    data = splitString(filedata,'-',1);
    sendFirebaseStringNoDataFormatting(path,data);
    counter++;
    if(counter>MAX_CONSECUTIVE_SENDS) return;
  }

}


void nRF24ToProcess(){  //Data for nRF24 in format {address}-{int_value}
  String data;
  int address;
  int value;

  //Send instant priority
  while(data = readRecordFromFile(PRIORITY_INSTANT_ONLY,false),data != NO_FILE){
    address = splitString(data,'-',0).toInt();
    value = splitString(data,'-',1).toInt();
    nRF24Send(address,value);
    
  }
  
  //Send priority 3
  while(data = readRecordFromFile(PRIORITY_HIGH,false),data != NO_FILE){
    address = splitString(data,'-',0).toInt();
    value = splitString(data,'-',1).toInt();
    if(!nRF24Send(address,value)){
      addRecordToFile(data,1,false);
      break;
    }
  }

  //Send priority 2
  while(data = readRecordFromFile(PRIORITY_NORMAL,false),data != NO_FILE){
    address = splitString(data,'-',0).toInt();
    value = splitString(data,'-',1).toInt();
    if(!nRF24Send(address,value)){
      addRecordToFile(data,2,false);
      break;
    }
  }
  
  //Send priority 1
  while(data = readRecordFromFile(PRIORITY_LOW,false),data != NO_FILE){
    address = splitString(data,'-',0).toInt();
    value = splitString(data,'-',1).toInt();
    nRF24Send(address,value);
    if(!nRF24Send(address,value)){
      addRecordToFile(data,3,false);
      break;
    }
  }

  
}

void nRF24FromProcess(){

  //If data is available process it...
  if(network.available()) {
    RF24NetworkHeader header;
    network.read(header,&sendData,sizeof(sendData));

    //DEBUG prints....
    if(DEBUG){
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
    int priority = sendData.priority;
    int from_address=header.from_node;
    
    //Read data and calculate priority
    String data = processnRF24Data(from_address);

    //Add message to outgoing queue
    if(priority == PRIORITY_HIGH || priority == PRIORITY_NORMAL || priority == PRIORITY_LOW){
      if(DEBUG)Serial.println("HIGH/NORMAL/LOW PRIORITY");
      addRecordToFile(data,priority,true); //toFirebase = true   
    }else if(priority == PRIORITY_INSTANT_ONLY){
      if(firebaseStarted){
        if(DEBUG)Serial.println("PRIORITY INSTANT ONLY, SENDING INSTANTLY!!!");
        String path = splitString(data,'-',0);
        String datasend = splitString(data,'-',1);
        sendFirebaseStringNoDataFormatting(path,datasend);
      }else{
        if(DEBUG)Serial.println("PRIORITY INSTANT ONLY, BUT NO NETWORK, IGNORING!!!");
      }
    }else{
      if(DEBUG)Serial.println("ERROR: UNKNOWN PRIORITY");
    }
   }  
}

String processnRF24Data(int from_address){
  int type = sendData.type;
  boolean awaitingZarez = false;
  String data = "NODE";
  data+=String(from_address)+"/"+String(timeClient.getEpochTime())+"-{";
  if(type == TYPE_DHT11 || type == TYPE_TEMPERATURE || type == TYPE_BMP280){
    if(awaitingZarez){
      data+=",";
      awaitingZarez=false;
    }
    data+="\"temperature\":"+String(sendData.temperature);
    awaitingZarez = true;
  }
  if(type == TYPE_DHT11 || type == TYPE_HUMIDITY){
    if(awaitingZarez){
      data+=",";
      awaitingZarez=false;
    }
    data+="\"humidity\":"+String(sendData.humidity);
    awaitingZarez = true;
  }
  if(type == TYPE_BMP280){
    if(awaitingZarez){
      data+=",";
      awaitingZarez=false;
    }
    data+="\"pressure\":"+String(sendData.pressure);
    awaitingZarez = true;
  }
  if(type == TYPE_LIGHT){
    if(awaitingZarez){
      data+=",";
      awaitingZarez=false;
    }
    data+="\"light\":"+String(sendData.light);
    awaitingZarez = true;
  }
  if(type == TYPE_SWITCH){
    if(awaitingZarez){
      data+=",";
      awaitingZarez=false;
    }
    data+="\"switchActive\":"+String(sendData.switchActive?"true":"false");
    awaitingZarez = true;
  }
  if(type == TYPE_BUTTON || type == TYPE_LIGHT_BTN){
    if(awaitingZarez){
      data+=",";
      awaitingZarez=false;
    }
    data+="\"buttonPressed\":"+String(sendData.buttonPressed?"true":"false");
    awaitingZarez = true;
  }
  if(type == TYPE_MOTION){
    if(awaitingZarez){
      data+=",";
      awaitingZarez=false;
    }
    data+="\"motionDetected\":"+String(sendData.motionDetected?"true":"false");
    awaitingZarez = true;
  }
  data+="}";
  return data;
}





String databaseRead(int* priority_address){
  //Read data from database

  //Calculate data priority

  //Convert to String in format: [priority] [device_id] [timestamp] [data]
  priority_address = 0;
  return "";
}

boolean nRF24Send(int device_id, int data){//ONLY ALLOW SENDING OF INTS TO NODES!!
  //Send data to device
  RF24NetworkHeader header(device_id);
  bool ok = network.write(header,&data,sizeof(data));
  if(DEBUG){
    if (ok)
      Serial.println("Send data to nRF24 ok:"+data);
    else
      Serial.println("Send data to nRF24 failed.");
  }
  return ok;
}


//START Firebase helper functions
void sendFirebaseString(String path, String data) {
  sendFirebaseDataHelper(String(FIREBASE_UPSTREAM_PATH) + "/" + path, "\"" + data + "\"");
}

void sendFirebaseStringNoDataFormatting(String path, String data) {
  sendFirebaseDataHelper(String(FIREBASE_UPSTREAM_PATH) + "/" + path, data);
}

void sendFirebaseInt(String path, int data) {
  sendFirebaseDataHelper(String(FIREBASE_UPSTREAM_PATH) + "/" + path, String(data));
}

void deleteFirebaseString(String path) {
  sendFirebaseDataHelper(String(FIREBASE_DOWNSTREAM_PATH) + "/" + path, "null");
}

void deleteFirebaseTree() {
  sendFirebaseDataHelper(String(FIREBASE_DOWNSTREAM_PATH), "null");
}

void sendFirebaseDataHelper(String path, String data) {
  if(DEBUG) Serial.print(String(F("Sending to: "))+path+", data: "+data+".......");

  if (sendClient.begin(sendClientW, String("https://") + String(FIREBASE_URL) + String(FIREBASE_ROOT_PATH) + "/" + String(EEPROMdata.ACCESS_UID) + "/" + String(DEVICE_TYPE) + "/" + String(EEPROMdata.ACCESS_DEVICE_ID) + path + ".json?auth=" + String(ACCESS_ID_TOKEN))) {
    sendClient.addHeader("Content-Type", "text/plain");
    //sendClient.addHeader("Content-Type", "application/json");
    sendClient.addHeader("Connection", "close");
    sendClient.setTimeout(HTTP_TIMEOUT * 1000);
    if(DEBUG) Serial.println(String(F("just before PUT! HEAP: "))+String(ESP.getFreeHeap()));
    int httpCode = sendClient.PUT(data);
    if (httpCode == 200) {
      if(DEBUG) Serial.println(F("SUCCESS"));
    } else if (httpCode > 0) {
      if(DEBUG) Serial.println(String(F("ERROR.... remote server problem(?), HTTPCODE: "))+String(httpCode));
    } else {
      if(DEBUG) Serial.println(F("ERROR... network problem(?)"));
    }
    sendClient.end();
  }
}

void addRecordToFile(String data, int priority, boolean toFirebase){
  //Open the requested file
  String filename = (toFirebase?"/fb":"/rf")+String(priority)+".txt";
  if(DEBUG)Serial.println("addRecord to file: "+filename+", record: "+data);
  File currentFile = SPIFFS.open(filename, "a+");
  //Write supplied data t the end
  currentFile.seek(0,SeekEnd);
  currentFile.println(data);
  currentFile.close();
}

String readRecordFromFile (int priority, boolean toFirebase){
  String data;
  String filename = (toFirebase?"/fb":"/rf")+String(priority)+".txt";
  //if(DEBUG)Serial.println("readRecord from file: "+filename);
  File currentFile = SPIFFS.open(filename, "r+");
  if(!currentFile){
    return NO_FILE;
  }
  //Read one line...
  char buffer[100]; 
  int l = currentFile.readBytesUntil('\n', buffer, sizeof(buffer));
  buffer[l] = 0;
  data=String(buffer);
  //if(DEBUG)Serial.println(String("Next line read: ")+data);

  //Delete this line by transfering all other lines to new file and deleting the current one and renaming the new one because why the hell not.....
  File tempFile = SPIFFS.open("/tempfile.txt","w+");
  while (currentFile.available()) {
    tempFile.write(currentFile.read());
  }
  boolean deleteTemp = false;
  if(tempFile.size() == 0) deleteTemp = true;
  
  currentFile.close();
  tempFile.close();
  SPIFFS.remove(filename);
  if(deleteTemp)
    SPIFFS.remove("/tempfile.txt");
  else
    SPIFFS.rename("/tempfile.txt",filename);
  
  return data;
}

//Helper function to split a string....
String splitString(String data, char separator, int index)
{
    int found = 0;
    int strIndex[] = { 0, -1 };
    int maxIndex = data.length() - 1;

    for (int i = 0; i <= maxIndex && found <= index; i++) {
        if (data.charAt(i) == separator || i == maxIndex) {
            found++;
            strIndex[0] = strIndex[1] + 1;
            strIndex[1] = (i == maxIndex) ? i+1 : i;
        }
    }
    return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}
