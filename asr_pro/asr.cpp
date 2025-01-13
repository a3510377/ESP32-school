// 簡體字是因為模型生成時用來抓取用的
/* asr-pro:
  ESP32  --- asr-pro
  17(TX) <-> A3(RX)
  16(RX) <-> A2(TX) */

#include "asr.h"

#include "ci_log.h"
extern "C" {
void* __dso_handle = 0;
}
#include "HardwareSerial.h"
#include "SerialCommand.h"
#include "myLib/asr_event.h"
#include "setup.h"

uint32_t snid;

SerialCommand sCmd(&Serial1);
xSemaphoreHandle xSerial1Mutex;

void ASR_CODE();
void TaskSerial1Play(void*);

//{speak:小蝶-清新女声,vol:10peed:10,platform:haohaodada}
//{ID:10250,keyword:"命令词",ASR:"最大音量",ASRTO:"音量调整到最大"}
//{ID:10251,keyword:"命令词",ASR:"中等音量",ASRTO:"音量调整到中等"}
//{ID:10252,keyword:"命令词",ASR:"最小音量",ASRTO:"音量调整到最小"}

void hardware_init() {
  vol_set(VOLUME_MID);
  sCmd.check();
  vTaskDelete(NULL);
}

void setup() {
  // set_wakeup_forever();

  //  TX,  RX
  // PB5, PB6
  setPinFun(13, SECOND_FUNCTION);
  setPinFun(14, SECOND_FUNCTION);
  Serial.begin(115200);

  //  TX,  RX
  // PA2, PA3
  setPinFun(2, FORTH_FUNCTION);
  setPinFun(3, FORTH_FUNCTION);
  Serial1.begin(115200);

  //{playid:10001,voice:请用 嘿俊哥 唤醒我}
  //{playid:10002,voice:}
  //{ID:0,keyword:"唤醒词",ASR:"嘿俊哥",ASRTO:"我在"}

  //{playid:10084,voice:零}
  //{playid:10085,voice:一}
  //{playid:10086,voice:二}
  //{playid:10087,voice:三}
  //{playid:10088,voice:四}
  //{playid:10089,voice:五}
  //{playid:10090,voice:六}
  //{playid:10091,voice:七}
  //{playid:10092,voice:八}
  //{playid:10093,voice:九}
  //{playid:10094,voice:十}
  //{playid:10095,voice:百}
  //{playid:10096,voice:千}
  //{playid:10097,voice:万}
  //{playid:10098,voice:亿}
  //{playid:10099,voice:负}
  //{playid:10100,voice:点}

  //{playid:10600,voice:上}
  //{playid:10601,voice:下}
  //{playid:10602,voice:午}
  //{playid:10603,voice:点}
  //{playid:10604,voice:分}
  //{playid:10605,voice:秒}
  //{playid:10606,voice:年}
  //{playid:10607,voice:月}
  //{playid:10608,voice:日}
  //{playid:10609,voice:星期}

  //{playid:10810,voice:已}
  //{playid:10811,voice:关闭}
  //{playid:10812,voice:开启}
  //{playid:10813,voice:失败}
  //{playid:10814,voice:风扇}
  //{playid:10815,voice:电灯}
  //{playid:10816,voice:帮浦}

  //{playid:10820,voice:当前}
  //{playid:10821,voice:为}
  //{playid:10822,voice:PH值}
  //{playid:10823,voice:温度}
  //{playid:10824,voice:湿度}
  //{playid:10825,voice:土壤湿度}
  //{playid:10826,voice:度}
  //{playid:10828,voice:百分之}
  //{playid:10829,voice:亮度}

  //{playid:10840,voice:获取数据失败}

  //{playid:10850,voice:天气}
  //{playid:10851,voice:晴}
  //{playid:10852,voice:雨}
  //{playid:10853,voice:阴}
  //{playid:10854,voice:天}

  while (1) {
    xSerial1Mutex = xSemaphoreCreateMutex();
    if (!xSerial1Mutex) {
      Serial.println("xSerial1Mutex create failed");
      vTaskDelay(1000);
    } else break;
  }

  xTaskCreate(TaskSerial1Play, "UART1_RX", 256, NULL, 4, NULL);
}

void TaskSerial1Play(void*) {
  while (1) {
    if (Serial1.available() && xSemaphoreTake(xSerial1Mutex, 200) == pdTRUE) {
      enter_wakeup(5000);

      String str = sCmd.readData("\n");
      Serial1.println(str);
      str.trim();
      int colonIndex = str.indexOf(":");
      if (colonIndex == -1) {
        xSemaphoreGive(xSerial1Mutex);
        continue;
      }

      String key = str.substring(0, colonIndex);
      String value = str.substring(colonIndex + 1);
      if (key == "PLAY") {
        int id = value.toInt();
        play_audio(id);
      } else if (key == "PLAY_NUM") {
        int num = value.toInt();
        play_num(num, 1);
      }
      xSemaphoreGive(xSerial1Mutex);
    }
    vTaskDelay(1);
  }
}

void ASR_CODE() {
  set_state_enter_wakeup(1e3 * 10);  // 10s
  if (xSemaphoreTake(xSerial1Mutex, 200) != pdTRUE) {
    Serial.println("xSerial1Mutex take failed in ASR_CODE");
    return;
  }

  switch (snid) {
    //{ID:1,keyword:"命令词",ASR:"当前温度",ASRTO:""}
    //{ID:2,keyword:"命令词",ASR:"现在温度",ASRTO:""}
    case 1:
    case 2: {
      sCmd.writeData("CMD:TMP");
      sCmd.flush();

      String tmp = sCmd.readCallback("TMP");
      Serial1.print("TMP: ");
      Serial1.println(tmp);

      if (tmp == "") {
        play_audio(10840);  // 获取数据失败
        break;
      }

      play_audio(10820);  // 当前
      play_audio(10823);  // 温度
      play_audio(10821);  // 为
      play_num((int64_t)(tmp.toFloat() * 100), 1);
      play_audio(10826);  // 度
      break;
    }

    //{ID:3,keyword:"命令词",ASR:"当前湿度",ASRTO:""}
    //{ID:4,keyword:"命令词",ASR:"现在湿度",ASRTO:""}
    case 3:
    case 4: {
      sCmd.writeData("CMD:HUM");
      sCmd.flush();

      String hum = sCmd.readCallback("HUM");
      Serial1.print("TMP: ");
      Serial1.println(hum);

      if (hum == "") {
        play_audio(10840);  // 获取数据失败
        break;
      }

      play_audio(10820);  // 当前
      play_audio(10824);  // 湿度
      play_audio(10821);  // 为
      play_audio(10828);  // 百分之
      play_num((int64_t)(hum.toFloat() * 100), 1);
      break;
    }

    //{ID:5,keyword:"命令词",ASR:"当前天气",ASRTO:""}
    //{ID:6,keyword:"命令词",ASR:"现在天气",ASRTO:""}
    //{ID:7,keyword:"命令词",ASR:"天气如何",ASRTO:""}
    case 5:
    case 6:
    case 7: {
      sCmd.writeData("CMD:WAT");
      sCmd.flush();

      String wat = sCmd.readCallback("WAT");
      Serial1.print("WAT: ");
      Serial1.println(wat);

      int watID = wat.toInt() + 1;
      if (wat == "" || watID > 4) {
        play_audio(10840);  // 获取数据失败
        break;
      }

      play_audio(10820);  // 当前
      play_audio(10850);  // 天气
      play_audio(10821);  // 为
      play_audio(10821);  // 为

      // 10851 -> 晴
      // 10852 -> 雨
      // 10853 -> 阴
      // 10854 -> 天
      play_audio(10850 + watID);
    } break;
  }

  xSemaphoreGive(xSerial1Mutex);
}
