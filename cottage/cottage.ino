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
#include <PubSubClient.h>

//---------------------------------
//         OLED Variables     
//---------------------------------
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);


//---------------------------------
//      Defintions & Variables     
//---------------------------------

#define BUTTON1PIN 2  //BUTTON0 is used for banana bread Message
#define BUTTON2PIN 4 // Button for KG message
#define BUTTON3PIN A0  //BUTTON1 is used for confirmation
#define BUTTON4PIN A2  //BUTTON2 is used for rejection/cancellation
#define V30
#define LED A6 // CHOOSE PIN
#define NUMPIXELS      10  // Number of LEDs on strip

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

/////////////////////////////////// MQTT Setup:

// MQTT Parameters (defined in config.h)
const char* mqttServer = ioHOST;            // Adafruit host
const char* mqttUsername = ioUSERNAME;      // Adafruit username
const char* mqttKey = ioKEY;                // Adafruit key
int mqttPort = defaultPORT;                 // Default MQTT Port
const char* feed = ioFeed;           //payload[0] (sent) should be set to current counter value
WiFiClient wifiClient;
PubSubClient client(wifiClient);


 
//---------------------------------
//     Event & State Variables     
//---------------------------------

// Renaming events to have more useful names
// (you can add more and rename as needed)
#define EVENTBUTTON1DOWN EventManager::kEventUser0 // send message 1
#define EVENTBUTTON2DOWN EventManager::kEventUser1 // send message 2
#define EVENTBUTTON3DOWN EventManager::kEventUser2 // event accepted
#define EVENTBUTTON4DOWN EventManager::kEventUser3 // event denied
#define EVENTCALLBACK EventManager::kEventUser4 // event timer

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

  ///////////////////////////////////// Connect to WiFi:
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);
  WiFi.begin(wifi_ssid, wifi_password);
  
  while (WiFi.status() != WL_CONNECTED) 
  {
    // wait while we connect...
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected!");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("------------------------");

  // Set up MQTT
  client.setServer(mqttServer, mqttPort);
  client.setCallback(OnCALLBACK); // set function to be called when new messages arrive from a subscription
    
  
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
  eventManager.addListener(EVENTCALLBACK, BEACH_HOUSE_SM);
  
    // Initialize state machine
  BEACH_HOUSE_SM(INIT,0);
}


//---------------------------------
//           Main Loop  
//---------------------------------
// Put your main code here, to run repeatedly:
void loop() {

  // Maintain MQTT Connection
  connectMQTT();
    
  // Handle any events that are in the queue
  eventManager.processEvent();

  // Check for any new events
  //    Add all of your event checking functions here
  //    Rename and add more as needed
  
  OnBUTTON1DOWN();
  OnBUTTON2DOWN();
  OnBUTTON3DOWN();
  OnBUTTON4DOWN();

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

void OnCALLBACK(char* path, byte* value, unsigned int value_length){
  Serial.println("This is being called");
  char response[value_length+1];
//  String response = "";
  for (int i = 0; i < value_length; i++) {
    response[i] = (char)value[i];
    //Serial.println(response[i]);
  }
  response[value_length] = 0;
  
  //Serial.println(response);
  String response_new = String(response);
  
  if (response_new == "bananabread") {
    Serial.println("bananabread!");
    eventManager.queueEvent(EVENTCALLBACK, 1);
  } 
  else if (response_new == "friday") {
    Serial.println("friday!");
    eventManager.queueEvent(EVENTCALLBACK, 0);
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
      
      // The below lines are also necessary to configure pins 6/7 as serial
      pinPeripheral(6, PIO_SERCOM_ALT); 
      pinPeripheral(7, PIO_SERCOM_ALT);
    
      ShowSerial.begin(9600); // initialize both serial ports
      COMSerial.begin(9600);
      while (!ShowSerial); // wait for serial to connect //while(!ShowSerial&&(millis()<5000)))
      while (!COMSerial);

      Mp3Player.controller->init(COMSerial); // initialize the MP3 player
      
      // Set up the LED Strip
      strip.begin();           // INITIALIZE NeoPixel strip object (REQUIRED)
      strip.show();            // Turn OFF all pixels ASAP
      strip.setBrightness(100); // Set BRIGHTNESS 
      
      
      // LEDs shine white to show initialization
      LED_ON(250,250,250);
      LED_OFF;     
      
      // PRINT IN LED SCREEN "READY"   
      Serial.println("System Ready");
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_VCR_OSD_tf);
      u8g2.drawStr(20, 16, "System Ready");
      u8g2.setFont(u8g2_font_7x13_tf);
      u8g2.sendBuffer();
      
      // Transition to a different state
      nextState = OFF; 
      Serial.println("Next State: OFF");  
      break;


    case OFF:
      Serial.println("STATE: OFF");
      LED_OFF;      
      u8g2.clearBuffer();
            
      if (event == EVENTCALLBACK && param == 1){ //banana message incoming
        LED_ON(250,250,250); //LEDs shine white to indicate incoming message
        Mp3Player.controller->playSDSong("jeffsound.mp3"); 
        Serial.println("bananabread");
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_VCR_OSD_tf);
        u8g2.drawStr(20, 16, "banana bread time!");
        u8g2.setFont(u8g2_font_7x13_tf);
        u8g2.sendBuffer();
        nextState = MESSAGE_RECEIVED;
        Serial.println("Next State: MESSAGE_RECEIVED");  
        } 

      else if (event == EVENTCALLBACK && param == 0){  //friday incoming      
        LED_ON(250,250,250); //LEDs shine white to indicate incoming message
        Mp3Player.controller->playSDSong("roysound.mp3"); 
        Serial.println("friday");
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_VCR_OSD_tf);
        u8g2.drawStr(20, 16, "friday!!");
        u8g2.setFont(u8g2_font_7x13_tf);
        u8g2.sendBuffer();
        nextState = MESSAGE_RECEIVED;
        Serial.println("Next State: MESSAGE_RECEIVED");  
      }   
    
      break;
    
    case MESSAGE_RECEIVED:
      Serial.println("STATE: MESSAGE_RECEIVED");
      static int startTime = millis();
      
      //RESPONSE CHECKER FUNCTION, STAY IN THIS STATE UNTIL RESPONSE IS IDENTIFIED
      //in this state, sound is only played once but LED display and lights stay on 
      //time out occurs in 30 minutes -- there wont be any food left after that anyway  

      if (event == EVENTBUTTON3DOWN){
      client.publish(feed, "accepted");
      Serial.println("YESSSS");
      LED_ON(0,250,0);
      nextState=OFF;
      Serial.println("Next State: OFF"); 
      }
      
      if (event == EVENTBUTTON4DOWN){
      client.publish(feed, "rejected");
      Serial.println("No Thanks");
      LED_ON(250,0,0);
      nextState=OFF;
      Serial.println("Next State: OFF");    
      }   

      if(startTime>1800000)
      {
        Serial.println("message timed out");
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
 delay(3000);
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
  
void connectMQTT() {
  
  // If we're not yet connected, reconnect
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    // Create a random client ID
    String clientId = "ArduinoClient-";
    clientId += String(random(0xffff), HEX);
   
    // Attempt to connect
    if (client.connect(clientId.c_str(), mqttUsername, mqttKey)) {
      // Connection successful
      Serial.println("successful!");

      // Subscribe to desired topics
      client.subscribe(feed);
      /* You can add more topics here as needed */
      // client.subscribe(subTopic2);
    } 
    else {
      // Connection failed. Try again.
      Serial.print("failed, state = ");
      Serial.print(client.state());
      Serial.println(". Trying again in 5 seconds.");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }

  // Process incoming messages and maintain connection to MQTT server
  client.loop();
  
}
  
