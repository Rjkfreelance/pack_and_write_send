#include <WiFi.h>
#include <Wire.h>
#include <PubSubClient.h> 
#include <Adafruit_MCP3008.h>
#include "time.h"
#include <RTClib.h>
#include <Machine.h>
#include <string.h>
#include <stdio.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"

#define LED2 13 //CS2#
#define RST 12

/*--------------Config Variable---------------------*/
const char* wifi_pwd;
const char* wifi_ssid;
const char* mqtt_server;
int mqttPort;
const char* mqttUser;
const char* mqttPassword;
const char* clientId;
int otatimeout; // OTA Timeout limit 30 sec
const char* sendtopic; // Machine send data Topic
const char* gtopic; //OTA Group Topic 
const char* ctopic; //OTA Sub Companny Topic
const char* stopic; //OTA Self Machine Topic
const char* ackota; //OTA Acknowledge use for Machine confirm received OTA
const char* getconf; // //Topic of this machine subscribe (or listen) for receive command from web socket command getcf(get config)
const char* sendconf; // Topic for Machine send config back to(publish to) web server (use web socket)
const char* dbreply;//Topic for check db active  Server Reply OK if Insert data already  ADD BY RJK 
/*--------------Config Variable---------------------*/

String eachline;// String  Object receiving each line in file conf.txt

char* certssl; // SSL Certification for download firmware OTA Load from certi.txt
String  Certs = "";// String Object for concatination certification from file certi.txt


/* String Object array for keep config each line in file conf.txt in SD Card */
String Line[16];

/*----------TIME NTP Server-------------*/
const char* ntpServer = "time.uni.net.th";
const long  gmtOffset_sec = 7 * 3600;
const int   daylightOffset_sec = 0;

String datareceivedmqtt; // receive OK from DBserv
volatile bool dbready = false;
int checksettime = 0;

#define LEDTIME 500 //Send Data time interval for Testsend
#define Qos  1 //Quality of Service
uint64_t chipid;  //Declaration for storage ChipID
#define VER "AB02" // Firmware Version

WiFiClient FMXClient;
PubSubClient client(FMXClient);


RTC_DS3231 RTC;
Machine mac;
Adafruit_MCP3008 adc;

#define LEDTIME 500 //Send Data time interval for Testsend
#define Qos  1 //Quality of Service
#define TALK_DB "DB/talk" //add by rjk
#define DB_READY "DB/run" //add by rjk

/*-----------------------Machine-------------------------*/
#define IO_1 mac.READ_DATASW(sw1)
#define IO_2 mac.READ_DATASW(sw2)
#define IO_3 mac.READ_DATASW(sw3)
#define IO_4 mac.READ_DATASW(sw4)
#define IO_5 mac.READ_DATASW(sw5)
#define IO_6 mac.READ_DATASW(sw6)
#define IO_7 mac.READ_DATASW(sw7)
#define IO_8 mac.READ_DATASW(sw8)
#define writeaddr_eeprom1 32001
#define writeaddr_eeprom2 32002
#define FILE_COUNT_INHISTORYSD 31250 //1GB:1,000,000KB
#define addrsize 128

#define time_limitwifi  5
char DATA_PACKHEADHIS[16];
char DATA_PACKHEAD[21];
char DATA_PACKDATE[6];
char DATA_PACKIO[2];
char Chipid_buf[12];
char filnamechar[12];
char buf_date[12];
char buf_io[4];
char v_fw[4];

unsigned char DATA_PACKPWM1[4];
unsigned char DATA_PACKPWM2[4];
unsigned char DATA_PACKRELAY[1];
unsigned char DATA_PACKPWM3[4];
unsigned char DATA_PACKPWM4[4];
unsigned char DATA_PACKAD1[4];
unsigned char DATA_PACKAD2[4];
unsigned char DATA_PACKAD3[4];
unsigned char DATA_PACKGPS[6];
unsigned char DATA_PACKM1[8];
unsigned char DATA_PACKM2[3];
unsigned char DATA_PACKM3[3];

const char* datasaveram;
const char* datamqtt;
const char* datamqttinsdcard;
const char* filenamesavesd;

unsigned int data_IO;
unsigned int write_addeeprom;
int countfileinsd = 0;
int buf_head = 0;
int bufwrite_eeprom1, bufwrite_eeprom2;
int read_packADD;
int time_outwifi = 0;

int filename = 0;
int checksendmqtt = 0;

//uint64_t chipid;

String sDate;
String filenames;
String datainfilesd;
String Headerhistory = "";
String buffilenamedel;

long time_out = 0;
long time_limit = 100;

void Callback(char *topic, byte *payload, unsigned int length){
  Serial.println(topic);//Print topic received
  Serial.println((char*)payload);//print payload (or message) in topic received
  
   if (strncmp(DB_READY, topic, strlen(DB_READY)) == 0){
       /*-----check talk to database server reply---*/
       dbready = (bool)(char)payload[0];
       Serial.println("DB Active");
       
   }
   
   if (strncmp(dbreply, topic, strlen(dbreply)) == 0) {
     
     Serial.println((char*)payload);
     
    for (int r = 0; r < length; r++) {
       
        datareceivedmqtt += (char)payload[r];
      
    }

    Serial.println();
  }

}

void mqttconnect() {
  /* Loop until reconnected */
  while (!client.connected()) {
    /* connect now */
    if (client.connect(clientId, mqttUser, mqttPassword)) {
     
      Serial.println("Mqtt....connected");
      
      /* subscribe topic */
       
        client.subscribe(dbreply,Qos);
        client.subscribe(DB_READY,Qos);
        
      }else{
        Serial.print("Can't connect MQTT Cloud!!!");
        Serial.print(client.state());
        delay(1000);
      }
    }
   
}
void ChkDBReadram_send(){
 int count = 0;
   if(!client.connected()){
        mqttconnect(); 
      }
      client.publish(TALK_DB,"$a");
         
      Serial.println("Run check DBserver Task");

      if(dbready){
        Serial.println(dbready);
        client.publish(sendtopic,datamqtt);
        Serial.println("Send realtime ok");
        Serial.println("Insert: " + datareceivedmqtt);
      }else{
        count++;
        if(count >= 5) ESP.restart();
        dbready = false;
        Serial.println(dbready);
      }
     dbready = false;
    
}

void taskChkDB_Send( void * pvParameters ){
 
   while(1){
      ChkDBReadram_send();
      dbready = 0; 
      client.loop();//mqtt loop 
      vTaskDelay(500 / portTICK_PERIOD_MS); 
     
    }
}

void adcbegin()
{
  if (!adc.begin(18, 23, 19, 15))
  {
    Serial.print("Initializing MCP3008...Failed");
  }
  Serial.println("initialization MCP3008 done.");
  delay(100);
}
void wifi_setup()
{ 
  
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid,wifi_pwd); //assign wifi ssid , pass

  while (WiFi.status() != WL_CONNECTED) {//Loop is not connect until connected exit loop
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("Connect ok");
}

void sdbegin()
{
  if (!SD.begin()) {
    Serial.println("Card Mount Failed");
    return;
  } else {
    Serial.println("SD Card OK");
  }

}

void assignConfig(fs::FS &fs, const char* path) {
  File file = fs.open(path);
  if (!file) {
    Serial.println("Failed to open directory");
    return;
  }
  
  Serial.print("Reading file: ");
  Serial.println(path);
    int n =0;
    while (file.available()) {
     eachline = file.readStringUntil('\n');    
     int posi = eachline.indexOf(':');
     String val = eachline.substring(posi+1);
      //Serial.println(val);
      Line[n] = val;
      Line[n].trim();
      n++;
    }
   wifi_ssid = (const char*)Line[0].c_str();
   
   wifi_pwd = (const char*)Line[1].c_str();
  
   mqtt_server = (const char*)Line[2].c_str();
 
   mqttPort = Line[3].toInt();
   
   //Serial.println(mqtt_server);//debug ok
   //Serial.println(mqttPort);//debug ok

   mqttUser = (const char*)Line[4].c_str();

   mqttPassword = (const char*)Line[5].c_str();
   
   clientId = (const char*)Line[6].c_str();
 
   otatimeout = Line[7].toInt();
   sendtopic = (const char*)Line[8].c_str();
   gtopic = (const char*)Line[9].c_str();
   ctopic = (const char*)Line[10].c_str();
   stopic = (const char*)Line[11].c_str();
   ackota = (const char*)Line[12].c_str();
   getconf = (const char*)Line[13].c_str();
   sendconf = (const char*)Line[14].c_str();
   dbreply =  (const char*)Line[15].c_str(); // add by rjk
}

String DateTimeNOW(){
   DateTime now = RTC.now();
   String DMY = String(now.day())+"/"+String(now.month())+"/"+String(now.year())+" "+String(now.hour())+":"+String(now.minute())+":"+String(now.second());
   return DMY;
}

void data_time()
{
  DateTime now = RTC.now();
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  //  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  //    Serial.printf("NTP TIME : %02d/%02d/%04d ",timeinfo.tm_mday,timeinfo.tm_mon + 1,timeinfo.tm_year + 1900);
  //    Serial.printf("%02d:%02d:%02d \r\n",timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec);
  if (checksettime == 0 ) {
    RTC.adjust(DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec));
    checksettime = 1;
  }
  else
  {
    if (timeinfo.tm_wday == 0 && timeinfo.tm_hour == 0 && timeinfo.tm_min == 0 && timeinfo.tm_sec <= 60)
    {
      RTC.adjust(DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec));
      Serial.println("Update Time Success");
    }
  }
  Serial.printf("%02d", now.day());
  Serial.print('/');
  Serial.printf("%02d", now.month());
  Serial.print('/');
  Serial.printf("%02d", now.year());
  Serial.print(' ');
  Serial.printf("%02d", now.hour());
  Serial.print(':');
  Serial.printf("%02d", now.minute());
  Serial.print(':');
  Serial.printf("%02d", now.second());
  Serial.println();

}
void packdata_HEADSDCARD() {

  Headerhistory = "";
  DATA_PACKHEADHIS[0] = 0x23;
  DATA_PACKHEADHIS[1] = 0x52; //#Serial
  DATA_PACKHEADHIS[2] = 0x3B;
  DATA_PACKHEADHIS[3] = DATA_PACKHEAD[3];
  DATA_PACKHEADHIS[4] = DATA_PACKHEAD[4];
  DATA_PACKHEADHIS[5] = DATA_PACKHEAD[5];
  DATA_PACKHEADHIS[6] = DATA_PACKHEAD[6];
  DATA_PACKHEADHIS[7] = DATA_PACKHEAD[7];
  DATA_PACKHEADHIS[8] = DATA_PACKHEAD[8];
  DATA_PACKHEADHIS[9] = DATA_PACKHEAD[9];
  DATA_PACKHEADHIS[10] = DATA_PACKHEAD[10];
  DATA_PACKHEADHIS[11] = DATA_PACKHEAD[11];
  DATA_PACKHEADHIS[12] = DATA_PACKHEAD[12];
  DATA_PACKHEADHIS[13] = DATA_PACKHEAD[13];
  DATA_PACKHEADHIS[14] = DATA_PACKHEAD[14];
  DATA_PACKHEADHIS[15] = 0x3B;
  for (int i = 0; i < sizeof(DATA_PACKHEADHIS); i++) {
    Headerhistory += DATA_PACKHEADHIS[i];
    delay(10);
  }
  Serial.print("packdata_HEADSDCARD : ");
  Serial.println(Headerhistory);
}

void packdata_HEAD()
{

  DATA_PACKHEAD[0] = 0x23;
  DATA_PACKHEAD[1] = 0x4D; //#Serial
  DATA_PACKHEAD[2] = 0x3B;
  DATA_PACKHEAD[3] = Chipid_buf[0];
  DATA_PACKHEAD[4] = Chipid_buf[1];
  DATA_PACKHEAD[5] = Chipid_buf[2];
  DATA_PACKHEAD[6] = Chipid_buf[3];
  DATA_PACKHEAD[7] = Chipid_buf[4];
  DATA_PACKHEAD[8] = Chipid_buf[5];
  DATA_PACKHEAD[9] = Chipid_buf[6];
  DATA_PACKHEAD[10] = Chipid_buf[7];
  DATA_PACKHEAD[11] = Chipid_buf[8];
  DATA_PACKHEAD[12] = Chipid_buf[9];
  DATA_PACKHEAD[13] = Chipid_buf[10];
  DATA_PACKHEAD[14] = Chipid_buf[11];
  DATA_PACKHEAD[15] = 0x3B;
  DATA_PACKHEAD[16] = v_fw[0];
  DATA_PACKHEAD[17] = v_fw[1];
  DATA_PACKHEAD[18] = v_fw[2];
  DATA_PACKHEAD[19] = v_fw[3];
  DATA_PACKHEAD[20] = 0x3B;
  if (buf_head == 0) {
    for (int i = 0; i < sizeof(DATA_PACKHEAD); i++)
    {
      mac.packet_header += DATA_PACKHEAD[i];
    }
    buf_head = 1;
    delay(10);
  }


}

void packdata_DATE()
{
  sDate = "";
  DateTime now = RTC.now();
  DATA_PACKDATE[0] = now.day();
  DATA_PACKDATE[1] = now.month();
  DATA_PACKDATE[2] = now.year() - 2000;
  DATA_PACKDATE[3] = now.hour();
  DATA_PACKDATE[4] = now.minute();
  DATA_PACKDATE[5] = now.second();
  
  //sprintf(buf_date, "%02u%02u%02u%02u%02u%02u",now.day(),now.month(),now.year()-2000,now.hour(),now.minute(),now.second());
  
  sprintf(buf_date, "%02u%02u%02u%02u%02u%02u", now.year() - 2000, now.month(), now.day(), now.hour(), now.minute(), now.second());
  for (int i = 0; i < sizeof(buf_date); i++)
  {
    sDate += buf_date[i];
  }
  filenames = sDate;
  sDate += ";";
  delay(50);
}

void packdata_IO()
{
  data_IO = (IO_8 << 7) | (IO_7 << 6) | (IO_6 << 5) | (IO_5 << 4) | (IO_4 << 3) | (IO_3 << 2) | (IO_2 << 1) | (IO_1 << 0);
  DATA_PACKIO[0] = 00;
  DATA_PACKIO[1] = data_IO;
  sprintf(buf_io, "%02X%02X", DATA_PACKIO[0], DATA_PACKIO[1]);
}

void packdata_PWM()
{
  DATA_PACKPWM1[0] = 11;
  DATA_PACKPWM1[1] = 11;
  DATA_PACKPWM1[2] = 11;
  DATA_PACKPWM1[3] = 11;

  DATA_PACKPWM2[0] = 11;
  DATA_PACKPWM2[1] = 11;
  DATA_PACKPWM2[2] = 11;
  DATA_PACKPWM2[3] = 11;
}

void packdata_RELAY()
{
  DATA_PACKRELAY[0] = 11;
}

void packdata_PWM2()
{
  DATA_PACKPWM3[0] = 11;
  DATA_PACKPWM3[1] = 11;
  DATA_PACKPWM3[2] = 11;
  DATA_PACKPWM3[3] = 11;

  DATA_PACKPWM4[0] = 11;
  DATA_PACKPWM4[1] = 11;
  DATA_PACKPWM4[2] = 11;
  DATA_PACKPWM4[3] = 11;
}

void packdata_AD()
{
  DATA_PACKAD1[0] = 11;
  DATA_PACKAD1[1] = 11;
  DATA_PACKAD1[2] = 11;
  DATA_PACKAD1[3] = 11;

  DATA_PACKAD2[0] = 11;
  DATA_PACKAD2[1] = 11;
  DATA_PACKAD2[2] = 11;
  DATA_PACKAD2[3] = 11;

  DATA_PACKAD3[0] = 11;
  DATA_PACKAD3[1] = 11;
  DATA_PACKAD3[2] = 11;
  DATA_PACKAD3[3] = 11;
}

void packdata_GPS()
{
  DATA_PACKGPS[0] = 11;
  DATA_PACKGPS[1] = 11;
  DATA_PACKGPS[2] = 11;
  DATA_PACKGPS[3] = 11;
  DATA_PACKGPS[4] = 11;
  DATA_PACKGPS[5] = 11;
  DATA_PACKGPS[6] = 11;
}



void packdata_M1()
{
  DATA_PACKM1[0] = 11;
  DATA_PACKM1[1] = 11;
  DATA_PACKM1[2] = 11;
  DATA_PACKM1[3] = 11;
  DATA_PACKM1[4] = 11;
  DATA_PACKM1[5] = 11;
  DATA_PACKM1[6] = 11;
  DATA_PACKM1[7] = 11;
}

void packdata_M2()
{
  DATA_PACKM2[0] = 11;
  DATA_PACKM2[1] = 11;
  DATA_PACKM2[2] = 11;
}

void packdata_M3()
{
  DATA_PACKM3[0] = 11;
  DATA_PACKM3[1] = 11;
  DATA_PACKM3[2] = 11;
}
void ChipID(){//Show Chip ID
  chipid=ESP.getEfuseMac();//The chip ID is  MAC address(length: 6 bytes).
  Serial.printf("Machine Board Chip ID = %04X",(uint16_t)(chipid>>32));//print High 2 bytes
  Serial.printf("%08X\n",(uint32_t)chipid);//print Low 4bytes.
}

void writeToram(){
  
  packdata_HEAD();
  packdata_DATE();
  packdata_IO();
  packdata_PWM();
  packdata_RELAY();
  packdata_PWM2();
  packdata_AD();
  packdata_GPS();
  packdata_M1();
  packdata_M2();
  packdata_M3();
  delay(100);
  bufwrite_eeprom1 = mac.readAddress(writeaddr_eeprom1);
  bufwrite_eeprom2 = mac.readAddress(writeaddr_eeprom2);
  write_addeeprom = (bufwrite_eeprom2 << 8) + bufwrite_eeprom1;
  String sText;

  sText += sDate;
  for (int i = 0; i < sizeof(buf_io); i++)
  {
    sText += buf_io[i];
  }
  sText += ";" ;
  for (int i = 0; i < sizeof(DATA_PACKPWM1); i++)
  {
    sText += DATA_PACKPWM1[i];
  }
  sText += ";" ;
  for (int i = 0; i < sizeof(DATA_PACKPWM2); i++)
  {
    sText += DATA_PACKPWM2[i];
  }
  sText += ";" ;
  for (int i = 0; i < sizeof(DATA_PACKRELAY); i++)
  {
    sText += DATA_PACKRELAY[i];
  }
  sText += ";" ;
  for (int i = 0; i < sizeof(DATA_PACKPWM3); i++)
  {
    sText += DATA_PACKPWM3[i];
  }
  sText += ";" ;
  for (int i = 0; i < sizeof(DATA_PACKPWM4); i++)
  {
    sText += DATA_PACKPWM4[i];
  }
  sText += ";" ;
  for (int i = 0; i < sizeof(DATA_PACKAD1); i++)
  {
    sText += DATA_PACKAD1[i];
  }
  sText += ";" ;
  for (int i = 0; i < sizeof(DATA_PACKAD2); i++)
  {
    sText += DATA_PACKAD2[i];
  }
  sText += ";" ;
  for (int i = 0; i < sizeof(DATA_PACKAD3); i++)
  {
    sText += DATA_PACKAD3[i];
  }
  sText += ";" ;
  for (int i = 0; i < sizeof(DATA_PACKGPS); i++)
  {
    sText += DATA_PACKGPS[i];
  }
  sText += ";" ;
  for (int i = 0; i < sizeof(DATA_PACKM1); i++)
  {
    sText += DATA_PACKM1[i];
  }
  sText += ";" ;
  for (int i = 0; i < sizeof(DATA_PACKM2); i++)
  {
    sText += DATA_PACKM2[i];
  }
  sText += ";" ;
  for (int i = 0; i < sizeof(DATA_PACKM3); i++)
  {
    sText += DATA_PACKM3[i];
  }
  sText += ";" ;

  datasaveram = sText.c_str();
  delay(50);
  Serial.print("Data For PackSendMQTT : ");
  Serial.println(sText);
  Serial.print("EEPROM ADDR : ");
  Serial.println(write_addeeprom);
  delay(50);
  for (int i = 0; i < addrsize; i++)
  {
    mac.writeAddress(write_addeeprom, datasaveram[i]);
    write_addeeprom++;
  }

  mac.writeAddress(writeaddr_eeprom1, write_addeeprom & 0xFF); //ระบุ ADDRESS
  mac.writeAddress(writeaddr_eeprom2, (write_addeeprom >> 8) & 0xFF);
  readRam(sText, write_addeeprom);
}

void readRam(String sdatamqtt, int write_addr){
  int buf_lasteeporm = write_addr;
    if (write_addeeprom > 0)
      {
        String datahistory;
        String datamakemqtt;
        mac.Scan_data_sstv(write_addr);
        datamakemqtt = mac.make_send_string(sdatamqtt);
        datamqtt = datamakemqtt.c_str();
        Serial.print("write_addr : ");
          Serial.println(write_addr);
          Serial.println(datareceivedmqtt);
           mac.Check_senddata_fram(write_addr);
       }
}


void setup() {
 pinMode(LED2,OUTPUT);
  digitalWrite(LED2,HIGH);//HIGH Turn off LOW on
  Serial.begin(115200);
  v_fw[0] = 0x30;
  v_fw[1] = 0x31;
  v_fw[2] = 0x31;
  v_fw[3] = 0x30;
  sdbegin();
   Serial.println(F("Connected SD Card ok."));
    Serial.println(F("Load config from SD card file:conf.txt"));
  assignConfig(SD,"/conf.txt");
  ChipID();//Show Chip ID (IMEI)
  wifi_setup();
  delay(100);
  Wire.begin();
  delay(100);
  mac.begin();
  delay(100);  
  RTC.begin();
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  delay(100);
  Serial.println("NOW: "+DateTimeNOW());
  client.setServer(mqtt_server,mqttPort); 
  client.setCallback(Callback);
  mqttconnect();
  TaskHandle_t CheckDBserver_send;
    xTaskCreatePinnedToCore(
             taskChkDB_Send, 
             "CheckDBserver_send",   
             5000,     
             NULL,      
             1,        
             &CheckDBserver_send,    
             0);     
}

void loop() {
  chipid = ESP.getEfuseMac();
  sprintf(Chipid_buf, "%04X%08X", (uint16_t)(chipid >> 32), (uint32_t)(chipid));
  delay(50);
  data_time();
  writeToram();
  delay(500);
}
