# Main gateway
This is the source code for the main gateway. Bellow is the description of its functions and configuration.




## Program configuration
This section describes hard-coded "#define" values set in the Arduino IDE program

### ESP8266 configuration
 - **DEBUG** 
   - Format: *true* or *false*
   - true sends debug data to Serial port, while false is used for production 
 - **ACCESS_REFRESH_URL** 
   - Format: "`https://securetoken.googleapis.com/v1/token?key=\{API-KEY\}`"
   - `\{API-KEY\}` is the firebase project Web API key from the firebase settings
 - **ACCESS_TOKEN_POST_STRING_BEGIN**
   - Format: `"{\"grant_type\" : \"refresh_token\", \"refresh_token\" : \""`
   - First part of the POST data to be sent to aquire new ID token, refresh-token is appended programatically depending on the registered user
 -  **ACCESS_TOKEN_POST_STRING_END**
   - Format: "\" }"
   - Last part of the above string. Comes after the token. Current configuration sends JSON format as a workaround for some library issues
 - **FIREBASE_URL**
   - Format: `"\{PROJECT-NAME\}.firebaseio.com"`
   - `\{PROJECT-NAME\}` is the name of the firebase project. The whole link can usually be found on the [Firebase console](https://console.firebase.google.com/)
 - **FIREBASE_ROOT_PATH**
   - Format: `"/\{USERS-PATH\}"`
   - `\{USERS-PATH\}` is the path in the database where users are stored.
 - **DEVICE-TYPE**
   - Format: `"SENSOR-GATEWAY"`
   - This is the device type identificator. Child of `FIREBASE_ROOT_PATH/\{CURRENT-USER-ID\}` for the specified user in the database
 - **FIREBASE_DOWNSTREAM_PATH**
   - Format: `"/to_gateway"`
   - Can be changed if needed, but this is the direct child of DEVICE-TYPE in database. Contains data to be sent to the device.
 - **FIREBASE_UPSTREAM_PATH**
   - Format: `"from_gateway"`
   - Can be changed if needed, but this is the direct child of DEVICE-TYPE in database. Contains current states of the device.
 - **HTTP_TIMEOUT**
   - Format: number in **seconds**
   - Doesn't really seem to work, but this should specify the maximum waiting time for possibly failed HTTP PUT request
 - **MAX_FIREBASE_RETRIES**  
   - Format: number
   - Maximum amount of times to retry firebase downstream connection with valid internet connection before declaring expired token
 - **MAX_TOKEN_RETRIES**
   - Format: number
   - Maximum amount of times to retry ID token retrieval with valid internet connection before declaring user account change and resetting to factory settings
 - **PING_INTERVAL**
   - Format: number in milliseconds
   - How often to ping defined internet host in normal conditions to confirm Internet connection
 - **FIREBASE_FORCE_TOKEN_RELOAD_INTERVAL**
   - Format: time in millisesconds
   - How often to reload the ID token in normal conditions to make sure we're not using expired token
 - **FIREBASE_STREAM_RELOAD_INTERVAL** 
   -Format: time in milliseconds
   - How often to reload firebase receiving stream in normal conditions to make sure no unexpected situation interrupted it
 - **DEFAULT_AP_SSID** 
   - Format: String
   - Default SSID for the setup-mode Access Point. MAC address is appended to the end to make it uniqe for each device and avoid setup collision
 - **DEFAULT_APP_PASSWORD**
   - Format: String
   - Default setup AP password. Recommended length is 8 characters, but other combinations might work
 - **UTC_OFFSET_IN_SECONDS**
   - Format: number in **seconds**
   - This could allow to set timezone offset, but for the sake of keeping things syncronized with the server, leave this at 0
 - **NTP_HOST**
   - Format: String, NTP server link
   - Defines which online NTP server to use to sync current time with the device
 - **NTP_REFRESH_INTERVAL**
   - Format: number in milliseconds
   - How often to sync time with the NTP server to avoid drifting caused by internal clock inaccuracies
 - **TYPE_{device-type}**
   - Format: incrementing integer
   - This defines int values for all available sensor types. Used in processing to determin which data was sent and how to format it for firebase. Each new type added should be added in all functions that use this
 - **GATEWAY_ADDRESS**
   - Format: 00-06
   - nRF24 address of the gateway. Should allway be 00!
 - **CHANNEL**
   - Format: 0-124
   - nRF24 channel that all modules work on
 - **UNAVAILABLE_VALUE**
   - Format: number
   - This is the value sent in the packet structure for the values that the sending node does not support. Should be a value that is never expected in normal use
 - **PRIORITY_{priority-level}**
   - Format: incrementing integer
   - Defines available priority levels as integers to save memory.
 - **MAX_CONSECUTIVE_SENDS**
   - Format: integer
   - Maximum number of packets to send to firebase at once without looping through all other checks
 - **MAX_DEVICES**
   - Format: number:
   - Maximum amount of nodes (addresses) expected to be available to the gateway at some point
