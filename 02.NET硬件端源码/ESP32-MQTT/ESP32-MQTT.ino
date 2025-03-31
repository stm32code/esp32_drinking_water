#include <WiFi.h>
#include <PubSubClient.h>
#include <Ticker.h>
//OLED
#include <U8g2lib.h>
#include <Wire.h>
//JSON
#include <ArduinoJson.h>
//二维码所需库
#include "qrcode.h"
// 温度库
#include <OneWire.h>
#include <DallasTemperature.h>

// 设置wifi接入信息(请根据您的WiFi信息进行修改)
const char* ssid = "NET";
const char* password = "12345678";
const char* mqttServer = "iot-06z00axdhgfk24n.mqtt.iothub.aliyuncs.com";
// 如以上MQTT服务器无法正常连接，请前往以下页面寻找解决方案
// http://www.taichi-maker.com/public-mqtt-broker/

Ticker ticker;
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
int APP;
int page = 0, page_copy = 1;  // 界面 
// ****************************************************

//遗嘱相关信息
const char* willMsg = "esp8266 offline";  // 遗嘱主题信息
const int willQos = 0;                    // 遗嘱QoS
const int willRetain = false;             // 遗嘱保留
const int subQoS = 1;                     // 客户端订阅主题时使用的QoS级别（截止2020-10-07，仅支持QoS = 1，不支持QoS = 2）
const bool cleanSession = true;           // 清除会话（如QoS>0必须要设为false）
// LED 配置
bool ledStatus = LOW;
#define LED_1 12

// 水泵 配置
#define Pump 14
#define Water 4
int pump_state = 0;
int state = 0;
int water_ml = 0;
bool water_state = false;           // 清除会话（如QoS>0必须要设为false）

// 按键 配置
#define Key1 19
#define Key2 18
int key_time = 0;
// 显示屏 配置
#define SCL 22
#define SDA 21
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, SCL, SDA, /*reset=*/U8X8_PIN_NONE);
char str[50];  //拼接字符使用
//  TDS 配置
#define TdsSensorPin 32
#define VREF 5.0           // analog reference voltage(Volt) of the ADC
#define SCOUNT 8           // sum of sample point
int analogBuffer[SCOUNT];  // store the analog value in the array, read from ADC
int analogBufferTemp[SCOUNT];
int analogBufferIndex = 0, copyIndex = 0, tdsValue = 0;
float averageVoltage = 0, temperature = 25;

// DS18B20数据引脚定义
#define DS18B20 15
OneWire oneWire(DS18B20);
DallasTemperature sensors(&oneWire);
DeviceAddress insideThermometer;  //实例化对象
float tempC;

void setup() {
  Serial.begin(9600);
  //OELD
  u8g2.begin();
  u8g2.setFont(u8g2_font_t0_16_tf);  //设定字体
  //设置ESP8266工作模式为无线终端模式
  WiFi.mode(WIFI_STA);
  // 按键
  pinMode(Key1, INPUT_PULLUP);
  pinMode(Key2, INPUT_PULLUP);
  // 水流监测
  pinMode(Water, INPUT_PULLUP);
  // LED
  pinMode(LED_1, OUTPUT);
  pinMode(Pump, OUTPUT);  // 水泵引脚为输出模式
  // 开始DS18B20的通讯
  sensors.begin();
  if (!sensors.getAddress(insideThermometer, 0)) {  //检测设备是否上线
    Serial.println("error");                        //串口打印对应的值
  } else {
    Serial.println("succeed");  //串口打印对应的值
  }
  // 连接WiFi
  connectWifi();
  // 设置MQTT服务器和端口号
  mqttClient.setServer(mqttServer, 1883);
  mqttClient.setCallback(receiveCallback);
  // 连接MQTT服务器
  connectMQTTserver();

  digitalWrite(LED_1, 0);  // 启动后关闭板上LED
  digitalWrite(Pump, 0);   // 启动后关闭板上LED
}

void loop() {
  // 如果开发板未能成功连接服务器，则尝试连接服务器
  if (!mqttClient.connected()) {
    connectMQTTserver();
  } else {
    // 40ms
    static unsigned long analogSampleTimepoint = millis();
    if (millis() - analogSampleTimepoint > 40U)  //every 40 milliseconds,read the analog value from the ADC
    {
      analogSampleTimepoint = millis();
      analogBuffer[analogBufferIndex] = analogRead(TdsSensorPin);  //read the analog value and store into the buffer
      analogBufferIndex++;
      if (analogBufferIndex == SCOUNT)
        analogBufferIndex = 0;
    }
    static unsigned long printTimepoint = millis();
    // 1S
    if (millis() - printTimepoint > 1000U) {
      if(pump_state == 0 ){
      // TDS
      printTimepoint = millis();
      for (copyIndex = 0; copyIndex < SCOUNT; copyIndex++)
        analogBufferTemp[copyIndex] = analogBuffer[copyIndex];
        averageVoltage = getMedianNum(analogBufferTemp, SCOUNT) * (float)VREF / 1024.0;                                                                                                   // read the analog value more stable by the median filtering algorithm, and convert to voltage value
        float compensationCoefficient = 1.0 + 0.02 * (tempC - 25.0);                                                                                                                //temperature compensation formula: fFinalResult(25^C) = fFinalResult(current)/(1.0+0.02*(fTP-25.0));
        float compensationVolatge = averageVoltage / compensationCoefficient;                                                                                                             //temperature compensation
        tdsValue = (133.42 * compensationVolatge * compensationVolatge * compensationVolatge - 255.86 * compensationVolatge * compensationVolatge + 857.39 * compensationVolatge) * 0.5;  //convert voltage value to tds value
      }
                                                                                                                                                                            // 实现LED闪烁
      if (ledStatus == LOW) {
        ledStatus = HIGH;
        digitalWrite(LED_1, ledStatus);  // 则点亮LED。
      } else {
        ledStatus = LOW;
        digitalWrite(LED_1, ledStatus);  // 则点亮LED。
      }

      if (page == 1) {
        u8g2.clearBuffer();
        sprintf(str, "Water: %d  mL  ", water_ml/2);
        u8g2.drawStr(0, 16, str);
        sprintf(str, "Quality: %d  ", tdsValue);
        u8g2.drawStr(0, 32, str);
        sprintf(str, "Temp: %.2f C", tempC);
        u8g2.drawStr(0, 48, str);
        u8g2.sendBuffer();  //显示
        page_copy = page;
      }
      if (page == 0 && page != page_copy) {
        u8g2.clearDisplay();  // 清屏
        QR_Code();
        page_copy = page;
      }
      Serial.print("TDS Value:");
      Serial.print(tdsValue, 0);
      Serial.println("ppm");
    }
    // 3S
    static unsigned long time3s = millis();
    if (millis() - time3s > 3000U) {
      if(pump_state == 0 ){
        printTemperature(insideThermometer);  //获取温度数据
      }
      if(APP==0 && state == 1){
        APP = 1;
      }
      time3s = millis();
    }
    // 则点亮水泵
    digitalWrite(Pump, pump_state);  
    // 按钮检测
    Key_check(); 
    // 水流
    water_quantity();
    if(APP){
      //回传数据
      pubMQTTmsg();
      APP=0;
    }
  }
  mqttClient.loop();  // 处理信息以及心跳
}
int getMedianNum(int bArray[], int iFilterLen) {
  int bTab[iFilterLen];
  for (byte i = 0; i < iFilterLen; i++)
    bTab[i] = bArray[i];
  int i, j, bTemp;
  for (j = 0; j < iFilterLen - 1; j++) {
    for (i = 0; i < iFilterLen - j - 1; i++) {
      if (bTab[i] > bTab[i + 1]) {
        bTemp = bTab[i];
        bTab[i] = bTab[i + 1];
        bTab[i + 1] = bTemp;
      }
    }
  }
  if ((iFilterLen & 1) > 0)
    bTemp = bTab[(iFilterLen - 1) / 2];
  else
    bTemp = (bTab[iFilterLen / 2] + bTab[iFilterLen / 2 - 1]) / 2;
  return bTemp;
}
// 连接MQTT服务器并订阅信息
void connectMQTTserver() {
  // 根据ESP8266的MAC地址生成客户端ID（避免与其它ESP8266的客户端ID重名）
  /* 连接MQTT服务器
  boolean connect(const char* id, const char* user, 
                  const char* pass, const char* willTopic, 
                  uint8_t willQos, boolean willRetain, 
                  const char* willMessage, boolean cleanSession); 
  若让设备在离线时仍然能够让qos1工作，则connect时的cleanSession需要设置为false                
                  */
  if (mqttClient.connect(clientId, mqttUserName,
                         mqttPassword, willTopic,
                         willQos, willRetain, willMsg, cleanSession)) {
    Serial.print("MQTT Server Connected. ClientId: ");
    Serial.println(clientId);
    Serial.print("MQTT Server: ");
    Serial.println(mqttServer);

    subscribeTopic();  // 订阅指定主题

  } else {
    Serial.print("MQTT Server Connect Failed. Client State:");
    Serial.println(mqttClient.state());
    delay(5000);
  }
}

// 收到信息后的回调函数
void receiveCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message Received [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  //解析数据
  massage_parse_json((char*)payload);
}

// 订阅指定主题
void subscribeTopic() {
  // 通过串口监视器输出是否成功订阅主题以及订阅的主题名称
  // 请注意subscribe函数第二个参数数字为QoS级别。这里为QoS = 1
  if (mqttClient.subscribe(subTopic, subQoS)) {
    Serial.println(subTopic);
  } else {
    Serial.print("Subscribe Fail...");
    connectMQTTserver();  // 则尝试连接服务器
  }
}

// 发布信息
void pubMQTTmsg() {
  char pubMessage[254];
  // 数据流
  MQTT_FillBuf(pubMessage);

  // 实现ESP8266向主题发布信息
  if (mqttClient.publish(pubTopic, pubMessage)) {
    //Serial.println("Publish Topic:");Serial.println(pubTopic);
    // Serial.println("Publish message:");
    // Serial.println(pubMessage);
  } else {
    subscribeTopic();  // 则尝试连接服务器
    Serial.println("Message Publish Failed.");
  }
}
 

// 数据封装
unsigned char MQTT_FillBuf(char* buf) {

  char text[256];
  memset(text, 0, sizeof(text));

  strcpy(buf, "{");

  
  memset(text, 0, sizeof(text));
  sprintf(text, "\"water_t\":\"%d\"", tdsValue);
  strcat(buf, text);


  memset(text, 0, sizeof(text));
  sprintf(text, "}");
  strcat(buf, text);

  return strlen(buf);
}

// 解析json数据
void massage_parse_json(char* message) {
  // 声明一个 JSON 文档对象
  StaticJsonDocument<200> doc;

  // 解析 JSON 数据
  DeserializationError error = deserializeJson(doc, (const char*)message);

  // 检查解析是否成功
  if (error) {
    Serial.print("Failed to parse JSON: ");
    Serial.println(error.c_str());
    return;
  }
  // 从解析后的 JSON 文档中获取键值对
  int cmd = doc["cmd"];
  // JsonObject data = doc["data"];
  switch (cmd) {
    case 1:
      // 实现水泵开关
      pump_state = doc["pump"];
      if(APP==0){
        APP = 1;
      }
      break;
    case 2:
      state = doc["state"];
      page = state;
      water_ml = 0;
      if (page == 1) {
        u8g2.clearDisplay();  // 清屏
        //u8g2_font_t0_16_tf
        u8g2.setFont(u8g2_font_t0_16_tf);  //设定字体
        u8g2.setColorIndex(1);
      }
      if(APP==0){
        APP = 1;
      }
      break;
  }
}
/****************************************ds18b20 part****************************************/
/*获取温度数据*/
void printTemperature(DeviceAddress deviceAddress) {
 
}
// 显示二维码
void QR_Code() { 
    }

  } while (u8g2.nextPage());
}
// 按钮检测
void Key_check() {
  //将开关状态数值读取到变量中
  int K1 = digitalRead(Key1);
  int K2 = digitalRead(Key2);
  if (K1 == LOW) {  //按钮按下
    // 时间计算
    if (key_time < 1000) {
      key_time++;
    }
    if (key_time == 5) {
      if (state == 1) {
        state = 2;
        pump_state = 0;
        water_ml = 0;
        if(APP==0){
           APP = 1;
        }
      }
    }
  } else if (K2 == LOW) {  //按钮按下
    // 时间计算
    if (key_time < 1000) {
      key_time++;
    }
    if (key_time == 5) {
      if (pump_state == 0 && state == 1) {
        pump_state = 1;
      } else {
        pump_state = 0;
      }
      if(APP==0){
        APP = 1;
      }
    }
  } else {
    // 清空一下计数
    if (key_time != 0) {
      key_time = 0;
    }
  }
}
// 流量
void water_quantity(){
  int WaterKey = digitalRead(Water); 
  if (WaterKey == HIGH && water_state == true) {  //触发
    water_ml++;
    water_state= false;
  }else if(WaterKey == LOW && water_state == false){
    water_state= true;
  }
}
