#define USE_A2DP

#include "AudioTools.h"
#include "esp_avrc_api.h"

#define I2S_PIN_BCK         4
#define I2S_PIN_WS          16
#define I2S_PIN_DATA        17

I2SStream in;                            // Access I2S as stream
A2DPStream out = A2DPStream::instance(); // access A2DP as stream
StreamCopy copier(out, in);              // copy i2sStream to a2dpStream

QueueHandle_t pt_event_queue;
QueueHandle_t scan_result_queue;

const char* LOG_TAG = "iPodN3GBluetooth";

scan_result_msg_t results[20];
char device_name_buffer[ESP_BT_GAP_MAX_BDNAME_LEN + 1];
char device_name[ESP_BT_GAP_MAX_BDNAME_LEN + 1];
char last_dev_buffer[ESP_BT_GAP_MAX_BDNAME_LEN + 1];
uint8_t nameidx = 0;

TaskHandle_t audio_copy_task_handle = NULL;
TaskHandle_t ipod_control_task_handle = NULL;
TaskHandle_t reset_command_task_handle = NULL;

// Arduino Setup
void setup(void) {
  Serial.begin(9600);
  Serial.print("\n\nstarting\n");

  memset((void*)device_name_buffer, 0, ESP_BT_GAP_MAX_BDNAME_LEN + 1);

  // start bluetooth
  ESP_LOGI(LOG_TAG, "starting A2DP...");
  auto cfgA2DP = out.defaultConfig(TX_MODE);
  cfgA2DP.name = (char*)"test";
  pt_event_queue = cfgA2DP.passthrough_event_queue;
  scan_result_queue = cfgA2DP.scan_result_queue;

  // step 1: begin the A2DP connection. my modifications make it no longer block,
  //         so we have to manage the connection state ourselves.
  out.begin(cfgA2DP);

  // step 2: listen for events on the scan result queue. when we get one, we write it to serial
  //         if we get a name back over serial, then we submit it back on the queue.
  scan_result_msg_t result;
  bool looking = true;

  while(true) {
    if(Serial.available() > 0) {
      char c = Serial.read();
      if(c > 0x7f) {}
      else if(c == '\n') {
        nameidx = 0;
        if(strncmp(device_name_buffer, "reset", 5) == 0) {
          ESP.restart();
        } else {
          strcpy(device_name, device_name_buffer);
          out.a2dp_source->set_local_name(device_name);
          break;
        }
      } else {
        device_name_buffer[nameidx] = c;
        nameidx++;
      }
    }
    
    if(xQueueReceive(scan_result_queue, &result, 10) == pdTRUE) {
      if(strcmp((char*) result.name, last_dev_buffer) != 0 || ESP.getCycleCount() % 1000 == 0) {
        Serial.print("device: ");
        Serial.print((char*)result.name);
        Serial.print("\n");
        strncpy(last_dev_buffer, (char*) result.name, ESP_BT_GAP_MAX_BDNAME_LEN);
      }
    }
  }

  nameidx = 0;

  // step 3: after submitting it back on the queue, we continuously poll is connected before proceeding
  while(!out.a2dp_source->is_connected()){
    Serial.print("connecting to ");
    Serial.print(device_name);
    Serial.print("\n");
    
    if(Serial.available() > 0) {
      char c = Serial.read();
      if(c > 0x7f) {}
      else if(c == '\n') {
        if(strcmp(device_name_buffer, "reset") == 0) {
          ESP.restart();
        } else {
          nameidx = 0;
          memset((void*)device_name_buffer, 0, ESP_BT_GAP_MAX_BDNAME_LEN + 1);
        }
      } else {
        device_name_buffer[nameidx] = c;
        nameidx++;
      }
    }
    delay(1000);
  }

  Serial.print("connected\n");

//  ESP_LOGI(LOG_TAG, "starting I2S... ");
  Serial.print("starting I2S...\n");
  auto config = in.defaultConfig(RX_MODE);
  config.i2s_format = I2S_STD_FORMAT;
  config.sample_rate = 44100;
  config.channels = 2;
  config.bits_per_sample = 16;
  config.is_master = false;
  config.pin_bck = I2S_PIN_BCK;
  config.pin_ws = I2S_PIN_WS;
  config.pin_data = I2S_PIN_DATA;
  config.use_apll = false;
  in.begin(config);
  Serial.print("I2S ready\n");

  // step 4: notify the dependent audio stream
  out.notifyBaseInfo(44100);
  
  ESP_LOGI(LOG_TAG, "Done!");

  BaseType_t xReturned = xTaskCreate(audio_copy_task, "AudioCopyTask", 1024 * 8, NULL, tskIDLE_PRIORITY, &audio_copy_task_handle);
  if(xReturned != pdPASS) {
    Serial.print("error making the audio copy task. restarting.\n");
    ESP.restart();
  }

  xReturned = xTaskCreate(ipod_control_task, "iPodControlTask", 1024 * 4, NULL, tskIDLE_PRIORITY, &ipod_control_task_handle);
  if(xReturned != pdPASS) {
    Serial.print("error making the audio copy task. restarting.\n");
    ESP.restart();
  }

  xReturned = xTaskCreate(reset_command_task, "ResetCommandTask", 1024 * 4, NULL, tskIDLE_PRIORITY, &reset_command_task_handle);
  if(xReturned != pdPASS) {
    Serial.print("error making the audio copy task. restarting.\n");
    ESP.restart();
  }

  Serial.print("Done\n");
}

void audio_copy_task(void* pvParameters) {
  for( ;; ) copier.copy();
}

void ipod_control_task(void* pvParameters) {
  esp_avrc_tg_cb_param_t::avrc_tg_psth_cmd_param event;
  esp_bt_gap_cb_param_t result;

  for( ;; ) {
    if(pt_event_queue) {
      if(xQueueReceive(pt_event_queue, &event, 0) == pdTRUE) {
        if(event.key_code != 0) passthrough_callback(event);
      }
    }
  }
  taskYIELD();
}

void reset_command_task(void* pvParameters) {
  char serial_buf[512];
  int serialidx = 0;
  
  memset((void*)serial_buf, 0, 512);
  
  for( ;; ) {
    if(Serial.available() > 0) {
      char c = Serial.read();
      if(c > 0x7f) {}
      else if(c == '\n' || serialidx >= 512) {
        if(strcmp(serial_buf, "reset") == 0) {
          ESP.restart();
        } else {
          serialidx = 0;
          memset((void*)serial_buf, 0, 512);
        }
      } else {
        serial_buf[nameidx] = c;
        serialidx++;
      }
    }
  }
  taskYIELD();
}

void loop() {}

void passthrough_callback(esp_avrc_tg_cb_param_t::avrc_tg_psth_cmd_param param) {
  // key codes: https://github.com/espressif/esp-idf/blob/a82e6e63d98bb051d4c59cb3d440c537ab9f74b0/components/bt/host/bluedroid/api/include/api/esp_avrc_api.h#L44-L102
  // key states: 0 = Pressed, 1 = Released
  if(param.key_state != 0) return;

  switch(param.key_code) {
    case ESP_AVRC_PT_CMD_VOL_UP:
      Serial.write((uint8_t*)"\xFFU\x03\x02\x00\x02\xF9\xFFU\x03\x02\x00\x00\xFB", 14);
      break;
    case ESP_AVRC_PT_CMD_VOL_DOWN:
      Serial.write((uint8_t*)"\xFFU\x03\x02\x00\x04\xF7\xFFU\x03\x02\x00\x00\xFB", 14);
      break;
    case ESP_AVRC_PT_CMD_FORWARD:
    case ESP_AVRC_PT_CMD_FAST_FORWARD:
      Serial.write((uint8_t*)"\xFFU\x03\x02\x00\b\xF3\xFFU\x03\x02\x00\x00\xFB", 14);
      break;
    case ESP_AVRC_PT_CMD_BACKWARD:
    case ESP_AVRC_PT_CMD_REWIND:
      Serial.write((uint8_t*)"\xFFU\x03\x02\x00\x10\xEB\xFFU\x03\x02\x00\x00\xFB", 14);
      break;
    case ESP_AVRC_PT_CMD_STOP:
      Serial.write((uint8_t*)"\xFFU\x03\x02\x00\x80{\xFFU\x03\x02\x00\x00\xFB", 14);
      break;
    case ESP_AVRC_PT_CMD_PLAY:
      Serial.write((uint8_t*)"\xFFU\x04\x02\x00\x00\x01\xF9\xFFU\x03\x02\x00\x00\xFB", 15);
      break;
    case ESP_AVRC_PT_CMD_PAUSE:
      Serial.write((uint8_t*)"\xFFU\x04\x02\x00\x00\x02\xF8\xFFU\x03\x02\x00\x00\xFB", 15);
      break;
    case ESP_AVRC_PT_CMD_MUTE:
      Serial.write((uint8_t*)"\xFFU\x04\x02\x00\x00\x04\xF6\xFFU\x03\x02\x00\x00\xFB", 15);
      break;
    case ESP_AVRC_PT_CMD_ROOT_MENU:
      Serial.write((uint8_t*)"\xFFU\x05\x02\x00\x00\x00@\xB9\xFFU\x03\x02\x00\x00\xFB", 16);
      break;
    case ESP_AVRC_PT_CMD_SELECT:
      Serial.write((uint8_t*)"\xFFU\x05\x02\x00\x00\x00\x80y\xFFU\x03\x02\x00\x00\xFB", 16);
      break;
    case ESP_AVRC_PT_CMD_UP:
      Serial.write((uint8_t*)"\xFFU\x06\x02\x00\x00\x00\x00\x01\xF7\xFFU\x03\x02\x00\x00\xFB", 17);
      break;
    case ESP_AVRC_PT_CMD_DOWN:
      Serial.write((uint8_t*)"\xFFU\x06\x02\x00\x00\x00\x00\x02\xF6\xFFU\x03\x02\x00\x00\xFB", 17);
      break;
    default:
      break;
  }

  Serial.flush();
}
