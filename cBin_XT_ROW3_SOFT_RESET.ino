#include <WiFi.h>

#include <PubSubClient.h>


#include "HX711.h"
#include <SPI.h>
#include <MFRC522.h>

#include <FastLED.h>

#include <ArduinoOTA.h>

#include <ArduinoJson.h> 

String SSID_1;
String PSWD_1;
String IP_1;

#define address 2
bool send_data = false;
boolean nt_Flag = false;
String SLNO = "XT0000004";
#define SOURCE "cBin_XT02"
#define ACTIVE true
#define STATUS "REGISTERED"

#define MAX_NO_CARD_COUNT 4

#define LED_PIN 2//LED COLOUR IS GREEN

struct LEDState{
  bool isOn;
  unsigned long turnOnTime;
  unsigned long duration;
};

#define NUM_LEDS 20
CRGB leds[NUM_LEDS]; 
#define MAX_LEDS 10
LEDState ledStates[NUM_LEDS]; // Time each LED was turned on

#define RESERVE_WT 0.5
#define CRITIC_WT 0.25
#define TARE_TOL 0.02
#define SENSE_WT 0.025

//**************************************************************
#include <Arduino.h>
#ifdef ESP32
#include <WiFi.h>
#include <AsyncTCP.h>
#include <SPIFFS.h>
#else
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <Hash.h>
#include <FS.h>
#endif
#include <ESPAsyncWebServer.h>

AsyncWebServer server(80);

//IPAddress local_ip(15, 12, 15, 25);
//IPAddress gateway(15, 12, 15, 25);
//IPAddress subnet(255, 255, 255, 0);these commands are used to set for the custom ip

// REPLACE WITH YOUR NETWORK CREDENTIALS
const char* ssid = "cBin_XT_Config_XT0000004";
const char* password = "MATRIOT_PASS";

const char* PARAM_STRING = "SSID";
const char* PARAM_STRING_1 = "PASSWORD";
const char* PARAM_STRING_2 = "SERVERIP";

const char* mqttUser = "matriot_cbin_row3";
const char* mqttPassword = "matriot_row3";

const char *cleintID = "XT0000004";

//unsigned long previousRestartTime = 0;
//const unsigned long restartInterval = 3600000;

//unsigned long previousMillis[NUM_LEDS] = {0};

bool pick_to_light = false;

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

String readFile(fs::FS &fs, const char * path) {
  Serial.printf("Reading file: %s\r\n", path);
  File file = fs.open(path, "r");
  if (!file || file.isDirectory()) {
    Serial.println("- empty file or failed to open file");
    return String();
  }
  Serial.println("- read from file:");
  String fileContent;
  while (file.available()) {
    fileContent += String((char)file.read());
  }
  Serial.println(fileContent);
  return fileContent;
}

void writeFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Writing file: %s\r\n", path);
  File file = fs.open(path, "w");
  if (!file) {
    Serial.println("- failed to open file for writing");
    return;
  }
  if (file.print(message)) {
    Serial.println("- file written");
  } else {
    Serial.println("- write failed");
  }
}

// Replaces placeholder with stored values
String processor(const String& var) {
  //Serial.println(var);
  if (var == "SSID") {
    return readFile(SPIFFS, "/inputString.txt");
  }
  else if (var == "inputInt") {
    return readFile(SPIFFS, "/inputInt.txt");
  }
  else if (var == "SERVERIP") {
    return readFile(SPIFFS, "/inputFloat.txt");
  }
  return String();
}

//**************************************************************

HX711 scale1(16, 17); //(dout,clk);
HX711 scale2(26, 27); //(dout,clk);
HX711 scale3(21, 33); //(dout,clk);
HX711 scale4(32, 25); //(dout,clk);

float curWt1 = 0;
float curWt2 = 0;
float curWt3 = 0;
float curWt4 = 0;

int start_time;
int endtime;
float sum1 = 0;
float sum2 = 0;
float sum3 = 0;
float sum4 = 0;

float units;

#define RST_PIN         22          // Configurable, see typical pin layout above
#define SS_1_PIN        5         // Configurable, take a unused pin, only HIGH/LOW required, must be diffrent to SS 2
#define SS_2_PIN        13          // Configurable, take a unused pin, only HIGH/LOW required, must be diffrent to SS 1
#define SS_3_PIN        14         // Configurable, take a unused pin, only HIGH/LOW required, must be diffrent to SS 2
#define SS_4_PIN        15          // Configurable, take a unused pin, only HIGH/LOW required, must be diffrent to SS 1

#define NR_OF_READERS   4

byte ssPins[] = {SS_1_PIN, SS_2_PIN, SS_3_PIN, SS_4_PIN};

MFRC522 mfrc522[NR_OF_READERS];   // Create MFRC522 instance.

uint16_t xor_uid_1 = 0;
uint16_t xor_uid_2 = 0;
uint16_t xor_uid_3 = 0;
uint16_t xor_uid_4 = 0;

bool ext_light_flag_1 = false;
bool ext_light_flag_2 = false;
bool ext_light_flag_3 = false;
bool ext_light_flag_4 = false;

//char matriotServer[] = "34.229.135.98";   // Cloud MQTT - Sentry Server
//char matriotServer[] = "192.168.1.108";     // Local MQTT - Ramesh Laptop with JioFi connection
//char matriotServer[] = "192.168.1.110";
int matriotPort = 1883;

// Initialize the Ethernet client object
WiFiClient espClient;

PubSubClient client(espClient);

int status = WL_IDLE_STATUS;

//Change this calibration factor as per your load cell once it is found you many need to vary it in thousands
//float calibration_factor = -48100; //Strain Gauge 50kg    //-14050; //20kg  //13650     //-96650 for 5KG; //-106600 worked for my 40Kg max scale setup
float calibration_factor_1 = -26284; // this calibration factor is adjusted according to my load cell//+1 tentimes taken to calibrate
float calibration_factor_2 = -26907; // this calibration factor is adjusted according to my load cell
float calibration_factor_3 = -24071; // this calibration factor is adjusted according to my load cell
float calibration_factor_4 = -23095;

float baud_rate = 115200;
int setupDelay = 1000;

int wtAvgLoop = 0;
int wtAvgIter = 10;
int wtPrecision = 3;

float lastWt = -1000;
float Total_Weight = 0;

boolean weight_changed = false;

String lastStatus = "EMPTY";
String currStatus = "EMPTY";

long count = 0;

void setup()
{
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
    leds[0] = CRGB(0, 0, 0);
    leds[1] = CRGB(0, 0, 0);
    leds[2] = CRGB(0, 0, 0);
    leds[3] = CRGB(0, 0, 0);
    leds[4] = CRGB(0, 0, 0);
    FastLED.show();
    leds[0] = CRGB(255, 255, 255);
    leds[1] = CRGB(255, 255, 255);
    leds[2] = CRGB(255, 255, 255);
    leds[3] = CRGB(255, 255, 255);
    leds[4] = CRGB(255, 255, 255);
    FastLED.show();  
  Serial.begin(baud_rate);
  ap_mode_setup();
  Serial.println("MatrIoT Smart Bin with four Strain Gauges");
  Serial.print("Source : ");
  Serial.println(SOURCE);
  Serial.println("Press T to tare");
  long zero_factor1 = scale1.read_average(20); //Get a baseline reading
  long zero_factor2 = scale2.read_average(20); //Get a baseline reading
  long zero_factor3 = scale3.read_average(20); //Get a baseline reading
  long zero_factor4 = scale4.read_average(20); //Get a baseline reading
  Serial.print("Zero factor1: "); //This can be used to remove the need to tare the scale. Useful in permanent scale projects.
  Serial.println(zero_factor1);
  Serial.print("Zero factor2: "); //This can be used to remove the need to tare the scale. Useful in permanent scale projects.
  Serial.println(zero_factor2);
  Serial.print("Zero factor3: "); //This can be used to remove the need to tare the scale. Useful in permanent scale projects.
  Serial.println(zero_factor3);
  Serial.print("Zero factor4: "); //This can be used to remove the need to tare the scale. Useful in permanent scale projects.
  Serial.println(zero_factor4);
  scale1.set_offset(8040146);//8021613//8019264
  scale2.set_offset(8137036);//8196028//8198293
  scale3.set_offset(8326266);//513803//514122
  scale4.set_offset(8148966);//8063553//8062252
  scale1.set_scale(calibration_factor_1); //Adjust to this calibration factor
  scale2.set_scale(calibration_factor_2); //Adjust to this calibration factor
  scale3.set_scale(calibration_factor_3); //Adjust to this calibration factor
  scale4.set_scale(calibration_factor_4); //Adjust to this calibration factor
  SPI.begin();        // Init SPI bus

  for (uint8_t reader = 0; reader < NR_OF_READERS; reader++)
  {
    mfrc522[reader].PCD_Init(ssPins[reader], RST_PIN); // Init each MFRC522 card
    Serial.print(F("Reader "));
    Serial.print(reader);
    Serial.print(F(": "));
    mfrc522[reader].PCD_DumpVersionToSerial();
  }
  //*********************************************************************************************************************************//

  //InitWiFi();
  check_Ssid();

  //    client.setServer( matriotServer, matriotPort );
  //    client.setCallback(callback);

  Serial.println("Boot Success!");
  delay(setupDelay);
  InitWiFi();

}

//rfid will be reading continously if the rfid is found led turns green if not red
void checkrfid()
{
  if(ext_light_flag_1 == false) {
    if(xor_uid_1 == 0)
        {
    
             leds[0] = CRGB(255, 0, 0);
             FastLED.show();
        }
        else
        {
      
             leds[0] = CRGB(0, 255, 0);
             FastLED.show();          
        }
  }
  if(ext_light_flag_2 == false) {        
              if(xor_uid_2 == 0)
        {
        
             leds[1] = CRGB(255, 0, 0);
             FastLED.show();
        }
        else
        {
          
             leds[1] = CRGB(0, 255, 0);
             FastLED.show();          
        }
     }
  if(ext_light_flag_3 == false) {
              if(xor_uid_3 == 0)
        {
          
             leds[2] = CRGB(255, 0, 0);
             FastLED.show();
        }
        else
        {
         
             leds[2] = CRGB(0, 255, 0);
             FastLED.show();          
        }
     }

  if(ext_light_flag_4 == false) {
              if(xor_uid_4 == 0)
        {
          
             leds[3] = CRGB(255, 0, 0);
             FastLED.show();
        }
        else
        {
          
             leds[3] = CRGB(0, 255, 0);
             FastLED.show();          
        }
}
}

void loop()
{
  check_rfid_and_weight();
  checkrfid();
  
  for (uint8_t reader = 0; reader < NR_OF_READERS; reader++)
  {
    mfrc522[reader].PCD_Init(ssPins[reader], RST_PIN); // Init each MFRC522
  }
  
  if (nt_Flag == true)
  {
    delay(50);
    client.loop();
    if (!client.connected()) {
      mqtt_reconnect();
    }
    

    if (ACTIVE == true) {
      sum1 += measure_weight(1, scale1);
      //delay(100);
      sum2 += measure_weight(2, scale2);
      //delay(100);
      sum3 += measure_weight(3, scale3);
      //delay(100);
      sum4 += measure_weight(4, scale4);

      if (wtAvgLoop == wtAvgIter) {
        //float curWt = sum/10/1.33;    //5kg
        curWt1 = sum1 / 10;     //20KG
        curWt2 = sum2 / 10;     //20KG
        curWt3 = sum3 / 10;     //20KG
        curWt4 = sum4 / 10;     //20KG

        Total_Weight = curWt1 + curWt2 + curWt3 + curWt4;

        if ((Total_Weight - lastWt > SENSE_WT || Total_Weight - lastWt < (-1 * SENSE_WT)))
        {
          //start_time = millis();
          weight_changed  = true;

          lastStatus = currStatus;
          lastWt = Total_Weight;
        }

        if (weight_changed && (millis() - start_time) >= 150)
        {
          sendData(curWt1, curWt2, curWt3, curWt4, Total_Weight, currStatus);
          weight_changed = false;
        }
        //start_time = 0;
        sum1 = 0;
        sum2 = 0;
        sum3 = 0;
        sum4 = 0;
        wtAvgLoop = 0;
      } else {
        wtAvgLoop += 1;
      }
    }
    count += 1;

  }
  checkrfid();

check_and_updateLED();
}



//Initize the wifi connection
void InitWiFi()
{
  // Connect to WiFi network
  Serial.println();
  Serial.println();
  SSID_1 = readFile(SPIFFS, "/inputString.txt");
  PSWD_1 = readFile(SPIFFS, "/inputInt.txt");
  Serial.print("Connecting to ");
  Serial.println(SSID_1);
  WiFi.begin(SSID_1.c_str(), PSWD_1.c_str());
  //WiFi.begin(WIFI_AP, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
   // digitalWrite(DATA_LED_1, HIGH);
      leds[0] = CRGB(0, 0, 0);
   leds[1] = CRGB(0, 0, 0);
   leds[2] = CRGB(0, 0, 0);
   leds[3] = CRGB(0, 0, 0);
   leds[4] = CRGB(0, 0, 0);
   FastLED.show();
   leds[0] = CRGB(0, 0, 255);
   leds[1] = CRGB(0, 0, 255);
   leds[2] = CRGB(0, 0, 255);
   leds[3] = CRGB(0, 0, 255);
   leds[4] = CRGB(0, 0, 255);
   FastLED.show();
   // digitalWrite(DATA_LED_2, LOW);
    delay(5000);
    Serial.print(".");
  }
  Serial.println("");
  //digitalWrite(DATA_LED_1, LOW);
  //digitalWrite(DATA_LED_2, HIGH);
  Serial.println("WiFi connected");
  Serial.println("IP is");
  IP_1 = readFile(SPIFFS, "/inputFloat.txt");
  Serial.println(IP_1);
  client.setServer(IP_1.c_str(), matriotPort );
  //client.setServer("hairdresser.cloudmqtt.com", 18594);//src-testing
  //client.setServer( matriotServer, matriotPort );
  client.setCallback(callback);
  mqtt_reconnect();
}

/*-------MQTT handling SECTION Start-----------*/
/**
   @brief Function to reconnect MQTT server
   subscribes topics to the MQTT serverc

   @return none
*/

//check mqtt connection if not connected mqtt will be reconnected
void mqtt_reconnect() {

  while (!client.connected()) {
    Serial.println("Attempting MQTT connection...");
   // digitalWrite(DATA_LED_1, HIGH);
   leds[0] = CRGB(0, 0, 0);
   leds[1] = CRGB(0, 0, 0);
   leds[2] = CRGB(0, 0, 0);
   leds[3] = CRGB(0, 0, 0);
   leds[4] = CRGB(0, 0, 0);
   FastLED.show();
   leds[0] = CRGB(255, 0, 255);
   leds[1] = CRGB(255, 0, 255);
   leds[2] = CRGB(255, 0, 255);
   leds[3] = CRGB(255, 0, 255);
   leds[4] = CRGB(255, 0, 255);
   FastLED.show();
   delay(5000);

    //if ( client.connect("Client123", "ramesh", "sangee") ) {
    if ( client.connect(cleintID) ) {//src-testing
   leds[0] = CRGB(0, 0, 0);
   leds[1] = CRGB(0, 0, 0);
   leds[2] = CRGB(0, 0, 0);
   leds[3] = CRGB(0, 0, 0);
   leds[4] = CRGB(0, 0, 0);
   FastLED.show();
      Serial.println( "connected" );
      Serial.println(cleintID);
      Serial.println(mqttUser);
      Serial.println(mqttPassword);
      Serial.println("Subsribe to matriot/device/all");
      client.subscribe("matriot/device/all", 2);
    } else {
      Serial.print( "[FAILED][rc=" );
      Serial.print( client.state() );
      Serial.println( ":retry]" );
      // Wait 5 seconds before retrying 
    }
  }
  //Serial.println("Subsribe to toppic");//src
  String str = "matriot/cbin/" + String(SLNO);
  int str_len = str.length() + 1;
  char topic[str_len];
  str.toCharArray(topic, str_len);
  boolean i = client.subscribe(topic);
  //Serial.println(i);//src
  boolean j = client.subscribe("matriot/cbin/all");
  //Serial.println(j);//src
}

//check mqtt connection if not connected mqtt will be reconnected
void reconnect() {
  // Loop until we're reconnected
  //digitalWrite(DATA_LED, HIGH);
  Serial.println("MQTT CONNECTION CHECK");
  while (!client.connected()) {
    Serial.println("MQTT NOT CONNECTED. TRYING TO CONNECT...");
    // Attempt to connect (clientId, username, password)
    //    if ( client.connect("Arduino Uno Device", TOKEN, NULL) ) {
    //if ( client.connect("Client123", "ramesh", "sangee") ) {
    if ( client.connect(cleintID) ) {
      //    if ( client.connect(NULL, NULL, NULL) ) {
      Serial.println("MQTT CONNECTED and SUBSCRIBING TOPIC");
      client.subscribe("matriot/device/all");
      Serial.println( "[DONE]" );
    } else {
      Serial.print( "[FAILED][rc=" );
      Serial.print( client.state() );
      Serial.println( ":retry]" );
      // Wait 5 seconds before retrying
      delay( 5000 );
    }
  }
  //Serial.println("Subsribe to toppic");//src
  String str = "matriot/cbin/" + String(SLNO);
  int str_len = str.length() + 1;
  char topic[str_len];
  str.toCharArray(topic, str_len);
  boolean i = client.subscribe(topic);
  //Serial.println(i);//src
  boolean j = client.subscribe("matriot/cbin/all");
  //Serial.println(j);//src
}

// Callback function for based on the different set of command functions will be called
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived in topic: ");//src
  String topicString = String(topic);
  String msg;
  int reqID;
  Serial.println(topic);
  //src
  Serial.print("Message:");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    msg += (char)payload[i];
  }

if (topicString == "matriot/cbin/" + SLNO) {
 // Serial.println("running");
  int cmdIndex = msg.indexOf("cmd");
  if (cmdIndex != -1) {
      String cmd = msg.substring(8,9);
     // Serial.println(cmdIndex);
 // Serial.println("running2");
  
 // Serial.println(cmd);
      if (cmd == "5") {
        sendResponse(msg);
        Serial.println("sendResponse is activated");
      }if (cmd == "4"){
        controlLED(msg);
        Serial.println("controlLED is activated");
      }if (cmd == "3"){
        performOTAUpdate();
        Serial.println("OTA is Updating");
      }if(cmd == "2"){
        Healthcheck();
        Serial.println("Health Check is Activated");
      }if(cmd == "1"){
        Ping();
        Serial.println("Ping request is Activated");
      }if(cmd == "0"){
       restart();
       Serial.println("Device resetting to normal mode"); 
    }
  }
  Serial.println(msg);
  Serial.println("-----------------------");

 
  if (topicString == "matriot/cbin/all"){
    sendResponse(msg);
  } else if (topicString == "matriot/cbin/" + SLNO){
    sendResponse(msg);
  } else {
    Serial.print("We are not subscribed to this topic!");
  }
}
}

//Prepare the Json payload to send the weight and rfid readings 
void sendData(float reading_1, float reading_2, float reading_3, float reading_4, float Total_weightage, String state)
{
  Serial.println(client.connected());
  /*if (!client.connected() ) {
      mqtt_reconnect();
    }*/
  //digitalWrite(DATA_LED, HIGH);
  xor_uid_1 = 0;
  xor_uid_2 = 0;
  xor_uid_3 = 0;
  xor_uid_4 = 0;

  read_card();
  //delay(50);
  read_card();//read_card function is called twice because the chances of reading the card is more
  //delay(150);
  read_card();
  // Prepare a JSON payload string
  String payload = "{";
  payload += "\"slno\":\""; payload += SLNO; payload += "\",";
  payload += "\"readings\":\"";
  payload += "1-1_";
  payload += xor_uid_1; payload += "_";
  payload += reading_1; payload += ",1-2_";
  payload += xor_uid_2; payload += "_";
  payload += reading_2; payload += ",1-3_";
  payload += xor_uid_3; payload += "_";
  payload += reading_3; payload += ",1-4_";
  payload += xor_uid_4; payload += "_";
  payload += reading_4;
  payload += "\""; payload += "}";
  //payload += Total_weightage;

  // Send payload
  char attributes[payload.length() + 1];
  payload.toCharArray( attributes, payload.length() + 1);
  client.publish( "matriot/cbin/record", attributes );
  Serial.println( attributes );
  //digitalWrite(DATA_LED_1, HIGH);
  //digitalWrite(DATA_LED_2, LOW);
  delay(50);
  //digitalWrite(DATA_LED_1, LOW);
  //digitalWrite(DATA_LED_2, HIGH);
}


//Send response of weight and Rfid reading for thr requested channel based on the specified type
void sendResponse(String msg) {
  // Serial.println(client.connected());
  /*if (!client.connected() ) {
      mqtt_reconnect();
    }*/
  //digitalWrite(DATA_LED, HIGH);
  xor_uid_1 = 0;
  xor_uid_2 = 0;
  xor_uid_3 = 0;
  xor_uid_4 = 0;

  boolean wt = msg.indexOf("WT") > 0;
  boolean rf = msg.indexOf("RF") > 0;
  boolean channel[4];
  boolean argu = (wt || rf);
  
  channel[0] = msg.indexOf("1-1") > 0;
  channel[1] = msg.indexOf("1-2") > 0;
  channel[2] = msg.indexOf("1-3") > 0;
  channel[3] = msg.indexOf("1-4") > 0;
  boolean chann_st = (channel[0] || channel[1] || channel[2] || channel[3]);
      String cmd = msg.substring(8,9);
     // Serial.println("cmd"+cmd);
//  String cmd = msg.substring(msg.indexOf("\"cmd\":") + 7, msg.indexOf("\"cmd\"") + 8);
  String type = msg.substring(msg.indexOf("\"type\":") + 8, msg.indexOf("\"type\":") + 9);
  String reqId = msg.substring(msg.indexOf("\"reqId\":"), msg.indexOf("\"reqId\":") + 34);
  boolean id = msg.indexOf("reqId") > 0;

  read_card();
  //delay(50);
  read_card();//read_card function is called twice because the chances of reading the card is more
  //delay(150);
  uint16_t read_cardArray[] = {xor_uid_1, xor_uid_2, xor_uid_3, xor_uid_4};
  float weig_Array[] = {curWt1, curWt2, curWt3, curWt4};

  // Prepare a JSON payload string
  String payload = "";
 // Serial.println(type);
  //Serial.println(cmd == "5");
  if((cmd == "5") && (type == "0" || type == "1" || type == "2" || type == 0)) {
   // Serial.println("Inside if");
   // Serial.println(type);
    payload += "{";
    payload += "\"slno\":\""; payload += SLNO; payload += "\",";
    payload += "\"type\":\""; payload += type; payload += "\",";
    payload += "\"reading\":\"";
    int chanle_length = 4;
    for (int k = 0; k < chanle_length; k++)
    {
      if ((channel[k] == 1) || (!chann_st) ) {
        payload += "1-"; payload += (k + 1); payload += "_";
        if ((wt && rf) || (!argu)) {
          payload += read_cardArray[k]; payload += "_";
          payload += weig_Array[k];
        } else if (wt) {
          payload += weig_Array[k];
        } else if (rf) {
          payload += read_cardArray[k];
        }
        if (k < (chanle_length - 2)) {
          payload += ",";
        }
      }
    }
    payload += "\"";
    if (id > 0)
    {
      payload += ",";
      payload += reqId;
    }
    payload += "}";
    //payload += Total_weightage;
  }
  
    // Parse the message to extract LED index, color name, and duration

  // Send payload
  char attributes[payload.length() + 1];
  payload.toCharArray( attributes, payload.length() + 1);
  client.publish( "matriot/cbin/respond", attributes );
  Serial.println( attributes );
  //digitalWrite(DATA_LED_1, HIGH);
  //digitalWrite(DATA_LED_2, LOW);
  delay(50);
  //digitalWrite(DATA_LED_1, LOW);
  //digitalWrite(DATA_LED_2, HIGH);

}

float measure_weight(int i, HX711 obj)
{

  units = obj.get_units(), 10;
  if (units < 0)
  {
    units = 0.00;
  }


  return units;
  //}
}

//Read the rfid if the card/tag present
void read_card() {
  //uint16_t xor_uid = 0;

  for (uint8_t reader = 0; reader < NR_OF_READERS; reader++) {
    // Look for new cards
    uint16_t xor_uid = 0;
    //delay(50);
    if (mfrc522[reader].PICC_IsNewCardPresent() && mfrc522[reader].PICC_ReadCardSerial()) {
      //      Serial.println("New Card Present");
      //      Serial.println("Card is read");
      for (int i = 0; i < mfrc522[reader].uid.size; i = i + 2)  {
        xor_uid = xor_uid ^ (mfrc522[reader].uid.uidByte[i] << 8 | mfrc522[reader].uid.uidByte[i + 1]);
      }
      //delay(50);
      //Serial.print(xor_uid);
      //Serial.println("Started the for loop");
      if (reader == 0)
      {
        //Serial.println("Reader-1");
        xor_uid_1 = xor_uid;
       
      }
      
      if (reader == 1)
      {
        xor_uid_2 = xor_uid;
      }
      
      if (reader == 2)
      {
        xor_uid_3 = xor_uid;
      }
      if (reader == 3)
      {
        xor_uid_4 = xor_uid;
      }
    }

  } 
}

void ap_mode_setup() {
  // Initialize SPIFFS
#ifdef ESP32
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
#else
  if (!SPIFFS.begin()) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
#endif

  Serial.println("Configuring access point...");

  WiFi.softAP(ssid, password);

  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
  server.begin();

  Serial.println("Server started");


  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    int n = WiFi.scanNetworks();
    String st[n];
    String strssi[n];
    int cnt = 0;
    for (int i = 0; i < n; ++i) {
      // Print SSID and RSSI for each network found
      if (WiFi.RSSI(i) > -70) {
        st[cnt] = WiFi.SSID(i);
        strssi[cnt] = WiFi.RSSI(i);
        cnt++;
      }
    }
    for (int i = 0; i < cnt; ++i) {
      for (int sort = i + 1; sort < cnt; ++sort) {
        if (strssi[i ] > strssi[sort]) {
          String temp;
          String temp1;
          temp = strssi[i];
          strssi[i ] = strssi[sort];
          strssi[sort] = temp;
          temp1 = st[i];
          st[i] = st[sort];
          st[sort] = temp1;
        }
      }
    }

    String content = "\n\r\n<!DOCTYPE HTML>\r\n<title>configuration</title><html><center><h2> cBin configuration </h2><h2> Available WiFi connection </h2><h4> Please select or enter the WiFi network to connect</h4>";
    content += "<head><link rel='stylesheet' href='https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap.min.css'>";
    content += "<meta name='viewport' content='width=device-width, initial-scale=1'></head>";
    content += "<table class='table' border=1 cellpadding=5 cellspacing=5 ><th>select</th><th>SSID</th><th>RSSI</th>";
    for (int i = 0; i < cnt; i++) {
      content += "<tr>";
      content += "<td><input name='slct' type='radio' onclick='selectSSID(\"";
      content += st[i];
      content += "\")'></td>";
      content += "<td>";
      content += st[i];
      content += "</td>";
      content += " <td>";
      content += strssi[i];
      content += "</td>";
      content += "</tr>";

    }


    content += "</center></table>";
    content += "<script>";
    content += "function selectSSID(ssid){document.getElementById('ssid').value= ssid;console.log(ssid);}";
    content += "function isIpAddress(s) { if (typeof s !== 'string') { return false; } var parts = s.split('.');  for (var i = 0; i < 4; ++i) { var part = parts[i]; if (!/^\\d{1,3}$/.test(part)) { return false; } var n = +part; if (0 > n || n > 0xff) { return false; } } return true; }";
    content += " ";
    content += "function validation() { var ipAddress = document.getElementById('server').value; var isValid = isIpAddress(ipAddress); if(isValid == true || document.getElementById.value('server') == '') return true;";
    content += "else { alert('Invalid IP Address'); return false; }}";
    content += "</script>";
    content += "<div><form method='get' onsubmit='return validation()' action='/get'>";
    content += "<table><tr><td> <label>SSID:(%SSID%) </label> </td><td><input id='ssid' name='SSID' length='15' style='border-style: solid' required onkeydown='return false;'></td></tr><p></p>";
    /*content += "<style>body {background-color: HoneyDew;}form { background-color: LightBlue;}label{ padding:5px;margin: 10px;}input[type=submit] {background-color: #4CAF50;color: white; margin:10px; padding: 12px 20px; border: none; border-radius: 4px; cursor: pointer;} input { padding: 5px;font-size: 1em;margin:10px; }input { width: 70%;align: center;} .input[type=submit]";
      content += "{width:50px; height:50px;border-radius: 20px;} table ,tr,td{  padding:5px;margin: 5px; }</style><tr><td><label>Password:</label></td><td><input type='password' name='PASSWORD' length='15' size='150' style='border-style: solid'></td></tr> <tr><p><td><label>Server:</label></td><td><input class='option' id='server' name='SERVERIP' length='30' style='border-style: solid'></p></td></tr> <p></p></table>";*/
    content += "<tr><td><label>Password:</label></td><td><input type='password' name='PASSWORD' length='15' size='150' style='border-style: solid' required></td></tr> <tr><p><td><label>Server:(%SERVERIP%)</label></td><td><input type='text' minlength='7' maxlength='15' size='15' class='option' id='server' name='SERVERIP' length='30' style='border-style: solid'></p></td></tr> <p></p></table>";
    content += "<style>body {background-color: HoneyDew;}form { background-color: LightBlue;}label{ padding:5px;margin: 10px;}input[type=submit] {background-color: #4CAF50;color: white; margin:10px; padding: 12px 20px; border: none; border-radius: 4px; cursor: pointer;} input { padding: 5px;font-size: 1em;margin:10px; }input { width: 70%;align: center;} .input[type=submit]";
    content += "{width:50px; height:50px;border-radius: 20px;} table ,tr,td{  padding:5px;margin: 5px; }</style>";
    content += "<center><input type='submit' value='Submit' width='48' height='48'></center></div></form>";
    content += "</center></html>";
    int content_length = content.length() + 1;
    char html_page[content_length];
    content.toCharArray(html_page, content_length);
    request->send_P(200, "text/html", html_page, processor);
  });

  // Send a GET request to <ESP_IP>/get?inputString=<inputMessage>
  server.on("/get", HTTP_GET, [] (AsyncWebServerRequest * request) {
    String inputMessage, inputMessage1, inputMessage2 ;
    //request->send(200, "text/html", "Configurr Successfully");
    // GET inputString value on <ESP_IP>/get?inputString=<inputMessage>
    if (request->hasParam(PARAM_STRING) || request->hasParam(PARAM_STRING_1) || request->hasParam(PARAM_STRING_2) ) {
      inputMessage = request->getParam(PARAM_STRING)->value();
      inputMessage1 = request->getParam(PARAM_STRING_1)->value();
      inputMessage2 = request->getParam(PARAM_STRING_2)->value();
      Serial.println("inputMessage recieved");
      Serial.println(inputMessage);
      Serial.println(inputMessage1);
      Serial.println(inputMessage2);

      if (inputMessage.length() > 0)
        writeFile(SPIFFS, "/inputString.txt", inputMessage.c_str());
      if (inputMessage1.length() > 0)
        writeFile(SPIFFS, "/inputInt.txt", inputMessage1.c_str());
      if (inputMessage2.length() > 0)
        writeFile(SPIFFS, "/inputFloat.txt", inputMessage2.c_str());
      // To access your stored values on inputString, inputInt, inputFloat
      SSID_1 = readFile(SPIFFS, "/inputString.txt");
      Serial.print("*** SSID_1: ");
      Serial.println(SSID_1);

      PSWD_1 = readFile(SPIFFS, "/inputInt.txt");
      Serial.print("*** PSWD_1: ");
      Serial.println(PSWD_1);

      IP_1 = readFile(SPIFFS, "/inputFloat.txt");
      Serial.print("*** IP_1: ");
      Serial.println(IP_1);
      //InitWiFi();
      //request->send(200, "text/html", "Configurr Successfully");
      //WiFi.mode(WIFI_OFF);
      //Server.end();
      String content2 = "<html><link rel='stylesheet' href='https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap.min.css'>";
      content2 += "<style>div { width:300px; border: 5px solid black; margin-top: 30px }";
      content2 +="#white {  height: 15px; width: 15px; background-color: #ffffff; border-radius: 50%; display: inline-block; border: 2px solid black; margin-top: 6px; align-items:center;}";
      content2 += "#blue {  height: 15px; width: 15px; background-color: #0000ff; border-radius: 50%; display: inline-block; border: 2px solid black; margin-top: 6px; align-items:center;}";          
      content2 += "#purple {  height: 15px; width: 15px; background-color: #ff00ff; border-radius: 50%; display: inline-block; border: 2px solid black; margin-top: 6px; align-items:center;}";
      content2+= "#green {  height: 15px; width: 15px; background-color: #00ff00; border-radius: 50%; display: inline-block; border: 2px solid black; margin-top: 6px; align-items:center;}";
      content2 += "#red {  height: 15px; width: 15px; background-color: #ff0000; border-radius: 50%; display: inline-block; border: 2px solid black; margin-top: 6px; align-items:center;}";
      content2 += "</style>";
      content2 += "<body>";
      content2 +="<center><h1>Configuration Saved Successfully</h2><br><a href=\"/\">Return to Home Page</a>";
      content2 += "<h2> LED Status Indication</h2>";
      content2 += "<table class='table' border='1' cellpadding='5' cellspacing='5'><tbody>";
     content2 += "<tr><th>Color</th><th>Status</th></tr>";
     content2 += "<tr><td><span id='white'></span> <span>White</span> </td><td>Device is powered up</td></tr>";
     content2 += "<tr><td><span id='blue'></span> <span>Blue</span></td><td>Connecting to WIFI</td></tr>";
     content2 += "<tr><td><span id='purple'></span> <span>Purple</span> </td><td>Connecting to server</td></tr>";
     content2 += "<tr><td><span id='green'></span> <span>Green</span> </td><td>RFID is reading</td></tr>";
     content2 += "<tr><td><span id='red'></span> <span>Red</span> </td><td>RFID is not reading</td></tr>";
     content2 += "</tbody></table>";
     content2 += "</center></body></html>";
         int content_length = content2.length() + 1;
    char html_page[content_length];
    content2.toCharArray(html_page, content_length);
    request->send(200, "text/html", html_page);
      check_Ssid();
    }
    //Serial.print("inputMessage ");
    //Serial.println(inputMessage);
    //request->send(200, "text/html", conf_html);
    //Serial.println("html sent");
  });
  server.onNotFound(notFound);
  server.begin();
}

void check_Ssid()
{
  //digitalWrite(DATA_LED_1, HIGH);
  //digitalWrite(DATA_LED_2, LOW);
  SSID_1 = readFile(SPIFFS, "/inputString.txt");
  int numberOfNetworks = WiFi.scanNetworks();

  for (int i = 0; i < numberOfNetworks; i++) {
    Serial.println("-----------------------");
    Serial.print("Network name: ");
    Serial.println(WiFi.SSID(i));
    if (strcmp(SSID_1.c_str(), WiFi.SSID(i).c_str()) == 0)
    {
      nt_Flag = true;
      InitWiFi();
      delay(500);
      i = numberOfNetworks;
      //nt_Flag = true;
    }
    else
      nt_Flag = false;
    Serial.println("-----------------------");

  }
}

String toLowerCase(String str) {
    String result = str;
    result.toLowerCase();
    return result;
}


unsigned long previousMillis = 0;
bool ledOn = false;
unsigned long ledDuration = 0;

//Turn led light to desired color
void controlLED(String msg) {
  // Extract LED control information from msg
  StaticJsonDocument<200> doc;
  deserializeJson(doc, msg);
String channel[] = {"1-1", "1-2", "1-3", "1-4"};
  String type = doc["type"].as<String>();
  String color;
  
//On recieving the type "1", below operation will be executed
  if (type == "1") {
JsonObject args = doc["args"];
color = args["color"].as<String>();
int duration = args["duration"];

//Json for reading channel,color and duration
JsonArray channels = doc["channels"];
for (JsonVariant channel : channels) {
  String channelStr = channel["channel"];
  String channelColor = channel.containsKey("color") ? channel["color"] : color;
  int channelDuration = channel.containsKey("duration") ? channel["duration"] : duration;

  
bool channelHandled = false;

//Particlar channel Flag will set true 
for (int i = 0; i < 4; i++) {
    if (channelStr == "1-1" || "1-2" || "1-3" || "1-4") {
      ext_light_flag_1 = true;
      channelHandled = true;
    }
    if (channelStr == "1-1" || "1-2" || "1-3" || "1-4") {
      ext_light_flag_2 = true;
      channelHandled = true;
    }
    if (channelStr == "1-1" || "1-2" || "1-3" || "1-4") {
      ext_light_flag_3 = true;
      channelHandled = true;
    }
    if (channelStr == "1-1" || "1-2" || "1-3" || "1-4") {
      ext_light_flag_4 = true;
      channelHandled = true;
    }

  int ledIndex = channelStr.substring(2).toInt() - 1;
  if (ledIndex >= 0 && ledIndex < NUM_LEDS) {
    CRGB channelColorRGB;
     if (channelColor == "Red") {
          channelColorRGB = CRGB::Red;
        } else if (channelColor == "Green") {
          channelColorRGB = CRGB::Green;
        } else if (channelColor == "Blue") {
          channelColorRGB = CRGB::Blue;
        } else if (channelColor == "Yellow") {
          channelColorRGB = CRGB::Yellow;
        } else if (channelColor == "Orange") {
          channelColorRGB = CRGB::Orange;
        } else if (channelColor == "Cyan") {
          channelColorRGB = CRGB::Cyan;
        } else if (channelColor == "White") {
          channelColorRGB = CRGB::White;
        } else if (channelColor == "Pink") {
          channelColorRGB = CRGB::Pink;
        } else if (channelColor == "Brown") {
          channelColorRGB = CRGB::Brown;
        } else if (channelColor == "Purple") {
          channelColorRGB = CRGB::Purple;
        } else if (channelColor == "Plum") {
          channelColorRGB = CRGB::Plum;
        } else if (channelColor == "Chocolate") {
          channelColorRGB = CRGB::Chocolate;
        } else if (channelColor == "Tomato") {
          channelColorRGB = CRGB::Tomato;
        }

        leds[ledIndex] = channelColorRGB; // Set LED color
        ledStates[ledIndex].isOn = true;
        ledStates[ledIndex].turnOnTime = millis(); //Time will be turned on for the specic led
        ledStates[ledIndex].duration = channelDuration; //Duration of each channel led will be updated
  }
}
FastLED.show();


int reqID = doc["reqID"].as<int>(); // Fetching reqID as an integer
doc["reqID"] = String(reqID); // Converting reqID to string and adding it to the document
Serial.println("reqID: ");
Serial.println(reqID);
String message = "You have chosen Pick to Light, reqID: " + String(reqID);
client.publish("matriot/cbin/respond", message.c_str());
}
  }else if (type == "0") { //type 0 will reset the device for the particular channel specified
    String channelsStr = doc["channel"].as<String>();

    // Split the channel string by comma to get individual channel strings
    int commaIndex = channelsStr.indexOf(',');
    String channel1 = channelsStr.substring(0, commaIndex);
    String channel2 = channelsStr.substring(commaIndex + 1);

    
    // Create a JSON object or array
    DynamicJsonDocument doc1(200); // Adjust the size as needed

    // Assign values to the JSON object or array
    doc1["channel1"] = channel1;
    doc1["channel2"] = channel2;

    // Serialize the JSON object or array to a string
    String jsonString;
    serializeJson(doc1, jsonString);

    Serial.println(jsonString);

    // Your existing logic for setting flags can remain unchanged
    if (channel1 == "1-1") {
        ext_light_flag_1 = false;
    } else if (channel1 == "1-2") {
        ext_light_flag_2 = false;
    } else if (channel1 == "1-3") {
        ext_light_flag_3 = false;
    } else if (channel1 == "1-4") {
        ext_light_flag_4 = false;
    }
    if (channel2 == "1-1") {
        ext_light_flag_1 = false;
    } else if (channel2 == "1-2") {
        ext_light_flag_2 = false;
    } else if (channel2 == "1-3") {
        ext_light_flag_3 = false;
    } else if (channel2 == "1-4") {
        ext_light_flag_4 = false;
    }
    client.publish("matriot/cbin/respond", "Device is reset to normal mode");
}
check_and_updateLED();
}


//Check the led status and will turn off led and turns device to normal mode
void check_and_updateLED() 
{
  for (int i = 0; i < NUM_LEDS; i++) {
  
  if (ledStates[i].isOn && (millis() - ledStates[i].turnOnTime >= ledStates[i].duration)) {  //it will compare the led turn on duration if the duration elapsed led will be turned off
  //Serial.println(i);
   if (ledStates[i].duration != 0) { //it will check the duration is zero if the duration is zero led will not turn off 
  leds[i] = CRGB::Black; //Black turns the led off
  ledStates[i].isOn = false; 
  FastLED.show(); // this is the function for turning off the led

      switch(i) { // this switch case will turn off the individual led based on the duration 
        case 0:
          ext_light_flag_1 = false;
          break;
        case 1:
          ext_light_flag_2 = false;
          break;
        case 2:
          ext_light_flag_3 = false;
          break;
        case 3:
          ext_light_flag_4 = false;
          break;
      }

      Serial.println("checkrfid is called");
      ext_light_flag_1 = false;
      ext_light_flag_2 = false;
      ext_light_flag_3 = false;
      ext_light_flag_4 = false;
      checkrfid();
      client.publish("matriot/cbin/respond", "Device is reset to normal mode");
    }
  }
}
}

void check_rfid_and_weight()
{
      if(xor_uid_1 == 0 && curWt1 > 0.15)
        {
             leds[0] = CRGB::Red;
             FastLED.show();
             delay(100);
             leds[0] = CRGB::Black;
             FastLED.show();
             delay(100);
        }
        else
        {
            checkrfid(); 
        }
      if(xor_uid_2 == 0 && curWt2 > 0.15)
        {
             leds[1] = CRGB::Red;
             FastLED.show();
             delay(100);
             leds[1] = CRGB::Black;
             FastLED.show();
             delay(100);
        }
        else
        {
            checkrfid(); 
        }
      
      if(xor_uid_3 == 0 && curWt3 > 0.15)
        {
             leds[2] = CRGB::Red;
             FastLED.show();
             delay(100);
             leds[2] = CRGB::Black;
             FastLED.show();
             delay(100);
        }
        else
        {
            checkrfid(); 
        }
      if(xor_uid_4 == 0 && curWt4 > 0.15)
      {
        leds[3] = CRGB::Red;
        FastLED.show();
        delay(100);
        leds[3] = CRGB::Black;
        FastLED.show();
        delay(100);
      }
      else
      {
        checkrfid();
      }
}



    // Turn off LEDs after the duration
    //fill_solid(leds, NUM_LEDS, CRGB::Black); // Turn off all LEDs
    //FastLED.show(); // Show the turned-off LEDs
    //loop();
    //client.publish("matriot/cbin/all", "Pick-to-Light is stopped");

void performOTAUpdate()
{
  ArduinoOTA.onStart([](){
    Serial.println("OTA Update Starts");
  });
  ArduinoOTA.onEnd([](){
    Serial.println("Updated to latest version");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA Update Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Update Error[%u]: ", error);
  });
ArduinoOTA.begin();
}

void Healthcheck()//To Check The Device Health
{
  if(WiFi.status()){
    client.publish("matriot/cbin/respond","Device is connected to WIFI");
    Serial.println("WIFI is connected");
  }
  if (client.connected()) {
      client.publish("matriot/cbin/respond" , "Device is connected to server");
    Serial.println("Device is connected to server");
    }
}

void Ping()
{
  client.publish("matriot/cbin/respond","Ping request is recieved");
  Serial.println("Ping is Requested");
}

void restart()
{
  client.publish("matriot/cbin/respond", "Device returning to normal mode.......");
  ESP.restart(); //Restart the ESP Board
  Serial.print("Device returned to normal mode");
  
}
