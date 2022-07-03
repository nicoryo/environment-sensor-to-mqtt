#include <WiFi.h>
#include <PubSubClient.h>
#include <M5StickCPlus.h>
#include "UNIT_ENV.h"

SHT3X sht30;
QMP6988 qmp6988;

float tmp = 0.0;
float hum = 0.0;
float pressure = 0.0;
long now = millis();
char timestamp[20];
 
// Wi-FiのSSID
char *ssid = "";
// Wi-Fiのパスワード
char *password = "";
// MQTTの接続先のIP
const char *endpoint = "";
// MQTTのポート
const int port = 1883;
// デバイスID
char *deviceID = "M5StickCPlus";  // デバイスIDは機器ごとにユニークにします
// メッセージを知らせるトピック
char *pubTopic = "/e2s/env";
// メッセージを待つトピック
char *subTopic = "/s2e/env";

const char* ntpServer =  "ntp.jst.mfeed.ad.jp";
const long  gmtOffset_sec = 9 * 3600;
const int   daylightOffset_sec = 0;
uint8_t YY, MM, dd, hh, mm, ss;

////////////////////////////////////////////////////////////////////////////////
   
WiFiClient httpsClient;
PubSubClient mqttClient(httpsClient);


   
void setup() {
    Serial.begin(115200);
     
    // ENV SENSOR
    M5.begin(); //Init M5Stack
    Wire.begin(); //Wire init, adding the I2C bus.
    qmp6988.init();
 
    // START
    M5.Axp.ScreenBreath(7);
    M5.Lcd.setRotation(3);
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(10, 10);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setTextSize(2);
    M5.Lcd.printf("START");
     
    // Start WiFi
    Serial.println("Connecting to ");
    Serial.print(ssid);
    WiFi.begin(ssid, password);
   
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
 
    // WiFi Connected
    Serial.println("\nWiFi Connected.");
    M5.Lcd.setCursor(10, 40);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setTextSize(2);
    M5.Lcd.printf("WiFi Connected.");

    //init and get the time
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
      Serial.println("Failed to obtain time");
      return;
    }
    YY = timeinfo.tm_year + 1900; MM = timeinfo.tm_mon+1;
    dd = timeinfo.tm_mday;        hh = timeinfo.tm_hour;
    mm = timeinfo.tm_min;         ss = timeinfo.tm_sec;
     
    mqttClient.setServer(endpoint, port);
   
    connectMQTT();
}
   
void connectMQTT() {
    while (!mqttClient.connected()) {
        if (mqttClient.connect(deviceID)) {
            Serial.println("Connected.");
            int qos = 0;
            mqttClient.subscribe(subTopic, qos);
            Serial.println("Subscribed.");
        } else {
            Serial.print("Failed. Error state=");
            Serial.print(mqttClient.state());
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}
   
long messageSentAt = 0;
int led,red,green,blue;

void getEnvData() {
    pressure = qmp6988.calcPressure();
    if(sht30.get()==0){ //Obtain the data of shT30. 
      tmp = sht30.cTemp;  //Store the temperature obtained from shT30.
      hum = sht30.humidity; //Store the humidity obtained from the SHT30.
    }else{
      tmp=0,hum=0;
    }
}

String buildJson() {
  String json = "{";
    json +=  "\"devices\": \"M5StickCPlus\"";
    json += ",";
    json += "\"payload\":";
    json += "{";
    json += "\"timestamp\":";
    json += timestamp;
    json += ",";
    json += "\"humidity\":";
    json += hum;
    json += ",";
    json += "\"temperature\":";
    json += tmp;
    json += ",";
    json += "\"pressure\":";
    json += pressure;
    json += "}";
    json += "}";

  return json;
}

  
void mqttLoop() {
    if (!mqttClient.connected()) {
        connectMQTT();
    }
    mqttClient.loop();
}

void loop() { 
    // 常にチェックして切断されたら復帰できるように
    mqttLoop();

    struct tm timeinfo;
    //時刻を取得する
      if (!getLocalTime(&timeinfo)) {
      Serial.println("Failed to obtain time");
      return;
    }

    sprintf (timestamp, " %04d/%02d/%02d %02d:%02d:%02d",
          timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
          timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    
    
    // 5秒ごとにメッセージを飛ばす
    long now = millis();
    if (now - messageSentAt > 5000) {
        messageSentAt = now;
        
        getEnvData();
        
        String json = buildJson();
        char jsonStr[200];
        json.toCharArray(jsonStr,200);
  
        boolean pubResult = mqttClient.publish(pubTopic,jsonStr);
  
        if (pubResult)
            Serial.println("successfully publish");
        else
            Serial.println("unsuccessfully publish");

        M5.Lcd.fillRect(0,0,200,200,BLACK); //Fill the screen with black (to clear the screen).
        M5.Lcd.setCursor(0,20);
        M5.Lcd.printf("Temp: %2.1f  \r\nHumi: %2.0f%%  \r\nPressure:%2.0fPa\r\n", tmp, hum, pressure);
        M5.Lcd.printf(timestamp);
        Serial.println(timestamp);

  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
    }
}

// Handles messages arrived on subscribed topic(s)
void callback(char* topic, byte* payload, unsigned int length) {
}
