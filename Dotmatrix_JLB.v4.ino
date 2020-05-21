#include <MD_MAX72xx.h>
#include <SPI.h>

// Use the MD_MAX72XX library to scroll text on the display
//
// Demonstrates the use of the callback function to control what
// is scrolled on the display text.
//
// User can enter text on the serial monitor and this will display as a
// scrolling message on the display.
// Speed for the display is controlled by a pot on SPEED_IN analog in.

#include <MD_MAX72xx.h>
#include <SPI.h>

// start code from get BTC price
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

//Wifi details
char wifiSSID[] = "Your SSID"; // enter your WiFi SSID
char wifiPASS[] = "Your WiFi password"; // enter your WiFi password
String on_currency = "BTCUSD";
String on_sub_currency = on_currency.substring(3);
char conversion[20];
const uint16_t WAIT_TIME = 1000;
// end code from get BTC price

#define USE_POT_CONTROL 1
#define PRINT_CALLBACK  0
#define PRINT(s, v) { Serial.print(F(s)); Serial.print(v); }

// Define the number of devices we have in the chain and the hardware interface
// NOTE: These pin numbers will probably not work with your hardware and may
// need to be adapted
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW // if your LEDs look odd try replacing FC16_HW with one of these ICSTATION_HW, GENERIC_HW, PAROLA_HW

#define MAX_DEVICES 8 // Number of 8x8 LED devices
#define CLK_PIN   18  // or SCK
#define DATA_PIN  23  // or MOSI
#define CS_PIN    5   // or SS

int myLoop = 0;
int myClear = 0;
int myDelay = 0;
int nextMessage = 0;
String myMessage = "no message set";


// SPI hardware interface
MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);
// Arbitrary pins
//MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

// Scrolling parameters
#if USE_POT_CONTROL
#define SPEED_IN  A5
#else
#define SCROLL_DELAY  75  // in milliseconds
#endif // USE_POT_CONTROL

#define CHAR_SPACING  1 // pixels between characters

// Global message buffers shared by Serial and Scrolling functions
#define BUF_SIZE  285
char curMessage[BUF_SIZE];
char newMessage[BUF_SIZE];
bool newMessageAvailable = false;

uint16_t  scrollDelay;  // in milliseconds

void readSerial(void)
{
  static uint8_t  putIndex = 0;

  while (Serial.available())
  {
    newMessage[putIndex] = (char)Serial.read();
    if ((newMessage[putIndex] == '\n') || (putIndex >= BUF_SIZE-3)) // end of message character or full buffer
    {
      // put in a message separator and end the string
      newMessage[putIndex++] = ' ';
      newMessage[putIndex] = '\0';
      // restart the index for next filling spree and flag we have a message waiting
      putIndex = 0;
      newMessageAvailable = true;
    }
    else if (newMessage[putIndex] != '\r')
      // Just save the next char in next location
      putIndex++;
  }
}

void scrollDataSink(uint8_t dev, MD_MAX72XX::transformType_t t, uint8_t col)
// Callback function for data that is being scrolled off the display
{
#if PRINT_CALLBACK
  Serial.print("\n cb ");
  Serial.print(dev);
  Serial.print(' ');
  Serial.print(t);
  Serial.print(' ');
  Serial.println(col);
#endif
}

uint8_t scrollDataSource(uint8_t dev, MD_MAX72XX::transformType_t t)
// Callback function for data that is required for scrolling into the display
{
  static char   *p = curMessage;
  static uint8_t  state = 0;
  static uint8_t  curLen, showLen;
  static uint8_t  cBuf[8];
  uint8_t colData;

  // finite state machine to control what we do on the callback
  switch(state)
  {
    case 0: // Load the next character from the font table
      showLen = mx.getChar(*p++, sizeof(cBuf)/sizeof(cBuf[0]), cBuf);
      curLen = 0;
      state++;

      // if we reached end of message, reset the message pointer
      if (*p == '\0')
      {
        p = curMessage;     // reset the pointer to start of message
        if (newMessageAvailable)  // there is a new message waiting
        {
          strcpy(curMessage, newMessage);	// copy it in
          newMessageAvailable = false;
        }
      }
      // !! deliberately fall through to next state to start displaying

    case 1: // display the next part of the character
      colData = cBuf[curLen++];
      if (curLen == showLen)
      {
        showLen = CHAR_SPACING;
        curLen = 0;
        state = 2;
      }
      break;

    case 2: // display inter-character spacing (blank column)
      colData = 0;
      if (curLen == showLen)
        state = 0;
      curLen++;
      break;

    default:
      state = 0;
  }

  return(colData);
}

 void scrollText(void)
{
  static uint32_t	prevTime = 0;

  // Is it time to scroll the text?
  if (millis()-prevTime >= scrollDelay)
  {
    mx.transform(MD_MAX72XX::TSL);  // scroll along - the callback will load all the data
    prevTime = millis();      // starting point for next time
  }
}

uint16_t getScrollDelay(void)
{
#if USE_POT_CONTROL
  uint16_t  t;

  t = analogRead(SPEED_IN);
  t = map(t, 0, 1023, 25, 250);

  return(t);
#else
  return(SCROLL_DELAY);
#endif
}

void setup()
{
  mx.begin();
  mx.setShiftDataInCallback(scrollDataSource);
  mx.setShiftDataOutCallback(scrollDataSink);
  mx.control(MD_MAX72XX::INTENSITY, MAX_INTENSITY/32); // 2 is default
  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
  mx.clear();

#if USE_POT_CONTROL
  pinMode(SPEED_IN, INPUT);
#else
  scrollDelay = SCROLL_DELAY;
#endif

  //connect to local wifi            
  WiFi.begin(wifiSSID, wifiPASS);   
  while (WiFi.status() != WL_CONNECTED) {}
  
  Serial.begin(57600);
  Serial.print("\n[MD_MAX72XX Message Display]\nType a message for the scrolling display\nEnd message line with a newline");

  resetMatrix(); // print UPDATING on matrix
  on_rates(); // get current BTC price the first time
  strcpy(curMessage, conversion);
  
}

void loop()
{
if (myLoop < 15000000){ // this number (15000000) is a 3 minute delay to update price from API. adjust to your liking
  scrollDelay = getScrollDelay();
  readSerial();
  scrollText();
  myLoop = myLoop + 1;
}
else { // go update price. scrolling will pause while updating price.
  resetMatrix(); // print UPDATING on matrix
  myLoop = 0;
  on_rates(); // get current BTC price
  strcpy(curMessage, conversion);
}
}

// reset the matrix
void resetMatrix(void)
{
  blank_matrix();
   //                   ":::::::::::::::::::::::::67 characers::::::::::::::::::::::::::::::" use this line as a guide for 67 characters
  //String conversionn = ".:UPDATING:.:UPDATING:.:UPDATING:.:UPDATING:.:UPDATING:.:UPDATING:";
  String conversionn = ".:UPDATING:.:UPDATING:.:UPDATING:.:UPDATING:.:UPDATING:.:UPDATING:";
  conversionn.toCharArray(conversion, conversionn.length()+1);
  Serial.println(conversion);
  strcpy(curMessage, conversion);
  myDelay = 0;
  while (myDelay < 299999){ // 359999 was the old number
    scrollDelay = getScrollDelay();
    readSerial();
    scrollText();
    myDelay++; // add 1 to myDelay 
  }
  //blank_matrix();
}

void blank_matrix(){
  String conversionn = "                                                                   ";
  conversionn.toCharArray(conversion, conversionn.length()+1);
  Serial.println(conversion);
  strcpy(curMessage, conversion);
  myDelay = 0;
  while (myDelay < 15999){
    scrollDelay = getScrollDelay();
    readSerial();
    scrollText();
    myDelay++; // add 1 to myDelay 
  }
}


// Get current next message and current rate
void on_rates(){

// get the next message
//             ":::::::::::::::::::::::::67 characers::::::::::::::::::::::::::::::" use this line as a guide for 67 characters
if (nextMessage == 0){
  myMessage = "                Questions about Bitcoin?  Visit JustLearnBitcoin.com"; // MAX 67 characters
}
if (nextMessage == 1){
  myMessage = "                Value your wealth in BITCOIN!"; // MAX 67 characters
}
if (nextMessage == 2){
  myMessage = "                BITCOIN: Money without boarders!"; // MAX 67 characters
}
if (nextMessage == 3){
  myMessage = "                Just buy BITCOIN and HODL!!!"; // MAX 67 characters
}
if (nextMessage == 4){
  myMessage = "                BITCOIN: A known fixed issuance and a finite amount."; // MAX 67 characters
}
if (nextMessage == 5){
  myMessage = "                The one... The only.....    BITCOIN!!!"; // MAX 67 characters
}
if (nextMessage == 6){
  myMessage = "                BITCOIN: Unconfiscatable & uncensorable money."; // MAX 67 characters
}
if (nextMessage == 7){
  myMessage = "                Save up to 65% on transaction fees... Use SegWit"; // MAX 67 characters
}
if (nextMessage == 8){
  myMessage = "                Bitcoin is the next Bitcoin!"; // MAX 67 characters
}
if (nextMessage == 9){
  myMessage = "                The real innovation is happening on BITCOIN!!!"; // MAX 67 characters
}
if (nextMessage == 10){
  myMessage = "                Not your keys....   Not your Bitcoin!"; // MAX 67 characters
}
if (nextMessage == 11){
  myMessage = "                BITCOIN: The most SOUND MONEY ever created by man!!!"; // MAX 67 characters
}
if (nextMessage == 12){
  myMessage = "                BITCOIN never needs a bailout."; // MAX 67 characters
}
nextMessage++; // add 1 to nextMessage

if (nextMessage == 13) {
  nextMessage = 0;
}
  
     // Use WiFiClientSecure class to create TLS connection
  WiFiClientSecure client;

  if (!client.connect("www.bitstamp.net", 443)) {

    return;
  }

  String url = "/api/v2/ticker/btcusd"; // Bitstamp API


  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + "www.bitstamp.net" + "\r\n" + // Bitstamp website
               "User-Agent: ESP32\r\n" +
               "Connection: close\r\n\r\n");

  while (client.connected()) {

    
    String line = client.readStringUntil('\n');
    if (line == "\r") {

      break;
    }
  }
  String line = client.readStringUntil('\n');

const size_t capacity = 169*JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(168) + 3800;
DynamicJsonDocument doc(capacity);

deserializeJson(doc, line);

// The following code will format the price with a comma at the thousand mark.
// This code does NOT allow for a comma at the million mark...YET!
// If anyone knows a better way to do this, please show me.
int tempprice = doc["last"];
if (tempprice >= 1000) {
 int tempthousand = tempprice / 1000; // gets thousands out of price without rounding
 int temphundred = tempprice - tempthousand * 1000; // gets the price minus the thousands
  if (temphundred <= 99) {
  //String conversionn = "                Do you want lower transaction fees?...  Use SegWit!          BTC $" + String(tempthousand) + ",0" + String(temphundred); // account for 1 zero if between 10 and 99
  String conversionn = myMessage + "        BTC $" + String(tempthousand) + ",0" + String(temphundred); // account for 1 zero if between 10 and 99
  conversionn.toCharArray(conversion, conversionn.length()+2);
  Serial.println(conversion);
    if (temphundred <= 9) {
    String conversionn = myMessage + "        BTC $" + String(tempthousand) + ",00" + String(temphundred); // account for 2 zero if between 1 and 9
    conversionn.toCharArray(conversion, conversionn.length()+2);
    Serial.println(conversion);
    }
  }
  else {
  //String conversionn = "                Value your wealth in Bitcoin.          BTC $" + String(tempthousand) + "," + String(temphundred); // no extra zeros needed
  String conversionn = myMessage + "        BTC $" + String(tempthousand) + "," + String(temphundred); // no extra zeros needed
  conversionn.toCharArray(conversion, conversionn.length()+2);
  Serial.println(conversion);
  }
 }
 else {
  // price is less than $1,000 - no comma needed
  String conversionn = myMessage + "        BTC $" + String(tempprice);
    if (tempprice < 1){
    String conversionn = myMessage + "        BTC $ API UNREACHABLE";
  }
  conversionn.toCharArray(conversion, conversionn.length()+1);
  Serial.println(conversion);
  }

}
