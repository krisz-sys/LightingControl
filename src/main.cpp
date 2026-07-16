#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <String.h>
#include "DHT.h"
//#include "time.h"
#include <ModbusIP_ESP8266.h>
#include <ArduinoOTA.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncTCP.h>
#include <ArduinoJson.h>




#include "index.h"
// -------------------------------------- NTP server

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
String formattedDate;
String dayStamp;
String timeStamp;
long date_check_timer;
//--------------------------------------  UDP communication
unsigned int localUdpPort = 9800;
char incomingPacket[200];                               // buffer for incoming packets
char respond[200];
WiFiUDP Udp;
//char* remoteIp="";
//--------------------------------------- temp, humidity 
#define DHTPIN D7              // GPIO13
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
int humidity=-10;
float temperature=-10;
// --------------------------------------- comm protocol
#define COMM_ERR 56
#define TEMP_REQUEST 39
#define TEMP_FLAG 40
#define HUM_REQUEST 41
#define HUM_FLAG 42
#define TEMP_SENSOR_ERROR 55
#define MSG_RECEIVED 65
#define SET_LED 66
#define LED_REQUEST 67
#define SEND_BACK_LED 68
#define HUM_SENSOR_ERR 101                // the humidity sensor is damaged or don t works
#define BOSS 50
//---------------------------------------- led
#define LED D5                             // GPIO12
#define POT A0                              // potentiometer
#define VALVE D2
long unsigned potTimer;
int ledSPMode=1;                       // 0 => led SP from the POT,       1 => led SP from the sender,      2 => light increasing and decreasing
int lastSample=0;
bool firstStart=true;
int requestedVal=0;
//----------------------------------------wifi

const char *ssid = "yourSSID";          //Enter your wifi SSID
const char *password = "yourPassword";        //Enter your wifi Password
IPAddress local_IP(192, 168, 1, 230);       // a telefonba ezt az ip-t kell beírni
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);           //optional
IPAddress secondaryDNS(8, 8, 4, 4);

#define debug(x) Serial.print(x)
#define debugln(x) Serial.println(x)
float myPWM=0;                                  
long PWM_period=4000;                               // period of the PWM                     
int up_limit=0.95*PWM_period;                        // above this value the LED should turn ON

long unsigned reconnect_period=120*1000;
long  reconnectTimer;
long  tempSampleTimer;
long ledTurnedOffTimer;

float ledVal=0;

// ---------------------------------pot sampling
int maxSample=6;
int maxValue=0;
int sampleNum=0;
int sampleCounter=0;
double *potBuffer=(double*)malloc(sizeof(double)*maxSample);             // the samples are saved here
int currentPotAVG;

// ----------------------------------errors
bool isError=false;
long curentBlinkingPeriod=5000;
int networkError=5000;
long tempSamplePeriod=30000;
long serverUpdatePeriod=300;
long valveHandlePeriod=5000;
long errorTimer;
long serverUpdateTimer;
long valveHandleTimer;
bool ledState=false;
bool disablePotmeter=false;

int hystUpperLimit=35;
int hystLowerLimit=30;

signed int potChanging=0;

String sliderValue = "0";

const char* PARAM_INPUT = "value";

// Create AsyncWebServer object on port 80
AsyncWebServer server1(80);
AsyncWebSocket ws("/ws"); 


// Function to send value updates to clients
void notifyClients() {
  String finalString=String(float(requestedVal)/5.6)+", "+String(temperature,1);

  ws.textAll(finalString);  // send value to all connected clients
  
}

// Handle incoming WebSocket messages (optional)
void onWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;

  if (info->final && info->index == 0 && info->opcode == WS_TEXT) {

    // Safe copy
    char message[len + 1];
    memcpy(message, data, len);
    message[len] = 0;

    String msg = String(message);
    Serial.println("Received: " + msg);

    // Expecting message like: "Slider: 234"
  String prefix = "Slider: ";
    if (msg.startsWith(prefix)) {
      String sliderVal = msg.substring(prefix.length());

      float sliderValue = sliderVal.toFloat();
      requestedVal=int(sliderValue*5.6);
      //ledVal=sliderValue*5.6;
      ledSPMode=1;

      Serial.println("Parsed slider value: " + sliderVal);
    }
    prefix = "Button: ";
    if (msg.startsWith(prefix)) {
      String buttonVal = msg.substring(prefix.length());

      if(buttonVal.equals("ON")){
        //digitalWrite(VALVE,1);
      }
      else{
          //digitalWrite(VALVE,0);
      }

      Serial.println("Parsed button value: " + buttonVal);
    }


  }
  /*
  AwsFrameInfo *info = (AwsFrameInfo*)arg;

  if (info->final && info->index == 0 && info->opcode == WS_TEXT) {

    // Safe copy
    char message[len + 1];
    memcpy(message, data, len);
    message[len] = 0;

    String msg = String(message);
    Serial.println("Received: " + msg);

    // Expecting message like: "Slider: 234"
    String prefix = "Slider: ";

    prefix = "Button: ";

    if (msg.startsWith(prefix)) {
      String buttonVal = msg.substring(prefix.length());
      Serial.println("Button value: " + buttonVal);
    }

  }
  */
  
}

void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, 
                      AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("Client #%u connected\n", client->id());
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("Client #%u disconnected\n", client->id());
  } else if (type == WS_EVT_DATA) {
    onWebSocketMessage(arg, data, len);
  }
}


//--------------------------------------- modbus variables
ModbusIP mb;

int PotValueAddr=0;
int RequestedValAddr=1;
int LedSPModeAddr=2;
int AvaragePotSampleAddr=3;
int LedValAddr=4;
int TempAddr=5;
int HumAddr=6;
int MaxPotAddr=7;


void sliderRequest(AsyncWebServerRequest *request);
 void temp1Request(AsyncWebServerRequest *request);
 void connect2Wifi(int max_attempts);
 void ledHandler();
 void decodeAndBuildRespond();
 void getTime();
void sendUDP();
void readHumAndTemp();
bool temp2bytes(char*tempBytes);
void calculateMax();
double calculateAvarage();
void setAll(int val);


void setup() {
  Serial.begin(9600);

  dht.begin();
  analogWriteFreq(75); 
  pinMode(LED,OUTPUT);
  pinMode(VALVE,OUTPUT);
  pinMode(POT,INPUT);
  
  pinMode(LED_BUILTIN, OUTPUT);
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("STA Failed to configure");
  }
  debugln("");
  Serial.printf("Connecting to %s ", ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password); //Connect to wifi
  
  connect2Wifi(200);
  timeClient.begin();
  timeClient.setTimeOffset(3*3600);
  Udp.begin(localUdpPort);

  reconnectTimer=millis();
  potTimer=millis();
  date_check_timer=millis();               
  errorTimer=micros();
  tempSampleTimer=millis();
  serverUpdateTimer=millis();
  ledTurnedOffTimer=millis();
  // ---------------------------------- webserver

  server1.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html; charset=utf-8",index_html);
  });
  ws.onEvent(onWebSocketEvent);
  server1.addHandler(&ws);
  server1.begin();


  //---------------------------------- OTA

// Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
   ArduinoOTA.setHostname("ESP8266-LED controller");

  // No authentication by default
  ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else {  // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  Serial.println("Ready");

  // ------------------------------ modbus
  mb.server();            // default modbus port 502
  mb.addHreg(PotValueAddr, 0);              // 0
  mb.addHreg(RequestedValAddr, 0);          // 1
  mb.addHreg(LedSPModeAddr, 0);             // 2
  mb.addHreg(AvaragePotSampleAddr, 0);      // 3
  mb.addHreg(LedValAddr, 0);                // 4
  mb.addHreg(TempAddr, 0);                  // 5
  mb.addHreg(HumAddr, 0);                   // 6
  mb.addHreg(MaxPotAddr,0);                 // 7

  readHumAndTemp();
}

  void connect2Wifi(int max_attempts){
  int i=0;
  
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(200);
    Serial.print(".");
    if(i>=max_attempts){
      debugln("");
      debug("localIP: ");
      debugln(WiFi.localIP());
      isError=true;
      curentBlinkingPeriod=networkError;
      return;
    }
    i++;
  }
  isError=false;
}


void ledHandler(){
  int currentSample=analogRead(POT);      // taking a sample
  mb.Hreg(PotValueAddr, currentSample);
  potBuffer[sampleCounter++]=currentSample;
  if(sampleCounter>maxSample-1){
    sampleCounter=0;
  }
  sampleNum++;
  if(sampleNum>maxSample){
    sampleNum=maxSample;
  }

  calculateMax();

  float avarageSample=(float)calculateAvarage();                  // avarage filter for preventing the unwanted led blinking
  currentPotAVG=avarageSample;
  mb.Hreg(AvaragePotSampleAddr, avarageSample);
  //------------------------- end of the sample handling
  
  mb.Hreg(LedSPModeAddr, ledSPMode);
  if(ledSPMode==0){                                           // control from potmeter
    if(avarageSample>8){
      ledVal=avarageSample;
      requestedVal=ledVal;
      analogWrite(LED,avarageSample);
    }
    else{
      ledVal=0;
      requestedVal=ledVal;
      analogWrite(LED,ledVal);
    }
  }
  else if(ledSPMode==1){                                      // control from the user
    int diff=ledVal-requestedVal;
    if(diff<0){
      diff*=-1;
    }

    if(diff!=0 && diff<=1){
      //ledVal=requestedVal;
      analogWrite(LED,requestedVal);
    }
    else if(ledVal-float(requestedVal)>1.0 && diff!=0){                 //positive difference
      ledVal-=3;
      //analogWrite(LED,ledVal*1.8);
      analogWrite(LED,ledVal);
    }
    else if(ledVal-float(requestedVal)<1.0 && diff!=0){                 //negative difference
      ledVal+=3;
      //analogWrite(LED,ledVal*1.8);
      analogWrite(LED,ledVal);
    }
  }
  
    
  if(firstStart){                                      // if the controller just started, the POT donsn't have priority
    lastSample=currentSample;
    firstStart=false;
  }
  int diff=currentSample-lastSample;
  if(diff>0){
    potChanging--;
  }
  else if(diff<0){
    potChanging++;
  }
  else{
    potChanging=0;
  }

  

  if(potChanging>20 || potChanging<-20){
    ledSPMode=0;
    setAll(currentSample);            // when the potentiometer is unes we want to prevent the initial delay

  }
  lastSample=currentSample;
  mb.Hreg(LedValAddr, ledVal);
  
}

void decodeAndBuildRespond(){
  
  debug("incoming:  ");
  for(int i=0;i<incomingPacket[0];++i){
    debug((byte)incomingPacket[i]);
    debug(" ");
  }
  debugln(" ");
  
  int i=0;                    // counter of the receiver packet
  int j=1;              // counter of the respond packet
  int len=incomingPacket[i++];
  //char respond[50];
  //debug("len: ");
  //debugln(len);
  debug("");
  while(1){
    if(i>=len){
      break;
    }
    byte b=(byte)incomingPacket[i++];
    debugln(b);
    if(b==TEMP_REQUEST){
      readHumAndTemp();
      char tempbytes[2];
      respond[j++]=TEMP_FLAG;
      if(temp2bytes(tempbytes)){
        respond[j++]=tempbytes[0];
        respond[j++]=tempbytes[1];
      }
      else{
        respond[j++]=TEMP_SENSOR_ERROR;
        respond[j++]=TEMP_SENSOR_ERROR;
      }
    }
    if(b==HUM_REQUEST){
      readHumAndTemp();
      respond[j++]=HUM_FLAG;
      if(humidity!=-10){
        respond[j++]=humidity;
      }
      else{
        respond[j++]=HUM_SENSOR_ERR;
      }
    }
    if(b==SET_LED){
      /*          //OLD
      ledVal=(byte)incomingPacket[i++];
      ledSPMode=true;
      analogWrite(LED,ledVal*10);
      */

      //requestedVal=(byte)incomingPacket[i++];
      requestedVal=incomingPacket[i++]*1.8;
      mb.Hreg(RequestedValAddr, requestedVal);
      ledSPMode=1;
      
      if(len==3){
        respond[j++]=MSG_RECEIVED;
      }
    }
    if(b==LED_REQUEST){
        respond[j++]=SEND_BACK_LED;
        respond[j++]=(byte)(mb.Hreg(LedValAddr)/1.8);
        debugln("led val requested");
      }
  }

  
  respond[j]='\0';
  respond[0]=j-1;
  debug("Byte code:  ");
  for(i=0;i<j;++i){
    debug((byte)respond[i]);
    debug(" ");
  }
  debugln(" ");
  debugln("");
  incomingPacket[0]=0;

  debugln(j);
    //int lengt=strlen(respond);
    for (int i=0;i<j;++i){
      respond[i]=(char)((int)respond[i]+1);
      debug((byte)respond[i]);
      debug(" ");
    }
  //return respond;
}

void getTime(){
  for (int i=0;i<3;++i){
    timeClient.forceUpdate();
    if(timeClient.update()){
      break;
    }
  }
  //formattedDate = timeClient.getFormattedDate();
  formattedDate = timeClient.getFormattedTime();
  

  // Extract date
  int splitT = formattedDate.indexOf("T");
  dayStamp = formattedDate.substring(0, splitT);
  
  timeStamp = formattedDate.substring(splitT+1, formattedDate.length()-4);

  Serial.println(formattedDate);
  Serial.print("DATE: ");
  Serial.println(dayStamp);
  Serial.print("HOUR: ");
  Serial.println(timeStamp);
}

int receiveUDP(){
  int packetSize = Udp.parsePacket();
  if (packetSize)
  {
    // receive incoming UDP packets
    //Serial.printf("Received %d bytes from %s, port %d\n", packetSize, Udp.remoteIP().toString().c_str(), Udp.remotePort());
    //const char* cRemIP=Udp.remoteIP().toString().c_str();
    //remoteIp=Udp.remoteIP();
    //const char* cRemIP=Udp.remoteIP();
    //debugln(typeid(Udp.remoteIP()).name());
    //strcpy(remoteIp,cRemIP);
    int len = Udp.read(incomingPacket, 255);
    debugln(len);
    debugln((int)incomingPacket[1]);
    if((int)incomingPacket[1]!=BOSS){
        return 0;
    }
    if (len > 0)
    {
      incomingPacket[len] = 0;
    }
    //Serial.printf("UDP packet contents: %s\n", incomingPacket);
  }
  return packetSize;
}

void sendUDP(){
    Udp.beginPacket(Udp.remoteIP(), localUdpPort);
    Udp.write(respond);
    Udp.endPacket();  


    debugln("");
    debug("remoteIP:  ");
    debugln(Udp.remoteIP());
    debug("respond:  ");
    
    for(int i=0;i<strlen(respond);++i){
      debug((byte)respond[i]);
      debug(" ");
    }
    
    debugln("");
}


void readHumAndTemp(){

  humidity = dht.readHumidity();
  float temp= dht.readTemperature();
  //temperature = dht.computeHeatIndex(temp, humidity, false);    

  temperature = dht.readTemperature()-5;

  if(isnan(humidity)){
    humidity=-10;
  }
  if(isnan(temperature)){
    temperature=-10;
  }
  
  mb.Hreg(HumAddr, humidity);
  mb.Hreg(TempAddr, temperature*10);
}

bool temp2bytes(char*tempBytes){
  if(temperature!=-10){
    double fractpart, intpart; 
    fractpart = modf (temperature , &intpart);
    tempBytes[0]=(char)intpart;
    tempBytes[1]=(char)(fractpart*100);
    return true;
  }
  return false;
}

void calculateMax(){
  //potBuffer[sampleCounter++]=currentSample;
  int max=0;
  for(int i=0;i<maxSample;++i){
    if(max<potBuffer[i]){
      max=potBuffer[i];
    }
  }
  maxValue=max;
  mb.Hreg(MaxPotAddr, maxValue);
}

double calculateAvarage(){
  double avarage=0;
  for(int i=0;i<sampleNum;++i){
    avarage+=potBuffer[i];
  }
  //Serial.println("");
  return avarage/sampleNum;
}
void setAll(int val){                       // when the potentiometer is unes we want to prevent the initial delay
  for(int i=0;i<sampleNum;++i){
    potBuffer[i]=val;
  }
}




void loop() {
  ArduinoOTA.handle();
  mb.task();
  
  /*
  if(receiveUDP()>0){
    //char*respond=decodeAndBuildRespond();
    decodeAndBuildRespond();
    //sendUDP(respond);
    sendUDP();
  }
  */

  if(millis()-potTimer>15){
      ledHandler();
      potTimer=millis();
      if(millis()-reconnectTimer>reconnect_period){
        reconnectTimer=millis();
        connect2Wifi(200);
      }
      if(millis()-date_check_timer>60000){
        getTime();
        if(strcmp(timeStamp.c_str(),"07:00")==0){
          debugln("restarting");
          ESP.restart();
        }
        date_check_timer=millis();
        
      }
      if(isError){
        if(millis()-errorTimer>curentBlinkingPeriod){
          errorTimer=millis();
          digitalWrite(LED_BUILTIN,ledState);
          ledState=!ledState;
        }
      }
      else{
        digitalWrite(LED_BUILTIN,1);
      }
      
      if(millis()-tempSampleTimer>tempSamplePeriod){
            tempSampleTimer=millis();
            readHumAndTemp();
      }

      if(millis()-serverUpdateTimer>serverUpdatePeriod){
        serverUpdateTimer=millis();
        notifyClients();
      }

      if(millis()-valveHandleTimer>valveHandlePeriod){
        valveHandleTimer=millis();
        if(temperature>hystUpperLimit){
          digitalWrite(VALVE,0);
        }
        else if(temperature<hystLowerLimit){
          digitalWrite(VALVE,1);
        }
      }

      if(currentPotAVG==0 && millis()-ledTurnedOffTimer>45000){
        if(ledSPMode==0){
          ledTurnedOffTimer=millis();
          ledSPMode=1;
        }
      }
    }
}




