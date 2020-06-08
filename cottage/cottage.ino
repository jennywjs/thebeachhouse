/*
 Smart Product Project 3- Beach House
 by Olga Saadi and Jenny Wang
 5/31/2020

 
 */
#include "config.h"  // wifi credentials & API info go here
#include "apikey.h"

#include <U8g2lib.h>
#include <U8x8lib.h>

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

#include <EventManager.h>

#include <ArduinoJson.h>
#include <SPI.h>
#include <WiFiNINA.h>
#include <ArduinoHttpClient.h>
#include <TimeLib.h>
#define I2C_ADDRESS 0x18 // should not need to change

#include <WT2003S_Player.h>
#include <SeeedGroveMP3.h>
#include <KT403A_Player.h>
#include "wiring_private.h"

#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif

//---------------------------------
//         OLED Variables     
//---------------------------------
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// WiFi and HTTP objects
WiFiSSLClient wifi;
HttpClient GetClient = HttpClient(wifi, io_host, io_port);
HttpClient PostClient = HttpClient(wifi, io_host, io_port);


//---------------------------------
//      Defintions & Variables     
//---------------------------------

#define BUTTON1PIN 2  //BUTTON0 is used for banana bread Message
#define BUTTON2PIN 4 // Button for KG message
#define BUTTON3PIN 6  //BUTTON1 is used for confirmation
#define BUTTON4PIN A0  //BUTTON2 is used for rejection/cancellation
#define V30
#define LED A2 // CHOOSE PIN
#define NUMPIXELS      11  // Number of LEDs on strip

#define ShowSerial SerialUSB // the serial port used for displaying info and reading user input
#define COMSerial mySerial // the serial port used for UART communication with the mp3 player

Uart mySerial (&sercom0, 6, 7, SERCOM_RX_PAD_0, UART_TX_PAD_2);
void SERCOM0_Handler() 
{
    mySerial.IrqHandler();
}

#ifdef V20
  MP3Player<KT403A<Uart>> Mp3Player; // mp3 player object, v2.0
#endif
#ifdef V30
  MP3Player<WT2003S<Uart>> Mp3Player; // mp3 player object, v3.0
#endif

String inputCommand; // string for holding input commands


/////////////////////////////////// Declare our NeoPixel strip object:

Adafruit_NeoPixel strip(NUMPIXELS, LED, NEO_GRB + NEO_KHZ800);
int delayval = 500; // delay for half a second

 
//---------------------------------
//     Event & State Variables     
//---------------------------------

// Renaming events to have more useful names
// (you can add more and rename as needed)
#define EVENTBUTTON1DOWN EventManager::kEventUser0 // send message 1
#define EVENTBUTTON2DOWN EventManager::kEventUser1 // send message 2
#define EVENTBUTTON3DOWN EventManager::kEventUser2 // event accepted
#define EVENTBUTTON4DOWN EventManager::kEventUser3 // event denied
#define EVENTTIMER EventManager::kEventUser4 // event timer

// Create the Event Manager
EventManager eventManager;

// Create the different states for the state machine
enum SystemState_t {INIT, OFF, MESSAGE_RECEIVED};

// Create and set a variable to store the current state
SystemState_t currentState = INIT;


//---------------------------------
//              Setup  
//---------------------------------
// Put your setup code here, to run once:
void setup() {
  
  // Initialize serial communication
  Serial.begin(9600);
  
  //////////////////// Set up for API
  while (!Serial && millis()<6000); // wait for serial to connect (6s timeout)

  // Light LED while trying to connect
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH); 

  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    while (true); // don't continue
  }
  // Check for firmware version
  String fv = WiFi.firmwareVersion();
  if (fv < "1.0.0") {
    Serial.println("Please upgrade the firmware");
  }

  // attempt to connect to Wifi network:
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(wifi_ssid);
    
    // Connect to WPA/WPA2 network. SSID and password are set in config.h
    WiFi.begin(wifi_ssid, wifi_password);
    /*** Use the line below instead if no password required! ***/
    // WiFi.begin(wifi_ssid);

    // wait 10 seconds for connection:
    delay(10000);
  }
  Serial.println("WiFi successfully connected");
  digitalWrite(LED_BUILTIN, LOW); // turn off light once connected
  
  ////////////////////  Set up the OLED
  u8g2.begin(); 
  
  // Clear the buffer and configure font/color

  u8g2.setFontRefHeightExtendedText();
  u8g2.setDrawColor(1);
  u8g2.setFontPosTop();
  u8g2.setFontDirection(0);
  u8g2.setFont(u8g2_font_6x10_tf);



  // Attach event checkers to the state machine
  eventManager.addListener(EVENTBUTTON1DOWN, BEACH_HOUSE_SM);
  eventManager.addListener(EVENTBUTTON2DOWN, BEACH_HOUSE_SM);
  eventManager.addListener(EVENTBUTTON3DOWN, BEACH_HOUSE_SM);
  eventManager.addListener(EVENTBUTTON4DOWN, BEACH_HOUSE_SM);
  eventManager.addListener(EVENTTIMER, BEACH_HOUSE_SM);
  
    // Initialize state machine
  BEACH_HOUSE_SM(INIT,0);
}


//---------------------------------
//           Main Loop  
//---------------------------------
// Put your main code here, to run repeatedly:
void loop() {
  
  // Handle any events that are in the queue
  eventManager.processEvent();

  // Check for any new events
  //    Add all of your event checking functions here
  //    Rename and add more as needed
  
  OnBUTTON1DOWN();
  OnBUTTON2DOWN();
  OnBUTTON3DOWN();
  OnBUTTON4DOWN();
  OnTIMER();

}


//---------------------------------
//        Event Checkers  
//---------------------------------
/* These are functions that check if an event has happened, 
 * and posts them (and any corresponding parameters) to the 
 * event queue, to be handled by the state machine.
 *    Change function names as desired. */


void OnBUTTON1DOWN() {

  static int lastButtonReading = LOW;
  int thisButtonReading = digitalRead(BUTTON1PIN);

  // Check if this event happened (e.g., button is pressed, timer expired, etc.):
  //      If it did, update eventHappened flag (and parameter, if desired)
  if (thisButtonReading == HIGH && (thisButtonReading != lastButtonReading)) {
    eventManager.queueEvent(EVENTBUTTON1DOWN, 0); 
    delay(100);
  }
  lastButtonReading = thisButtonReading;
}


void OnBUTTON2DOWN() {

  static int lastButtonReading = LOW;
  int thisButtonReading = digitalRead(BUTTON2PIN);

  // Check if this event happened (e.g., button is pressed, timer expired, etc.):
  //      If it did, update eventHappened flag (and parameter, if desired)
  if (thisButtonReading == HIGH && (thisButtonReading != lastButtonReading)) {
    eventManager.queueEvent(EVENTBUTTON2DOWN, 0); 
    delay(100);
  }
  lastButtonReading = thisButtonReading;
}


void OnBUTTON3DOWN() {

  static int lastButtonReading = LOW;
  int thisButtonReading = digitalRead(BUTTON3PIN);

  // Check if this event happened (e.g., button is pressed, timer expired, etc.):
  //      If it did, update eventHappened flag (and parameter, if desired)
  if (thisButtonReading == HIGH && (thisButtonReading != lastButtonReading)) {
    eventManager.queueEvent(EVENTBUTTON3DOWN, 0); 
    delay(100);
  }
  lastButtonReading = thisButtonReading;
}

void OnBUTTON4DOWN() {

  static int lastButtonReading = LOW;
  int thisButtonReading = digitalRead(BUTTON4PIN);

  // Check if this event happened (e.g., button is pressed, timer expired, etc.):
  //      If it did, update eventHappened flag (and parameter, if desired)
  if (thisButtonReading == HIGH && (thisButtonReading != lastButtonReading)) {
    eventManager.queueEvent(EVENTBUTTON4DOWN, 0); 
    delay(100);
  }
  lastButtonReading = thisButtonReading;
}

void OnTIMER() {
  
  static int startTime = millis();       // Change type as needed
  int totaLength = millis() - startTime;

  // Check if this event happened (e.g., button is pressed, timer expired, etc.):
  //      If it did, update eventHappened flag (and parameter, if desired)
  if (totaLength >= 60000) {
    startTime = millis();
    eventManager.queueEvent(EVENTTIMER, 0);
  }
}



//---------------------------------
//           State Machine  
//---------------------------------
/* Use this function to create your state machine, which responds 
 * to events based on the current state. Remember to update 
 * the name of your state machine and the names of any events 
 * and states! */
 
void BEACH_HOUSE_SM( int event, int param )
{
  // Initialize next state
  SystemState_t nextState = currentState;

 // Handle events based on the current state
  switch (currentState) {
    case INIT:
      Serial.println("STATE: Initialization");
      pinMode(BUTTON1PIN, INPUT);

      // Set up the LED Strip
      strip.begin();           // INITIALIZE NeoPixel strip object (REQUIRED)
      strip.show();            // Turn OFF all pixels ASAP
      strip.setBrightness(250); // Set BRIGHTNESS 
      
      
      // LEDs shine white to show initialization
      LED_ON(250,250,250);
      LED_OFF;     
      delay (3000);
      
      // PRINT IN LED SCREEN "READY"   
      Serial.println("System Ready");
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_VCR_OSD_tf);
      u8g2.drawStr(20, 16, "System Ready");
      u8g2.setFont(u8g2_font_7x13_tf);
      u8g2.sendBuffer();
      
      // Transition to a different state
      nextState = OFF; 
      break;


    case OFF:
      Serial.println("STATE: OFF");
      strip.clear();
      u8g2.clearBuffer();
      
      if(event == EVENTTIMER){
        Serial.println("Response being received");
        String currentValue = GetData();
        
        if (currentValue == "bananabread"){
          LED_ON(0,250,0);
          Serial.println("bananabread");
          u8g2.clearBuffer();
          u8g2.setFont(u8g2_font_VCR_OSD_tf);
          u8g2.drawStr(20, 16, "banana bread time!");
          u8g2.setFont(u8g2_font_7x13_tf);
          u8g2.sendBuffer();
          nextState = MESSAGE_RECEIVED;
          } 

        else if (currentValue == "friday"){        
          LED_ON(250,0,0);
          Serial.println("friday");
          u8g2.clearBuffer();
          u8g2.setFont(u8g2_font_VCR_OSD_tf);
          u8g2.drawStr(20, 16, "friday!!");
          u8g2.setFont(u8g2_font_7x13_tf);
          u8g2.sendBuffer();
          nextState = MESSAGE_RECEIVED;
        }   
      }       
      break;
    
    case MESSAGE_RECEIVED:
      Serial.println("STATE: MESSAGE_RECEIVED");
      
      //RESPONSE CHECKER FUNCTION, STAY IN THIS STATE UNTIL RESPONSE IS IDENTIFIED  

      if (event == EVENTBUTTON3DOWN){
      PostData("accepted");
      Serial.println("YESSSS");
      LED_ON(250,250,250);
      nextState=OFF;
      }
      
      if (event == EVENTBUTTON4DOWN){
      PostData("rejected");
      Serial.println("No Thanks");
      LED_ON(250,250,250);
      nextState=OFF;    
      }   
      break; 

    default:
      Serial.println("STATE: Unknown State");
      break;
  }
    
  // Update the current state
  currentState = nextState;
  
  }


//---------------------------------
//        Helper Functions     
//---------------------------------

void LED_ON (int x, int y, int z)
{   
  for(int i=0;i<NUMPIXELS;i++)
      {
       strip.setPixelColor(i, x, y, z);
       strip.show();
       Serial.println(i);
       delay(100); // Delay for a period of time (in milliseconds)
      }
}

void LED_OFF()
{
  for(int i=0;i<NUMPIXELS;i++)
      {
       strip.setPixelColor(i, 0,0, 0);
       strip.show();
       Serial.println(i);
       delay(100); // Delay for a period of time (in milliseconds)
      }
}
  


String GetData(){
  // Make sure we're connected to WiFi
  String output = "";
  if (WiFi.status() == WL_CONNECTED) {

    // Create a GET request to the advice path
    GetClient.get(io_response_path); // added codes
    Serial.println("[HTTP] GET... response Requested");

    // read the status code and body of the response
    int statusCode = GetClient.responseStatusCode();
    String response = GetClient.responseBody();
    
    // Check for the OK from the Server
    if(statusCode == 200) {
        Serial.println("[HTTP] GET received reply!"); 
        
        // Parse Server response as JSON document
        DynamicJsonDocument doc(GetClient.contentLength()); 
        deserializeJson(doc, response); 

        // Set output to the advice string from the JSON
        // The JSON looks like:   {"slip" : {"advice": advice_string}, ...}
        String currentValue = doc["last_value"];
        output = currentValue;
        Serial.println(currentValue);       
        
    } else if (statusCode > 0) {
        // Server issue
        Serial.print("[HTTP] GET... Received response code: "); 
        Serial.println(statusCode);
    } else {
        // Client issue
        Serial.print("[HTTP] GET... Failed, error code: "); 
        Serial.println(statusCode);
    }
  } else {
    Serial.println("[WIFI] Not connected");
  }
  
  Serial.println(output);
  return output;
} 

void PostData(String myMessage) {
  // Make sure we're connected to WiFi
  if (WiFi.status() == WL_CONNECTED) {

    Serial.println("[POST] Creating request with value: " + myMessage);
    PostClient.beginRequest();
    PostClient.post(io_path); // Add in the path for the Adafruit IO feed
    

    // Add message header with access key
    //    Parameter (String): https://io.adafruit.com/api/docs/#section/Authentication > HeaderKey
    //    Value (String): the api key
    PostClient.sendHeader("X-AIO-Key", io_key); // Add in the API key here

    // Add header with the content type of the message 
    //    Tell the server that the type of content we're sending is JSON
    PostClient.sendHeader("Content-Type", "application/json"); // fill in the header type to specify JSON data

    // Format myMessage to JSON
    DynamicJsonDocument doc(300);          // create object with arbitrary size 1000
    doc["value"] = myMessage;              // set { "value" : myMessage }
    String formatted_data;
    serializeJson(doc, formatted_data);    // save JSON-formatted String to formatted_data
    PostClient.sendHeader("Content-Length", formatted_data.length()); // fill in the header type to send the length of data

    // Post data, along with headers
    PostClient.beginBody();
    PostClient.print(formatted_data);
    PostClient.endRequest();

    // Handle response from Server
    int statusCode = PostClient.responseStatusCode();
    String response = PostClient.responseBody();
    if (statusCode == 200) {
      // Response indicated OK
      Serial.println("[HTTP] POST was successful!"); 
    } else if (statusCode > 0) {
      // Server response received
      Serial.print("[HTTP] POST... Received response code: "); 
      Serial.println(statusCode);
    } else {
      // httpCode will be negative on Client error
      Serial.print("[HTTP] POST... Failed, error: "); 
      Serial.println(statusCode);
    }
  } else {
    Serial.println("[WIFI] Not connected");
  }
}
  
