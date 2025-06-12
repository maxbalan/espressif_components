#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/unistd.h>

#include "driver/i2s_std.h"
#include "esp_afe_sr_iface.h"
#include "esp_afe_sr_models.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "format_wav.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "model_path.h"
#include "synchroniser.h"

#define SAMPLE_RATE 16000
#define BITS_PER_SAMPLE 16
#define CHANNELS 1
#define RECORD_SECONDS 3
#define BYTE_RATE (SAMPLE_RATE * (BITS_PER_SAMPLE / 8) * CHANNELS)  // 16000 * 2 = 32000 bytes per second
#define BUFFER_SIZE ((SAMPLE_RATE * (BITS_PER_SAMPLE / 8) * CHANNELS) * RECORD_SECONDS)

typedef struct {
    uint8_t* data_buffer;
    uint32_t data_buffer_size;
} recording_result_t;

// setup
esp_afe_sr_iface_t sr_init(afe_config_t config, i2s_std_gpio_config_t micConfig);

// data feed
void start_feed();
void stop_feed();

// wakeup word process
void start_wakeup_listener();
void stop_wakeup_listener();

// recording
recording_result_t* wav_record();

// events setup
typedef enum {
    SR_FEED_STOP = 0,
    SR_FEED_START,
    SR_WAKEWORD_START,
    SR_WAKEWORD_STOP,
    SR_WAKEWORD_DETECTED,
    SR_SYSTEM_READY,
    RECORDING_SUCCESS,
    RECORDING_FAIL,
} sr_event_t;

void sr_register_callback(esp_event_handler_t callback);