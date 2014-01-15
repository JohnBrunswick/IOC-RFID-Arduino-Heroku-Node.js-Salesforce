/*
  Sample code to read an RFID Tag ID and make HTTP PUT
  to service running on Heroku via Node.js.  Code sample created using
  some of the following resources
  - WiFi http://arduino.cc/en/Tutorial/WiFiWebClientRepeating
  - WEP http://arduino.cc/en/Tutorial/ConnectWithWEP
  - RFID (EM4x50 Tags) http://playground.arduino.cc/Learning/ParallaxRFIDreadwritemodule
  More at www.johnbrunswick.com
*/

#include 
#include 
#include 

char ssid[] = "XXXXX"; // your network SSID (name) 
char key[] = "XXXXX"; // your network key

// Keep track of web connectivity
boolean lastConnected = false;
boolean incomingData = false;
int status = WL_IDLE_STATUS; // the Wifi radio's status
int keyIndex = 0; // your network key Index number

// Take care of advoiding duplicate posts
unsigned long lastConnectionTime = 0;
const unsigned long postingInterval = 10*1000;

// Removing duplicate card reads
unsigned long lastReadTime = 0;
const unsigned long pollingInterval = 10*1000;

IPAddress server(192,168,1,100);  // numeric IP (no DNS)

// Initialize the Ethernet client library
// with the IP address and port of the server 
// that you want to connect to (port 80 is default for HTTP):
WiFiClient client;

#define RFID_READ 0x01
#define txPin 6
#define rxPin 8

SoftwareSerial mySerial(rxPin, txPin);
int val;
int runs = 0;

String completeRFIDCode;

void setup()
{
  Serial.begin(9600);
  Serial.println("------------------");
  Serial.println("Starting - RFID and WiFi POC Test");

  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }  

  // check for the presence of the shield:
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WiFi shield not present"); 
    // don't continue:
    while(true);
  } 

  // attempt to connect to Wifi network:
  while ( status != WL_CONNECTED) { 
    Serial.print("Attempting to connect to WEP network, SSID: ");
    Serial.println(ssid);
    status = WiFi.begin(ssid, keyIndex, key);

    // wait 10 seconds for connection:
    delay(10000);
  }

  Serial.println("Connected to WiFi...");
  Serial.println("------------------");
  Serial.println("");

  printWifiStatus();

  mySerial.begin(9600);
  pinMode(txPin, OUTPUT);    
  pinMode(rxPin, INPUT);
}

void suppressAll() // Suppresses the "null result" from being printed if no RFID tag is present
{
  if(mySerial.available() &gt; 0)
  { 
    mySerial.read();
    suppressAll();
  }
}

void loop()
{
  int val;
  completeRFIDCode = "";
  String segmentOne;
  String segmentTwo;
  String segmentThree;
  String segmentFour;

  mySerial.print("!RW");
  mySerial.write(byte(RFID_READ));
  mySerial.write(byte(32));

  if(mySerial.available() &gt; 0)
  {      
    // The mySerial.read() procedure is called, but the result is not printed because I don't want
    // the "error message: 1" cluttering up the serial monitor
    val = mySerial.read();
    // If the error code is anything other than 1, then the RFID tag was not read correctly and any data
    // collected is meaningless. In this case since we don't care about the resultant values they can be suppressed
    if (val != 1)
    {
      suppressAll();
    }                              
  }

  // Clear segmentOne
  segmentOne = "";

  // Had some issues around detection of different RFID types
  if(mySerial.available() &gt; 0) {      
    val = mySerial.read();
    segmentOne = String(val, HEX);
  }

  if(mySerial.available() &gt; 0) {        
    val = mySerial.read();
    segmentTwo = String(val, HEX);
  }

  if(mySerial.available() &gt; 0) {      
    val = mySerial.read();
    segmentThree = String(val, HEX);
  }

  if(mySerial.available() &gt; 0) {          
    val = mySerial.read();
    segmentFour = String(val, HEX);
  }  

  // If something was read in the first segment and it has been awhile since the last read, set an aggregated code for
  // the RFID code value - I am certain there is a more effective way, but this works for the proof
  if (segmentOne.length() &gt; 0 &amp;&amp; (millis() - lastReadTime &gt; pollingInterval)) {
    completeRFIDCode = segmentOne + "" + segmentTwo + "" + segmentThree + "" + segmentFour;
    Serial.println("&lt;-----------------");
    Serial.println("Card Detected: " + completeRFIDCode);
    Serial.println("&lt;-----------------");     Serial.println();     lastReadTime = millis();   }   else {     Serial.println("Polling...");   }     // Send data if we picked up anything via RFID   if (completeRFIDCode.length() &gt; 3)
  {
    // Ensure that we have not recently sent data to our service to stop duplicate posts
    if(!client.connected() &amp;&amp; (millis() - lastConnectionTime &gt; postingInterval)) {
      sendData();
    }
  }

  delay(750);
}

void sendData() {
  Serial.println("-----------------&gt;");
  Serial.println("Trying to send card data to server...");

  // if you get a connection, report back via serial:
  // change port info as needed, this was used with a local instance via Heroku command line
  if (client.connect(server, 3001)) {
    Serial.println("Connected to server...");

    String feedData = "n{\"carddata\" : {\"cardid\" : \"" + completeRFIDCode + "\"}}";
    Serial.println("Sending: " + feedData);

    client.println("PUT /card/ HTTP/1.0");

    client.println("Host: 192.168.1.100");
    client.println("Content-Type: application/json");
    client.println("Content-Length: " + String(feedData.length()));
    client.print("Connection: close");
    client.println();
    client.print(feedData);
    client.println();    

    Serial.println("Data has been sent...");
    Serial.println("-----------------&gt;");

    delay(3000); 
    lastConnectionTime = millis();
    lastConnected = client.connected();

    // Show any response from the service - helpful for debugging
    while (client.available() &amp;&amp; status == WL_CONNECTED) {
      if (incomingData == false)
      {
        Serial.println();
        Serial.println("&lt;-----------------");
        Serial.println("Receiving data from server...");
        incomingData = true;
      }
      char c = client.read();
      Serial.write(c);
    }

    // Clean up the connection
    client.flush();
    client.stop();

    if (incomingData == true) {
      Serial.println("&lt;-----------------");
      Serial.println("");
      incomingData = false; 
    }

    // Reset RFID code
    completeRFIDCode = "";

    lastConnected = client.connected();
  }
  else {
    Serial.println("*** ERROR: Failed connection ***");
  }
}

void printWifiStatus() {
  // Print the SSID of the network you're attached to:
  Serial.println("------------------");
  Serial.println("WiFi Status...");
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // Print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // Print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("Signal Strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
  Serial.println("------------------");
  Serial.println("");
}