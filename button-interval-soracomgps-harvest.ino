#include <WioLTEforArduino.h>
#include <stdio.h>
#define BUTTON_PIN  (WIOLTE_D38)

WioLTE Wio;

volatile bool StateChanged = false;
volatile bool State = false;

// ----------------------
// 初期値一覧
// ----------------------

int gps_interval_time_sec = 10; // GPS取得に諦める時間(秒)
int regular_action_interval_time_min = 5; // 定期的にGPS取得する間隔(分)

// ----------------------

int gps_interval_time_msec = gps_interval_time_sec * 1000 ;
int regular_action_interval_time_msec = regular_action_interval_time_min * 1000 * 60 ;

float latitude_before = 0;
float longitude_before = 0;

int start_time;

void change_state()
{
  State = !State;
  StateChanged = true;
  delay(150);
}

void send_to_soracom()
{

  // -------------------------
  // GET GPS DATA
  // -------------------------
  const char* gps_data;
  String str_gps_data = "";
  
  char* tmp = "";
  char* gps_quality = "";

  char gps_data_list[100];
  
  String latitude_str = "";
  String longitude_str = "";
  String send_gps_data = "" ;
  char send_gps_char[80];
  
  float map_float;
  int map_int;
  float map_few;
  float map_difference = 0;
  int map_difference_check = 0;

  
  // 取得開始の時間を取得
  int gps_start_time = millis();
  int gps_now_time = millis();

  // GPSデータで取得できまでLOOP
  while(1)
  {
    gps_data = GpsRead();
    
    if (gps_data != NULL && strncmp(gps_data, "$GPGGA,", 7) == 0) {

      // 取得したデータをカンマ区切りで7つ目データを取得する。値が1なら次の処理へ
      // 位置特定品質。0 = 位置特定できない、1 = SPS（標準測位サービス）モード、2 = differenctial GPS（干渉測位方式）モード
            
      str_gps_data = gps_data ;
      str_gps_data.toCharArray(gps_data_list,100);
    
      tmp = strtok(gps_data_list,",");
      tmp = strtok(NULL,",");
      latitude_str = strtok(NULL,",");
      tmp = strtok(NULL,",");
      longitude_str = strtok(NULL,",");
      tmp = strtok(NULL,",");
      gps_quality = strtok(NULL,",");
      
      if (strncmp(gps_quality,"1",1) == 0) {
        // 緯度経度を計算する
        map_float = latitude_str.toFloat();
        map_float = map_float / 100;
        map_int = map_float;
        map_few = map_float - map_int ;
        map_float = map_int + ((map_few / 60) * 100);
        // 前回との差を確認する
        map_difference = map_float - latitude_before;
        if ((map_difference > 0.001)||(map_difference < -0.001)) {
          map_difference_check = 1;
        }
        latitude_before = map_float ;   // 次回との比較用に今回の値と保存
        latitude_str = String(map_float, 6);

        map_float = longitude_str.toFloat();
        map_float = longitude_str.toFloat();
        map_float = map_float / 100;
        map_int = map_float;
        map_few = map_float - map_int ;
        map_float = map_int + ((map_few / 60) * 100);
        // 前回との差を確認する
        map_difference = map_float - longitude_before;
        if ((map_difference > 0.001)||(map_difference < -0.001)) {
          map_difference_check = 1;
        }
        longitude_before = map_float ;   // 次回との比較用に今回の値と保存
        longitude_str = String(map_float, 6);

        send_gps_data = "{\"lat\": ";
        send_gps_data.concat(latitude_str);
        send_gps_data.concat(", \"lng\": ");
        send_gps_data.concat(longitude_str);
        send_gps_data.concat("}");

        // string を char に変換
        send_gps_data.toCharArray(send_gps_char, 80) ;

        SerialUSB.println("GPS-DATA GET");
        //SerialUSB.println(gps_data);
        
        break;
      }
      
    }
    // GPS取得開始からある程度時間がたったら諦める
    gps_now_time = millis();
    int gps_progress_time = gps_now_time - gps_start_time;
    if (gps_progress_time > gps_interval_time_msec) {
      SerialUSB.println("GPS-GET TIMEOUT");
      break;
    }
    
  }


  // -------------------------
  // SORACOOM SEND GPS DATA
  // -------------------------


  if ((strncmp(gps_quality,"1",1) == 0)&&(map_difference_check == 1)) {
    Wio.PowerSupplyLTE(true);
    
    delay(1000);
    if (!Wio.TurnOnOrReset()) { 
      SerialUSB.println("TurnOnOrReset Error"); 
      setup();
      return;
    } 
    if (!Wio.Activate("soracom.io", "sora", "sora")) { 
      SerialUSB.println("Activate Error"); 
      setup();
      return; 
    }
    int connectId;
    
    //connectId = Wio.SocketOpen("beam.soracom.io", 23080, WIOLTE_UDP);
    connectId = Wio.SocketOpen("harvest.soracom.io", 8514, WIOLTE_UDP);
    
    if (connectId < 0) {
      SerialUSB.println("SocketOpen Error");
      setup();
      return;
    }
   
    if (!Wio.SocketSend(connectId, send_gps_char)) {
      SerialUSB.println("SocketSend Error"); 
      setup();
      return; 
    }
 
    SerialUSB.println("SocketSend");
    char res[128];
    int len;
    len = Wio.SocketReceive(connectId, res, sizeof (res), 3000);
    
    if (len < 0) {
      SerialUSB.println("Error len < 0"); 
      setup();
      return; 
    }
    if (len == 0) { 
      SerialUSB.println("Error len == 0"); 
      setup();
      return; 
    }
    if (!Wio.SocketClose(connectId)) {
      SerialUSB.println("SocketClose Error");
      setup();
      return;
    }
    if (!Wio.Deactivate()) {
      SerialUSB.println("Deactivate Error");
      setup();
      return; 
    }
  
    Wio.PowerSupplyLTE(false);
    gps_quality = "" ;
  }
  
}

void setup()
{
  delay(200);

  SerialUSB.println("");
  SerialUSB.println("--- START ---------------------------------------------------");
  
  SerialUSB.println("### I/O Initialize.");
  GpsBegin(&Serial);
  Wio.Init();

  SerialUSB.println("### Power supply ON.");
  Wio.PowerSupplyGrove(true);
  delay(500);

  pinMode(BUTTON_PIN, INPUT);
  attachInterrupt(BUTTON_PIN, change_state, CHANGE);


  start_time = millis();
  
  SerialUSB.println("### Setup completed.");
  
  
}



void loop()
{

  // ボタンでのアクション
  if (StateChanged) {
    if (State) {
      send_to_soracom();
    }
    StateChanged = false;
  }

  // 定期実行のアクション

  int now_time = millis();
  
  int gps_progress_time = now_time - start_time;
  if (gps_progress_time > regular_action_interval_time_msec) {
    start_time = millis();
    send_to_soracom();
    delay(10000);
  }
  
}

////////////////////////////////////////////////////////////////////////////////////////
//

#define GPS_OVERFLOW_STRING "OVERFLOW"

HardwareSerial* GpsSerial;
char GpsData[100];
char GpsDataLength;
void GpsBegin(HardwareSerial* serial)
{
  GpsSerial = serial;
  GpsSerial->begin(9600);
  GpsDataLength = 0;
}

const char* GpsRead()
{
  while (GpsSerial->available()) {
    char data = GpsSerial->read();
    if (data == '\r') continue;
    if (data == '\n') {
      GpsData[GpsDataLength] = '\0';
      GpsDataLength = 0;
      return GpsData;
    }
    
    if (GpsDataLength > sizeof (GpsData) - 1) { // Overflow
      GpsDataLength = 0;
      return GPS_OVERFLOW_STRING;
    }
    GpsData[GpsDataLength++] = data;
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////////////
