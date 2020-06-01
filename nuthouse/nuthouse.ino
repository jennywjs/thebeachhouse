/*
 Smart Product Project 3- Beach House
 by Olga Saadi and Jenny Wang
 5/31/2020
 
 */
#include "config.h"  // wifi credentials & API info go here

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


//---------------------------------
//         OLED Variables     
//---------------------------------
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// WiFi and HTTP objects
WiFiSSLClient wifi;
HttpClient GetClient = HttpClient(wifi, ad_host, ad_port);

// Strings to GET and POST
String weather;

//---------------------------------
//      Defintions & Variables     
//---------------------------------

#define BUTTON1PIN 2  //BUTTON0 is used for toggling
#define BUTTON2PIN 4  //BUTTON1 is used for confirmation
#define BUTTON3PIN 6  //BUTTON2 is used for rejection/cancellation
#define V30

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


//---------------------------------
//     Event & State Variables     
//---------------------------------

// Renaming events to have more useful names
// (you can add more and rename as needed)
#define EVENTBUTTON1DOWN EventManager::kEventUser0
#define EVENTBUTTON2DOWN EventManager::kEventUser1
#define EVENTBUTTON3DOWN EventManager::kEventUser2


// Create the Event Manager
EventManager eventManager;

// Create the different states for the state machine
enum SystemState_t {INIT, OFF, SELECT_MESSAGE, WAIT_RESPONSE, RECEIVE_RESPONSE, RECEIVE_MESSAGE};

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

  
  ////////////////////  try to communicate with seria
  if (!tempsensor.begin(I2C_ADDRESS)) {
    Serial.println("Couldn't find MCP9808! Check your connections and verify the address is correct.");
    while (1);
  }  
  Serial.println("Found MCP9808!");
  tempsensor.setResolution(0); 
  
  ////////////////////  Set up the OLED
  u8g2.begin(); 
  
  // Clear the buffer and configure font/color

  u8g2.setFontRefHeightExtendedText();
  u8g2.setDrawColor(1);
  u8g2.setFontPosTop();
  u8g2.setFontDirection(0);
  u8g2.setFont(u8g2_font_6x10_tf);


  // Attach event checkers to the state machine
  eventManager.addListener(EVENTBUTTON0DOWN, BEACH_HOUSE_SM);
  eventManager.addListener(EVENTBUTTON1DOWN, BEACH_HOUSE_SM);
  eventManager.addListener(EVENTBUTTON2DOWN, BEACH_HOUSE_SM);
  
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
  
  OnBUTTON0DOWN();
  OnBUTTON1DOWN();
  OnBUTTON2DOWN();

}


//---------------------------------
//        Event Checkers  
//---------------------------------
/* These are functions that check if an event has happened, 
 * and posts them (and any corresponding parameters) to the 
 * event queue, to be handled by the state machine.
 *    Change function names as desired. */


void OnBUTTON0DOWN() {

  static int lastButtonReading = LOW;
  int thisButtonReading = digitalRead(BUTTONPIN0);

  // Check if this event happened (e.g., button is pressed, timer expired, etc.):
  //      If it did, update eventHappened flag (and parameter, if desired)
  if (thisButtonReading == HIGH && (thisButtonReading != lastButtonReading)) {
    eventManager.queueEvent(EVENTBUTTON0DOWN, 0); 
    delay(100);
  }
  lastButtonReading = thisButtonReading;
}


void OnBUTTON1DOWN() {

  static int lastButtonReading = LOW;
  int thisButtonReading = digitalRead(BUTTONPIN1);

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
  int thisButtonReading = digitalRead(BUTTONPIN2);

  // Check if this event happened (e.g., button is pressed, timer expired, etc.):
  //      If it did, update eventHappened flag (and parameter, if desired)
  if (thisButtonReading == HIGH && (thisButtonReading != lastButtonReading)) {
    eventManager.queueEvent(EVENTBUTTON2DOWN, 0); 
    delay(100);
  }
  lastButtonReading = thisButtonReading;
}


void OnTIMER() {
  
  static int startTime = millis();       // Change type as needed
  int totaLength = millis() - startTime;

  // Check if this event happened (e.g., button is pressed, timer expired, etc.):
  //      If it did, update eventHappened flag (and parameter, if desired)
  if (totaLength >= 5000) {
    startTime = millis();
    eventManager.queueEvent(EVENTTIMER1, 0);
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
      pinMode(BUTTONPIN, INPUT);
      
      // Transition to a different state
      Serial.println("    Transitioning to State: MODE1");
      readSensor();
      printSensor();
      GetData();
      nextState = MODE1;
      break;

    case OFF:
      Serial.println("STATE: MODE1");
      
      if(event == EVENTTIMER1){
        Serial.println("Shutdown MCP9808.... ");
        tempsensor.shutdown_wake(1); // shutdown MSP9808 - power consumption ~0.1 mikro Ampere, stops temperature sampling
        Serial.println("");
           
        readSensor();
        printSensor();
        if ((roomTemp > higherLimit) || (roomTemp < lowerLimit)){
          Serial.println("ALERT!!");
          u8g2.clearBuffer();
          u8g2.setFont(u8g2_font_VCR_OSD_tf);
          u8g2.drawStr(20, 16, "ALERT!");
          u8g2.setFont(u8g2_font_7x13_tf);
          u8g2.sendBuffer();
          nextState = ALERT;
          } 
      }

      else if (event == EVENTTIMER2){
        GetData();
        }
        
      else if(event == EVENTBUTTONDOWN){ 
        Serial.println("    Transitioning to State: MODE2");
        nextState = MODE2;
        Serial.println(roomTemp);
        Serial.println(averageTemp);
        Serial.println(minTemp);
        Serial.println(maxTemp);
        u8g2.clearBuffer();
        drawFloat(5, 40, roomTemp);
        drawFloat(41, 40, averageTemp);
        drawFloat(71, 40, minTemp);
        drawFloat(101, 40, maxTemp);
        u8g2.drawStr(5, 20, "Room");
        u8g2.drawStr(41, 20, "Avg");
        u8g2.drawStr(71, 20, "Min");
        u8g2.drawStr(101, 20, "Max");
        u8g2.sendBuffer();
      } 
      
      break; 
      
    case MODE2:
      Serial.println("STATE: MODE2");
      
      if(event == EVENTTIMER1){
        Serial.println("Shutdown MCP9808.... ");
        tempsensor.shutdown_wake(1); // shutdown MSP9808 - power consumption ~0.1 mikro Ampere, stops temperature sampling
        Serial.println("");
        nextState = MODE2;
        readSensor();
        Serial.println(roomTemp);
        Serial.println(averageTemp);
        Serial.println(minTemp);
        Serial.println(maxTemp);
        u8g2.clearBuffer();
        drawFloat(5, 40, roomTemp);
        drawFloat(41, 40, averageTemp);
        drawFloat(71, 40, minTemp);
        drawFloat(101, 40, maxTemp);
        u8g2.drawStr(5, 20, "Room");
        u8g2.drawStr(41, 20, "Avg");
        u8g2.drawStr(71, 20, "Min");
        u8g2.drawStr(101, 20, "Max");
        u8g2.sendBuffer();
        
        if ((roomTemp > higherLimit) || (roomTemp < lowerLimit)){
          Serial.println("ALERT!!");
          u8g2.clearBuffer();
          u8g2.setFont(u8g2_font_VCR_OSD_tf);
          u8g2.drawStr(20, 16, "ALERT!");
          u8g2.drawStr(20, 41, "So Hot!");
          u8g2.setFont(u8g2_font_7x13_tf);
          u8g2.sendBuffer();
          nextState = ALERT;
          } 
        } 
      
      else if (event == EVENTTIMER2){
        GetData();
        }
        
      else if(event == EVENTBUTTONDOWN){ 
        Serial.println("    Transitioning to State: MODE3");
        nextState = MODE3;
        Serial.println(current_text);
        Serial.println(current_icon);
        Serial.println(current_temp);
        u8g2.clearBuffer();
        drawString(5, 30,current_text);        
        drawIcon(50, 25, current_text);
        drawFloat(92, 30, current_temp);
        u8g2.sendBuffer();
        
        }  
      break; 

    case MODE3:
      Serial.println("STATE: MODE3");
      
      if(event == EVENTTIMER2){
        GetData();
        u8g2.clearBuffer();
        drawString(5, 30,current_text);        
        drawIcon(50, 25, current_text);
        drawFloat(92, 30, current_temp);
        u8g2.sendBuffer();
        } 
       
      else if (event == EVENTTIMER1){
        readSensor();
        if ((roomTemp > higherLimit) || (roomTemp < lowerLimit)){
          Serial.println("ALERT!!");
          u8g2.clearBuffer();
          u8g2.setFont(u8g2_font_VCR_OSD_tf);
          u8g2.drawStr(20, 16, "ALERT!");
          u8g2.drawStr(20, 41, "So Hot!");
          u8g2.setFont(u8g2_font_7x13_tf);
          u8g2.sendBuffer();
          nextState = ALERT;
          } 
        }
        
      else if(event == EVENTBUTTONDOWN){ 
        Serial.println("    Transitioning to State: MODE4");
        nextState = MODE4;
        Serial.print(day1_date);
        Serial.print(day1_text);
        Serial.print(day1_icon);
        Serial.print(day2_date);
        Serial.print(day2_text);
        Serial.print(day2_icon);
        Serial.print(day3_date);
        Serial.print(day3_text);
        Serial.print(day3_icon);
        u8g2.clearBuffer();
        drawString(5,5,day1_date);
        drawString(52,5,day2_date);
        drawString(92,5,day3_date);
        drawString(5,22,day1_text);
        drawString(52,22,day2_text);
        drawString(92,22,day3_text);
        drawIcon(5, 38, day1_text);
        drawIcon(52, 40, day2_text);
        drawIcon(92, 40, day3_text);
        u8g2.sendBuffer();
        
        }  
      break; 

    case MODE4:
      Serial.println("STATE: MODE4");

      if(event == EVENTTIMER2){
        GetData();
        Serial.print(day1_date);
        Serial.print(day1_text);
        Serial.print(day1_icon);
        Serial.print(day2_date);
        Serial.print(day2_text);
        Serial.print(day2_icon);
        Serial.print(day3_date);
        Serial.print(day3_text);
        Serial.print(day3_icon);
        u8g2.clearBuffer();
        drawString(5,5,day1_date);
        drawString(52,5,day2_date);
        drawString(92,5,day3_date);
        drawString(5,22,day1_text);
        drawString(52,22,day2_text);
        drawString(92,22,day3_text);
        drawIcon(5, 38, day1_text);
        drawIcon(52, 40, day2_text);
        drawIcon(92, 40, day3_text);
        u8g2.sendBuffer();
        
        } 

      else if (event == EVENTTIMER1){
      readSensor();
      if ((roomTemp > higherLimit) || (roomTemp < lowerLimit)){
        Serial.println("ALERT!!");
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_VCR_OSD_tf);
        u8g2.drawStr(20, 16, "ALERT!");
        u8g2.drawStr(20, 41, "So Hot!");
        u8g2.setFont(u8g2_font_7x13_tf);
        u8g2.sendBuffer();
        nextState = ALERT;
        } 
      }
      else if(event == EVENTBUTTONDOWN){ 
        Serial.println("    Transitioning to State: MODE1");
        nextState = MODE1;
        printSensor();
        }  
        
      break; 
      
    case ALERT:
      Serial.println("STATE: ALERT");
      if(event == EVENTBUTTONDOWN) { 
        nextState = MODE1;
        printSensor();
      } 
      else if (event == EVENTTIMER1) {
        readSensor();
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


void readSensor(){
  static int sumTemp = 0;
  static int count = 0;
  
  tempsensor.wake();   // wake up, ready to read!

  // Read and print out the temperature, also shows the resolution mode used for reading.
  roomTemp = round(tempsensor.readTempC());

  sumTemp = sumTemp + roomTemp;
  count ++;
  
  averageTemp = sumTemp / count;
  if (roomTemp < minTemp) {
    minTemp = roomTemp;
  }
    
  if(roomTemp > maxTemp) {
    maxTemp = roomTemp;
  }
 
}

void printSensor(){
  Serial.print("Temp: "); 
  Serial.print(roomTemp); Serial.print("*C\t");
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_VCR_OSD_tf);
  drawFloat(45,40,roomTemp);  
  u8g2.setFont(u8g2_font_7x13_tf);
  u8g2.drawStr(10, 20, "Room Temperature");
  u8g2.sendBuffer();
  
}


void GetData(){
  // Make sure we're connected to WiFi
  if (WiFi.status() == WL_CONNECTED) {

    // Create a GET request to the advice path
    GetClient.get(ad_path); // added codes
    Serial.println("[HTTP] GET... Weather Requested");

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
        current_text = (const char*)doc["current"]["weather"][0]["main"];
        current_icon = (const char*)doc["current"]["weather"][0]["icon"];
        current_temp = doc["current"]["temp"]; 
        
        time_t day1_date_raw = doc["daily"][1]["dt"];
        day1_text = (const char*)doc["daily"][1]["weather"][0]["main"];
        day1_icon = (const char*)doc["daily"][1]["weather"][0]["icon"]; 
        day1_date = String(String(month(day1_date_raw)) + "/" + String(day(day1_date_raw)));
        
        time_t day2_date_raw = doc["daily"][2]["dt"];
        day2_text = (const char*)doc["daily"][2]["weather"][0]["main"];
        day2_icon = (const char*)doc["daily"][2]["weather"][0]["icon"]; 
        day2_date = String(String(month(day2_date_raw)) + "/" + String(day(day2_date_raw)));
        
        time_t day3_date_raw = doc["daily"][3]["dt"];
        day3_text = (const char*)doc["daily"][3]["weather"][0]["main"];
        day3_icon = (const char*)doc["daily"][3]["weather"][0]["icon"];
        day3_date = String(String(month(day3_date_raw)) + "/" + String(day(day3_date_raw)));


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
}   

void drawFloat(int x, int y, int num) {
  String str = String(num) + "C";
  char text[str.length() + 1];
  str.toCharArray(text, str.length() + 1);
  u8g2.drawStr(x, y, text);
  }

void drawString(int x, int y, String words) {
  char text[words.length() + 1];
  words.toCharArray(text, words.length() + 1);
  u8g2.drawStr(x, y, text);
  }


void drawIcon(int x, int y, String day_text) {
  if (day_text == "Rain" ){
  u8g2.drawXBMP(x, y, rain_width, rain_height, rain_bits);
  }
  else if (day_text == "Clouds" ){
  u8g2.drawXBMP(x, y, few_clouds_day_width, few_clouds_day_height, few_clouds_day_bits);
  }
  else if (day_text == "Clear" ){
  u8g2.drawXBMP(x, y, clear_day_width, clear_day_height, clear_day_bits);
  }
  }
