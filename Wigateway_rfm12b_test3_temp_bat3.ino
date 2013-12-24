#include <EtherCard.h>
#include <JeeLib.h>
#include <RTClib.h>                 // Real time clock (RTC) - used for software RTC to reset kWh counters at midnight
#include <Wire.h>                   // Part of Arduino libraries - needed for RTClib
RTC_DS1307 RTC; //非常重要，将原来的RTC_Millis RTC;改为RTC_DS1307 RTC;

int tempW;
int tempF;
int battery;
//--------------------------------------------------------------------------------------------
// RFM12B Settings
//--------------------------------------------------------------------------------------------
#define MYNODE 22            // Should be unique on network, node ID 30 reserved for base station
#define freq RF12_433MHZ     // frequency - match to same frequency as RFM12B module (change to 868Mhz or 915Mhz if appropriate)
#define group 210 

typedef struct { 
  int temp; 
  int battery;
} 
PayloadTx;
PayloadTx emontx;

static byte mymac[] = {
  0x00,0x69,0x69,0x2D,0x30,0x00};
byte Ethernet::buffer[700];

void setup () {

  Serial.begin(57600);
  Serial.println("BasicServer Demo");

  if (!ether.begin(sizeof Ethernet::buffer, mymac))
    Serial.println( "Failed to access Ethernet controller");
  else
    Serial.println("Ethernet controller initialized");

  if (!ether.dhcpSetup())
    Serial.println("Failed to get configuration from DHCP");
  else
    Serial.println("DHCP configuration done");

  ether.printIp("IP Address:\t", ether.myip);
  ether.printIp("Netmask:\t", ether.mymask);
  ether.printIp("Gateway:\t", ether.gwip);
  Serial.println();

  //-------RTC 增加部分
  Wire.begin();//
  RTC.begin();// 因为RTC改为DS1307，这里需要增加RTC.begin();

  if (! RTC.isrunning()) {
    Serial.println("RTC is NOT running!");
    RTC.adjust(DateTime(__DATE__, __TIME__));  //ADD RTC TIME,注意，这行必须在IF条件内，否则上电就会重置时间。
  }

  delay(500); 				   //wait for power to settle before firing up the RF
  rf12_initialize(MYNODE, freq,group);    //无线RF初始化
  delay(100);				   //wait for RF to settle befor turning on display



}

void loop() {

  //rfm12b程序部分
  if (rf12_recvDone())  //接收数据开始
  {
    if (rf12_crc == 0 && (rf12_hdr & RF12_HDR_CTL) == 0)  // and no rf errors/检查RF没有错误，如没有错误，继续
    {
      int node_id = (rf12_hdr & 0x1F);

      if (node_id == 18) {
        emontx = *(PayloadTx*) rf12_data;
        Serial.print("received ok");
      }
      /*if (node_id == 15)			//Assuming 15 is the emonBase node ID/ emonBase
       {
       RTC.adjust(DateTime(2012, 1, 1, rf12_data[1], rf12_data[2], rf12_data[3]));
       last_emonbase = millis();
       } 
       */
    }
  }
  //输出时间 
  DateTime now = RTC.now();

  Serial.print(now.year(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.println(now.day(), DEC);
  tempW = emontx.temp/100;
  tempF = emontx.temp%100;

  battery = emontx.battery;   // 得到tempnode的温度。需要除以100
  Serial.println(tempW);
  Serial.println(battery);

  //-----rfm12b部分

  word len = ether.packetReceive();
  word pos = ether.packetLoop(len);

  if(pos) {

    Serial.println("---------------------------------------- NEW PACKET ----------------------------------------");
    Serial.println((char *)Ethernet::buffer + pos);
    Serial.println("--------------------------------------------------------------------------------------------");
    Serial.println();

    BufferFiller bfill = ether.tcpOffset();
    bfill.emit_p(PSTR("HTTP/1.0 200 OK\r\n"
      "Content-Type: text/html\r\nPragma: no-cache\r\n\r\n"
      "<html><head><title>Komoya Labs</title></head>"
      "<body bgcolor=""#99CCFF"">"
      "Komoya Labs"
      "<br />"
      "Temp Outside: "
      "<br />"
      "<b><font size=""7"">"
      "$D.$D &deg;C</font></b>" 
      "<br />"
      "Battery Voltage:"
      "<br />"
      "<b><font size=""7"">"
      " $D mV </font></b> "
      "</body></html>"),tempW,tempF,battery);
    ether.httpServerReply(bfill.position());
  }
}




