//Include required libraries


//Define required constants
#define DEBUG //


//Initialize required variables and objects



void setup() {
  //Initialize all libraries and devices

}


void loop() {
  //Check for incoming data on the nRF24 bus and add it to queue
  nRF24FromProcess();

  //Send data to nRF24 from queue if available
  nRF24ToProcess();

  //If full connection to the database is available receive data from it and send from queue to it
  if(databaseConnectionAvailable()){

    //Read data from database into incoming queue
    databaseFromProcess();

    //Send data from outgoing queue to database
    databaseToProcess();
    
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
