// BOARD Seeed Wio LTE Cat.1
// GROVE UART <-> Grove - GPS (SKU#113020003)
//
// Platform
//  SeeedJP STM32 Boards ... http://www.seeed.co.jp/package_SeeedJP_index.json
// Libraries
//  Wio LTE for Arduino  ... https://github.com/SeeedJP/WioLTEforArduino
//  MjGrove              ... https://github.com/matsujirushi/MjGrove
//  TinyGPS++            ... https://github.com/mikalhart/TinyGPSPlus

#include <MjGrove.h>
#include <TinyGPS++.h>

////////////////////////////////////////////////////////////////////////////////
// Defines

#define DISPLAY_INTERVAL  (5000)
#define RECEIVE_TIMEOUT   (10000)

#define LED_NONE     0,  0,  0
#define LED_SETUP    0, 10,  0
#define LED_GATHER  10,  0, 10
#define LED_SEND     0,  0, 10
#define LED_STOP    10,  0,  0

#define ERROR()   Error(__FILE__, __LINE__)

////////////////////////////////////////////////////////////////////////////////
// Global variables

WioCellular Wio;

GroveBoard Board;
GroveGPS GPS(&Board.UART);
TinyGPSPlus TinyGPS;

////////////////////////////////////////////////////////////////////////////////
// setup and loop

void setup()
{
  delay(200);
  SerialUSB.begin(115200);

  Wio.Init();
  Wio.PowerSupplyLTE(true);
  Wio.PowerSupplyGrove(true);
  delay(500);
  if (!Wio.TurnOnOrReset()) ERROR();
  if (!Wio.Activate("soracom.io", "sora", "sora")) ERROR();
  
  Board.UART.Enable(9600, 8, HalUART::PARITY_NONE, 1);
  GPS.Init();
  
  GPS.AttachMessageReceived(MessageReceived);
}

void loop()
{
  static long nextDisplayTime = 0;
  
  GPS.DoWork();

  if (millis() >= nextDisplayTime)
  {
    nextDisplayTime = millis() + 5000;
    DisplayInfo();
    if (TinyGPS.location.isValid())
    {
      SendData();
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// Callback functions

void MessageReceived(const char* message)
{
//  SerialUSB.println(message);

  while (*message != '\0') TinyGPS.encode(*message++);
  TinyGPS.encode('\r');
  TinyGPS.encode('\n');
}

////////////////////////////////////////////////////////////////////////////////
// Task functions

void DisplayInfo()
{
  SerialUSB.print("Location: "); 
  if (TinyGPS.location.isValid())
  {
    SerialUSB.print(TinyGPS.location.lat(), 6);
    SerialUSB.print(",");
    SerialUSB.print(TinyGPS.location.lng(), 6);
  }
  else
  {
    SerialUSB.print("INVALID");
  }

  SerialUSB.print("  Date/Time: ");
  if (TinyGPS.date.isValid())
  {
    SerialUSB.print(TinyGPS.date.year());
    SerialUSB.print("/");
    SerialUSB.print(TinyGPS.date.month());
    SerialUSB.print("/");
    SerialUSB.print(TinyGPS.date.day());
  }
  else
  {
    SerialUSB.print("INVALID");
  }
  
  SerialUSB.print(" ");
  if (TinyGPS.time.isValid())
  {
    if (TinyGPS.time.hour() < 10) SerialUSB.print("0");
    SerialUSB.print(TinyGPS.time.hour());
    SerialUSB.print(":");
    if (TinyGPS.time.minute() < 10) SerialUSB.print("0");
    SerialUSB.print(TinyGPS.time.minute());
    SerialUSB.print(":");
    if (TinyGPS.time.second() < 10) SerialUSB.print("0");
    SerialUSB.print(TinyGPS.time.second());
    SerialUSB.print(".");
    if (TinyGPS.time.centisecond() < 10) SerialUSB.print("0");
    SerialUSB.print(TinyGPS.time.centisecond());
  }
  else
  {
    SerialUSB.print("INVALID");
  }

  SerialUSB.println();
}

void SendData()
{
  auto connectId = Wio.SocketOpen("uni.soracom.io", 23080, WIO_UDP);
  if (connectId < 0) ERROR();

  char sendData[100];
  sprintf(sendData, "{\"lat\":%.6lf,\"lng\":%.6lf}", TinyGPS.location.lat(), TinyGPS.location.lng());
  if (!Wio.SocketSend(connectId, (const byte*)sendData, strlen(sendData))) ERROR();
  char recvData[100];
  auto length = Wio.SocketReceive(connectId, recvData, sizeof (recvData), RECEIVE_TIMEOUT);
  if (length < 0) ERROR();
  if (length == 0) ERROR();

  if (!Wio.SocketClose(connectId)) ERROR();
}

////////////////////////////////////////////////////////////////////////////////
// Helper functions

void Error(const char* file, int line)
{
  Wio.LedSetRGB(LED_STOP);
  
  const char* filename = strrchr(file, '\\');
  filename = filename ? filename + 1 : file;
  
  SerialUSB.print("ERROR at ");
  SerialUSB.print(filename);
  SerialUSB.print("(");
  SerialUSB.print(line);
  SerialUSB.print(")");
  SerialUSB.println();

  while (true) {}
}

////////////////////////////////////////////////////////////////////////////////
