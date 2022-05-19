/**
   @file streams-i2s-a2dp.ino
   @author Phil Schatzmann
   @brief see https://github.com/pschatzmann/arduino-audio-tools/blob/main/examples/examples-stream/stream-i2s-a2dp/README.md

   @author Phil Schatzmann
   @copyright GPLv3
*/

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

const char* LOG_TAG = "iPodN3GBluetooth";

// Arduino Setup
void setup(void) {
  Serial.begin(9600);

  // start i2s input with default configuration
  ESP_LOGI(LOG_TAG, "starting I2S... ");
  auto config = in.defaultConfig(RX_MODE);
  config.i2s_format = I2S_STD_FORMAT; // if quality is bad change to I2S_MSB_FORMAT https://github.com/pschatzmann/arduino-audio-tools/issues/23
  config.sample_rate = 44100;
  config.channels = 2;
  config.bits_per_sample = 16;
  config.is_master = false;
  config.pin_bck = I2S_PIN_BCK;
  config.pin_ws = I2S_PIN_WS;
  config.pin_data = I2S_PIN_DATA;
  config.use_apll = false;
  in.begin(config);

  // start bluetooth
  ESP_LOGI(LOG_TAG, "starting A2DP...");
  auto cfgA2DP = out.defaultConfig(TX_MODE);
  cfgA2DP.name = "OontZ Angle 3 DS 66B";
  pt_event_queue = cfgA2DP.passthrough_event_queue;
  out.begin(cfgA2DP);
  out.setVolume(0.3);

  ESP_LOGI(LOG_TAG, "Done!");
}

// Arduino loop - copy data
void loop() {
  // process audio
  copier.copy();

  esp_avrc_tg_cb_param_t::avrc_tg_psth_cmd_param event;

  if(pt_event_queue) {
    xQueueReceive(pt_event_queue, &event, 0);
    if(event.key_code != 0) passthrough_callback(event);
  }
}

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
