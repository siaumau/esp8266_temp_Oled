#include "DHT.h"
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <U8g2lib.h>
#include <Wire.h>

// DHT 設定
#define DHTPIN 4 
#define DHTTYPE DHT11   // 如果使用 DHT22，請取消註解這一行
DHT dht(DHTPIN, DHTTYPE);

// Wi-Fi 設定
const char* ssid = "eric"; // 替換為您的 WiFi 名稱
const char* password = "07140623"; // 替換為您的 WiFi 密碼

// NTP 設定
const long utcOffsetInSeconds = 8 * 3600; // 台北的時區是 UTC+8
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

// OLED 設定
#define SCL 12
#define SDA 14
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, /* clock=*/ SCL, /* data=*/ SDA, /* reset=*/ U8X8_PIN_NONE);

void setup() {
  // 初始化串口
  Serial.begin(115200);
  
  // 初始化 DHT
  dht.begin();

  // 初始化 OLED
  u8g2.begin();
  u8g2.enableUTF8Print(); // 允許 UTF-8 輸出
  
  // 顯示 WiFi 連線中的字樣
  u8g2.clearBuffer(); 
  u8g2.setFont(u8g2_font_unifont_t_chinese2); // 使用支援中文的字型
  const char* connectingText = "WiFi Link...";
  int width = u8g2.getDisplayWidth();
  int strWidth = u8g2.getStrWidth(connectingText);
  u8g2.drawStr((width - strWidth) / 2, 30, connectingText); // 文字置中
  u8g2.sendBuffer();

  // 連接到 WiFi
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) { // 嘗試連接最多 20 次
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected");

    // 顯示 WiFi 連線成功的字樣
    u8g2.clearBuffer(); 
    const char* successText = "Link successful!";
    int width = u8g2.getDisplayWidth();
    int strWidth = u8g2.getStrWidth(successText);
    u8g2.drawStr((width - strWidth) / 2, 30, successText); // 文字置中
    u8g2.sendBuffer();
    
    delay(2000); // 顯示 2 秒後進入主循環
  } else {
    Serial.println("Failed to connect to WiFi");
    while (true); // 停止程式運行
  }

  // 啟動 NTP 客戶端
  timeClient.begin();
}

void loop() {
  // 更新時間
  timeClient.update();

  // 取得格式化的時間
  String formattedTime = timeClient.getFormattedTime();

  // 取得當前的 epoch 時間
  unsigned long epochTime = timeClient.getEpochTime();

  // 將 epoch 時間轉換為日期和時間
  int year = 1970 + epochTime / 31536000; // 計算年份
  int month = (epochTime % 31536000) / 2628000; // 計算月份
  int day = (epochTime % 2628000) / 86400; // 計算日期
  int hour = (epochTime % 86400) / 3600; // 計算小時
  int minute = (epochTime % 3600) / 60; // 計算分鐘
  int second = epochTime % 60; // 計算秒鐘

  // 轉換為年月日格式的時間
  String dateTime =   String(hour) + ":" + String(minute) + ":" + String(second);

  // 取得溫濕度
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  // 顯示時間和溫濕度在 OLED 上
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_unifont_t_chinese2); // 使用支援中文的字型

  // 顯示時間
  const char* timeText = "Taipei:";
  int width = u8g2.getDisplayWidth();
  int strWidth = u8g2.getStrWidth(timeText);
  u8g2.drawStr((width - strWidth) / 2, 15, timeText); // 文字置中
  strWidth = u8g2.getStrWidth(dateTime.c_str());
  u8g2.drawStr((width - strWidth) / 2, 35, dateTime.c_str()); // 文字置中

  // 顯示溫濕度
  String humidityText = "H: " + String(h) + " %";
  String temperatureText = "T: " + String(t) + " C";

  if (!isnan(h) && !isnan(t)) { // 確保溫濕度數據有效
    u8g2.drawStr(0, 50, humidityText.c_str());
    u8g2.drawStr(0, 60, temperatureText.c_str());
    
    // 發送溫濕度數據到雲端
    WiFiClient client;
    if (client.connect("rfid.pcdevelop.com.tw", 80)) { // 連接到伺服器 (使用 HTTP)
      // 構建 JSON 資料
      String jsonData = "{\"time\":\"" + dateTime + "\",\"temperature\":" + String(t) + ",\"humidity\":" + String(h) + "}";
      
      client.println("POST /temperature.php HTTP/1.1");
      client.println("Host: rfid.pcdevelop.com.tw");
      client.println("Content-Type: application/json");
      client.print("Content-Length: ");
      client.println(jsonData.length());
      client.println(); // 空行，表示標頭結束
      client.println(jsonData); // 發送資料

      // 等待回應
      while (client.connected() || client.available()) {
        if (client.available()) {
          String response = client.readStringUntil('\n');
          Serial.println(response); // 打印伺服器回應
        }
      }

      client.stop(); // 關閉連接
    } else {
      Serial.println("Connection failed");
    }

  } else {
    u8g2.drawStr(0, 50, "Reading Error");
  }

  u8g2.sendBuffer();

  delay(10000); // 每 10 秒更新一次
}
