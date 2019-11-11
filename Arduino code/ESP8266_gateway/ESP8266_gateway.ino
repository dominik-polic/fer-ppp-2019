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

//Define required constants
#define DEBUG true
#define FIREBASE_URL "ppp-2019-dominik-polic.firebaseio.com"
#define FIREBASE_ROOT_PATH "/users"
#define FIREBASE_DOWNSTREAM_PATH "/to_gateway"
#define FIREBASE_UPSTREAM_PATH "/from_gateway"
#define HTTP_TIMEOUT 5    //THIS IS IN SECONDS!!!!!!
#define MAX_FIREBASE_RETRIES 5
#define MAX_TOKEN_RETRIES 5
#define MAX_WIFI_RETRIES 30
#define PING_INTERVAL 20000 //20 seconds
#define FIREBASE_FORCE_TOKEN_RELOAD_INTERVAL 3500000  //a bit less then 1 hour
#define FIREBASE_STREAM_RELOAD_INTERVAL 1800000 //30 minutes
#define DEFAULT_AP_SSID "Sensor gateway" //MAC address is appended to this to make it unique
#define DEFAULT_AP_PASSWORD "12345678"

//NTP config
#define UTC_OFFSET_IN_SECONDS 0 //This can be changed to any timezone, but I recommend using central time to sync data between timezones
#define NTP_HOST "time.google.com"
//#define NTP_HOST "pool.ntp.org  //Some alternative hosts...
#define NTP_REFRESH_INTERVAL 10000 //10 seconds

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
  
  //load saved settings from memory
  loadSettings();

  //If no user data is found, enter initial setup mode
  if (!EEPROMdata.firstSetupDone) {
    firstSetupMode();
  } else {
    normalSetup();
  }

}


void loop() {
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
      if(debug) Serial.println(F("Unresolvable problem with firebase! User account has probably changed. Initiating factory reset...."));
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

//END Netowrk communication ad stuff ---------------------
  
  //If full connection to the database is available receive data from it and send from queue to it
  if(databaseConnectionAvailable()){

    //Read data from database into incoming queue
    databaseFromProcess();

    //Send data from outgoing queue to database
    databaseToProcess();
    
  }

}

//This reinitialises firebase stream and stuff
void reinitFirebase() {
  firebaseInstance->endStream();
  for (uint8_t i = 0; i < MAX_FIREBASE_RETRIES; i++) {
    if(debug) Seiral.println(String(F("Heap before firebase:"))+String(ESP.getFreeHeap()));
    if(debug) Seiral.println(F("INIT/REINIT firebase connection"));
    firebaseInstance->begin(FIREBASE_URL, String(ACCESS_ID_TOKEN));
    if(debug) Seiral.println(String(F("Heap before stream but after begin:"))+String(ESP.getFreeHeap()));
    firebaseInstance->stream(String(FIREBASE_ROOT_PATH) + "/" + String(EEPROMdata.ACCESS_UID) + "/" + String(DEVICE_TYPE) + "/" + String(EEPROMdata.ACCESS_DEVICE_ID) + String(FIREBASE_DOWNSTREAM_PATH));
    long startTime = millis();
    while (millis() <= startTime + 1000 && !firebaseInstance->success());
    if(debug) Seiral.println(String(F("Heap after firebase:"))+String(ESP.getFreeHeap()));
    if (firebaseInstance->success()) {
      firebaseStarted = true;
      if(debug) Seiral.println(F("SUCCESS! Firebase reinitialised :D"));
      break;
    } else {
      firebaseStarted = false;
      if(debug) Seiral.println(F("FAIL! Firebase reinit failed!"));
    }

  }

}

//This function checks network conectivity
void pingNetwork(boolean force) {
  if (millis() > lastPing + PING_INTERVAL || force) {
    lastPing = millis();
    checkWifiConnection();
    if (!wifiConnected) {
      if(debug) Seiral.println(F("PING determined no wifi connection!"));
      wifiConnected = false;
      internetConnected = false;
      tokenConnected = false;
      firebaseStarted = false;
      return;
    }
    checkInternetConnection();
    if (!internetConnected) {
      if(debug) Seiral.println(F("No network! Mark firebase as offline!"));
      firebaseStarted = false;
      tokenConnected = false;
    }
  }
}


void databaseToProcess(){
  //Check for data to be sent to WiFi bus
  if(availableQueueWiFi()){
    //Send data to WiFi bus
    
    int device_id;
    String data = readFromQueueWiFi(&device_id);
    WiFiSend(device_id, data);
  }
  
}

void databaseFromProcess(){
  
  //Check for available data
  if(databaseAvailable()){
    int priority;

    //Read data and calculate priority
    String data = databaseRead(&priority);

    //Add message to outgoing queue
    addToQueuenRF24(priority, data);    
    
  }    
}

int databaseAvailable(){
  //Check for new data in db

  return false;
}


boolean databaseConnectionAvailable(){
  //Check for wifi connection

  //Check for internet availability

  //Check for database connection

  //Other required checks

  return false;
}

void nRF24ToProcess(){
  //Check for data to be sent to nRF24 bus
  if(availableQueuenRF24()){
    //Send data to nRF24 bus
    
    int device_id;
    String data = readFromQueuenRF24(&device_id);
    nRF24Send(device_id, data);
  }
}

String readFromQueuenRF24(int* device_id){
  //Read data from queue

  //Set correct device_id
  device_id = 0;

  return "";
}

String readFromQueueWiFi(int* device_id){
  //Read data from queue

  //Set correct device_id
  device_id = 0;

  return "";
}

boolean availableQueuenRF24(){
  //Check for data in nRF24 queue
  
  return false;
}

boolean availableQueueWiFi(){
  //Check for data in WiFi queue
  
  return false;
}


void nRF24FromProcess(){
  //Check for available data
  int available_id = nRF24Available();
  if(available_id){
    int priority;

    //Read data and calculate priority
    String data = nRF24Read(available_id, &priority);

    //Add message to outgoing queue
    addToQueueWiFi(priority, data);
    
  }  
}

void addToQueueWiFi(int priority, String data){
  //Insert data into queue at the correct position
  
}

void addToQueuenRF24(int priority, String data){
  //Insert data into queue at the correct position
  
}


int nRF24Available(){
  //Check the bus for incoming data

  return -1;
}

String nRF24Read(int device_id, int* priority_address){
  //Read data from device

  //Calculate data priority

  //Convert to String in format: [priority] [device_id] [timestamp] [data]
  priority_address = 0;
  return "";
}

String databaseRead(int* priority_address){
  //Read data from database

  //Calculate data priority

  //Convert to String in format: [priority] [device_id] [timestamp] [data]
  priority_address = 0;
  return "";
}

void nRF24Send(int device_id, String data){
  //Send data to device
  
}

void WiFiSend(int device_id, String data){
  //Send data to device
  
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
  if(debug) Serial.print(String(F("Sending to: "))+path+", data: "+data+".......");

  if (sendClient.begin(sendClientW, String("https://") + String(FIREBASE_URL) + String(FIREBASE_ROOT_PATH) + "/" + String(EEPROMdata.ACCESS_UID) + "/" + String(DEVICE_TYPE) + "/" + String(EEPROMdata.ACCESS_DEVICE_ID) + path + ".json?auth=" + String(ACCESS_ID_TOKEN))) {
    sendClient.addHeader("Content-Type", "text/plain");
    sendClient.addHeader("Connection", "close");
    sendClient.setTimeout(HTTP_TIMEOUT * 1000);
    if(debug) Serial.println(String(F("just before PUT! HEAP: "))+String(ESP.getFreeHeap()));
    int httpCode = sendClient.PUT(data);
    if (httpCode == 200) {
      if(debug) Serial.println(F("SUCCESS"));
    } else if (httpCode > 0) {
      if(debug) Serial.pPrintln(F("ERROR.... remote server problem(?)"));
    } else {
      if(debug) Serial.pPrintln(F("ERROR... network problem(?)"));
    }
    sendClient.end();
  }
}
