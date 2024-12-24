// DFRobotDFPlayerMini函式庫下載：https://github.com/DFRobot/DFRobotDFPlayerMini

#include <Arduino.h>
#include <DFRobotDFPlayerMini.h>
#include <HTTPClient.h>
#include <SimpleDHT.h>
#include <U8g2lib.h>
#include <WiFiMulti.h>
#include <time.h>

#define LED_BUILTIN 2
#define TIME_ZONE 8

// 設定字型
#define SF0 u8g2_font_trixel_square_tf  // 4*6 字型(init 說明字型)

enum WeatherCondition { CLOUDY = 0, RAIN = 3, SUM = 5, UNKNOWN = 6 };
enum ScrollDirection { UP, DOWN, NONE };

struct weather_t {
  String location;
  float temperature;
  WeatherCondition condition;

  bool with_local;
  int local_hum;
  int local_temp;
} weather;

HTTPClient http;
WiFiMulti wifiMulti;
SimpleDHT11 dht(4);
HardwareSerial SerialDF(1);
DFRobotDFPlayerMini myDFPlayer;
U8G2_MAX7219_32X8_F_4W_SW_SPI u8g2(U8G2_R0, /* clock=*/25, /* data=*/32,
                                   /* cs=*/33, /* dc=*/U8X8_PIN_NONE,
                                   /* reset=*/U8X8_PIN_NONE);

void TaskNTP(void *);
void TaskDisplay(void *);
void TaskWeather(void *);

xSemaphoreHandle xDataMutex = NULL;

void setup() {
  Serial.begin(115200);
  SerialDF.begin(9600, SERIAL_8N1, 12, 13);
  WiFi.begin();

  xDataMutex = xSemaphoreCreateMutex();
  if (xDataMutex == NULL) {
    Serial.println("xDataMutex is NULL");
    while (1);
  }

  // myDFPlayer.volume(25);
  // myDFPlayer.start();
  // myDFPlayer.playMp3FolderSync(1, 30 * 1e3);
  // myDFPlayer.pause();
  // myDFPlayer.sleep();

  wifiMulti.addAP("computer05", "pmai0000");

  xTaskCreate(TaskNTP, "NTPTask", 512 * 3, NULL, 4, NULL);
  xTaskCreate(TaskDisplay, "DisplayTask", 512 * 6, NULL, 6, NULL);
  xTaskCreate(TaskWeather, "WeatherTask", 512 * 6, NULL, 4, NULL);
  // delay(1000);
  // pinMode(34, INPUT);
  // myDFPlayer.begin(SerialDF, false);
  // delay(500);
}

void loop() {
  for (uint8_t i = 0; i < 3; i++) {
    if (xSemaphoreTake(xDataMutex, 0) == pdTRUE) {
      byte temperature = 0, humidity = 0;
      int err;
      if ((err = dht.read(&temperature, &humidity, NULL)) ==
          SimpleDHTErrSuccess) {
        weather.with_local = true;

        weather.local_hum = (int)humidity;
        weather.local_temp = (int)temperature;

        Serial.print("Temperature = ");
        Serial.print(weather.local_temp);
        Serial.print(" °C\t");
        Serial.print("Humidity = ");
        Serial.print(weather.local_hum);
        Serial.println(" %");

        xSemaphoreGive(xDataMutex);
        break;
      } else {
        weather.with_local = false;

        Serial.print("Read DHT11 failed, err=");
        Serial.print(SimpleDHTErrCode(err));
        Serial.print(",");
        Serial.println(SimpleDHTErrDuration(err));
      }
      xSemaphoreGive(xDataMutex);
    }
    vTaskDelay(100);
  }
  vTaskDelay(1000 * 10);
}

void TaskDisplay(void *) {
  Serial.println("TaskDisplay Start");
  u8g2.begin();

  int count = 0, hourOffsetY = 0, minOffsetY = 0, secOffsetY = 0;
  bool weatheringVisible = false;
  unsigned long lastScrollMillis = 0, nextWeatheringShowMillis = 0,
                weatheringScrollEndMillis = 0;
  ScrollDirection weatherIconDirection = DOWN, lastWeatherIconDirection = NONE;

  u8g2.setFont(SF0);
  u8g2.drawStr(0, 7, "loading...");
  u8g2.sendBuffer();
  while (WiFi.status() != WL_CONNECTED);

  while (1) {
    if (xSemaphoreTake(xDataMutex, 0) == pdTRUE) {
      struct tm timeinfo;
      if (getLocalTime(&timeinfo)) {
        int now = millis();

        if (weatheringVisible) {
          u8g2.setFont(u8g2_font_open_iconic_weather_1x_t);
          u8g2.drawGlyph(2, -8 + hourOffsetY, 0x40 + weather.condition);

          u8g2.setFont(SF0);
          if (!weather.with_local) {
            u8g2.drawStr(3 + 12, -10 + minOffsetY, "-");
            u8g2.drawStr(3 + 22, -10 + secOffsetY, "-");
          } else {
            u8g2.drawStr(3 + 10, -9 + minOffsetY,
                         u8x8_u8toa(weather.local_temp, 2));
            u8g2.drawStr(3 + 20, -9 + secOffsetY,
                         u8x8_u8toa(weather.local_hum, 2));
          }

          if (weatherIconDirection != NONE) {
            if (now - lastScrollMillis > 80) {
              lastScrollMillis = now;
              lastWeatherIconDirection = weatherIconDirection;

              bool clear = false;
              if (weatherIconDirection == UP) {
                if (count < 16 + 2) {
                  count++;
                  hourOffsetY = min(count, 16);
                  minOffsetY = min(count - 1, 16);
                  secOffsetY = min(count - 2, 16);
                } else clear = true;
              } else if (weatherIconDirection == DOWN) {
                if (count > 0 - 2) {
                  count--;
                  hourOffsetY = max(count, 0);
                  minOffsetY = max(count - 1, 0);
                  secOffsetY = max(count - 2, 0);
                } else clear = true;
              }

              if (clear) {
                weatherIconDirection = NONE;
                weatheringScrollEndMillis = now;
              }
            }
          } else if (now - weatheringScrollEndMillis > 5000) {
            if (lastWeatherIconDirection == UP) weatherIconDirection = DOWN;
            else weatheringVisible = false;
          }
        }

        u8g2.setFont(SF0);
        u8g2.drawStr(3 + 0, 7 + hourOffsetY, u8x8_u8toa(timeinfo.tm_hour, 2));
        u8g2.drawStr(3 + 10, 7 + minOffsetY, u8x8_u8toa(timeinfo.tm_min, 2));
        u8g2.drawStr(3 + 20, 7 + secOffsetY, u8x8_u8toa(timeinfo.tm_sec, 2));

        int millisecond = now % 1000;
        if (millisecond / 400) {
          u8g2.drawStr(3 + 8, 6 + minOffsetY, ":");
          u8g2.drawStr(3 + 18, 6 + secOffsetY, ":");
        }

        if (weather.condition != WeatherCondition::UNKNOWN &&
            nextWeatheringShowMillis < now) {
          count = 0;
          weatheringVisible = true;
          lastWeatherIconDirection = weatherIconDirection = UP;
          nextWeatheringShowMillis = now + 20 * 1000 * 1;
        }

        u8g2.sendBuffer();
        u8g2.clearBuffer();
      } else Serial.println("Failed to obtain time");

      xSemaphoreGive(xDataMutex);
    }
    vTaskDelay(5);
  }
}

void TaskNTP(void *) {
  Serial.println("TaskNTP Start");
  while (1) {
    if (WiFi.status() != WL_CONNECTED) {
      vTaskDelay(1000);
      continue;
    }

    Serial.println("Configuring time...");
    configTime(TIME_ZONE * 3600, 0, "pool.ntp.org", "1.pool.ntp.org",
               "2.pool.ntp.org");
    vTaskDelay(1000 * 60 * 60);  // 60min
  }
}

void TaskWeather(void *) {
  Serial.println("TaskWeather Start");
  while (1) {
    if (WiFi.status() != WL_CONNECTED) {
      vTaskDelay(1000);
      continue;
    }

    // https://wttr.in/?format=%l\n%x\n%f
    http.begin("https://wttr.in/?format=%25l%0A%25x%0A%25f");
    int httpCode = http.GET();
    if (httpCode > 0) {
      if (httpCode == HTTP_CODE_OK) {
        // Bailing, Taiwan
        // /
        // +16°C
        String buf = "";
        String payload = http.getString();
        Serial.println(payload);
        int index = 0;

        if (xSemaphoreTake(xDataMutex, 0) == pdTRUE) {
          for (uint8_t i = 0; i < payload.length(); i++) {
            if (payload.charAt(i) == '\n') {
              index++;
              // location
              if (index == 1) weather.location = buf;
              // condition
              else if (index == 2) {
                if (buf == "o") weather.condition = SUM;
                else if (buf == "mm" || buf == "=" || buf == "m" ||
                         buf == "mmm") {
                  weather.condition = CLOUDY;
                } else if (buf == "///" || buf == "//" || buf == "**" ||
                           buf == "*/*" || buf == "/" || buf == "." ||
                           buf == "x" || buf == "x/" || buf == "*" ||
                           buf == "*/" || buf == "/!/" || buf == "!/") {
                  weather.condition = RAIN;
                } else {  // ?
                  weather.condition = UNKNOWN;
                }
              }
              // temperature
              else if (index == 3) {
                int temperature = buf.substring(1, buf.indexOf("°C")).toFloat();
                weather.temperature = temperature;
              }
              buf = "";
            } else buf += payload.charAt(i);
          }
          xSemaphoreGive(xDataMutex);
        }
      }
    } else {
      Serial.printf("[HTTP] GET... failed, error: %s\n",
                    http.errorToString(httpCode).c_str());
    }
    http.end();

    vTaskDelay(1000 * 60 * 30);  // 30min
  }
}
