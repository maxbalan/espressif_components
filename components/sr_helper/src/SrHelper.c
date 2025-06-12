#include "SrHelper.h"

#include <string.h>

#include "esp_vfs_fat.h"

// #ifdef DEBUG_ENABLED
static const char *TAG = "SR";
// #endif

ESP_EVENT_DECLARE_BASE(SR_EVENT);
ESP_EVENT_DEFINE_BASE(SR_EVENT);

static esp_afe_sr_iface_t *afe_handle = NULL;
static esp_afe_sr_data_t *afe_data = NULL;
i2s_chan_handle_t rx_handle = NULL;

bool is_feed_active = false;
bool continue_wakeword_detection = true;
bool wakeword_detection_stop = true;

// --------------------- callback process ----------------------------------------
void sr_register_callback(esp_event_handler_t callback) {
    ESP_ERROR_CHECK(esp_event_handler_instance_register(SR_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        callback,
                                                        NULL,
                                                        NULL));
}

void sr_trigger_event(sr_event_t event) {
    esp_event_post(
        SR_EVENT,      // Event base
        event,         // Event ID
        NULL,          // Event data (if any)
        0,             // Event data size
        portMAX_DELAY  // Max delay to post the event
    );
}

// --------------------- recording process ----------------------------------------
esp_err_t write_file(char *filePath, int16_t *audio_buffer, int bytes_collected) {
    // Create WAV header using the provided macro
    wav_header_t wav_header = WAV_HEADER_PCM_DEFAULT(
        bytes_collected,  // wav_sample_size
        BITS_PER_SAMPLE,  // wav_sample_bits
        SAMPLE_RATE,      // wav_sample_rate
        CHANNELS          // wav_channel_num
    );

    // remove file if exists
    struct stat st;
    if (stat(filePath, &st) == 0) {
        unlink(filePath);
        ESP_LOGI(TAG, "file [%s] removed", filePath);
    }

    // Open file for writing
    FILE *fp = fopen(filePath, "wb");
    if (!fp) {
        ESP_LOGE(TAG, "Failed to open file %s for writing", filePath);
        stop_feed();
        return ESP_FAIL;
    }

    // Write WAV header
    size_t written = fwrite(&wav_header, sizeof(wav_header), 1, fp);
    if (written != 1) {
        ESP_LOGE(TAG, "Failed to write WAV header to %s", filePath);
        fclose(fp);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Writing audio buffer to file: %d", bytes_collected);

    // Write audio data in chunks to handle large buffers
    size_t bytes_remaining = bytes_collected;
    size_t offset = 0;
    while (bytes_remaining > 0) {
        size_t chunk_to_write = bytes_remaining > 10000 ? 10000 : bytes_remaining;
        written = fwrite(audio_buffer + offset, 1, chunk_to_write, fp);
        
        if (written != chunk_to_write) {
            ESP_LOGE(TAG, "Failed to write audio data to %s, wrote %d/%d bytes",filePath, written, chunk_to_write);
        
            fclose(fp);

            return ESP_FAIL;
        }
        
        bytes_remaining -= written;
        offset += written / sizeof(int16_t);
        // ESP_LOGI(TAG, "Wrote %d bytes, remaining: %d/%d", written, bytes_remaining, bytes_collected);
    }

    ESP_LOGI(TAG, "Written audio buffer to file, remaining: %d/%d", bytes_remaining, bytes_collected);

    fclose(fp);

    return ESP_OK;
}

esp_err_t record_to_file(void *arg) {
    char *filePath = (char *)arg;
    esp_err_t ret = ESP_OK;
    size_t bytes_read;
    int16_t *audio_buffer = malloc(BUFFER_SIZE);
    if (!audio_buffer) {
        ESP_LOGE(TAG, "Failed to allocate memory for audio buffer (%d bytes)", BUFFER_SIZE);
        stop_feed();
        sr_trigger_event(RECORDING_FAIL);
        return ESP_FAIL;
    }

    // Get AFE chunk size and allocate temp buffer
    int afe_chunk_size = afe_handle->get_fetch_chunksize(afe_data);
    int16_t *temp_buffer = (int16_t *)malloc(afe_chunk_size * sizeof(int16_t));
    if (!temp_buffer) {
        ESP_LOGE(TAG, "Failed to allocate temporary buffer (%d bytes)", afe_chunk_size * sizeof(int16_t));
        stop_feed();
        free(audio_buffer);
        sr_trigger_event(RECORDING_FAIL);
        return ESP_FAIL;
    }

    // Calculate total samples needed for 3 seconds
    int total_samples = SAMPLE_RATE * RECORD_SECONDS;
    int bytes_to_read = total_samples * sizeof(int16_t);
    int bytes_collected = 0;

    ESP_LOGI(TAG, "Starting 3-second audio recording to %s, AFE chunk size: %d bytes", filePath, afe_chunk_size * sizeof(int16_t));

    // Read audio data from AFE
    while (bytes_collected < bytes_to_read) {
        afe_fetch_result_t *res = afe_handle->fetch(afe_data);
        if (!res || res->ret_value == ESP_FAIL) {
            ESP_LOGE(TAG, "AFE fetch error");
            stop_feed();
            free(temp_buffer);
            free(audio_buffer);
            sr_trigger_event(RECORDING_FAIL);
            return ESP_FAIL;
        }

        // Copy AFE data to audio buffer
        size_t chunk_size = (bytes_to_read - bytes_collected) > afe_chunk_size * sizeof(int16_t) ? afe_chunk_size * sizeof(int16_t) : (bytes_to_read - bytes_collected);
        memcpy(temp_buffer, res->data, chunk_size);
        memcpy(audio_buffer + (bytes_collected / sizeof(int16_t)), temp_buffer, chunk_size);
        bytes_read = chunk_size;

        bytes_collected += bytes_read;
        // ESP_LOGI(TAG, "Read %d bytes from AFE, total collected: %d/%d", bytes_read, bytes_collected, bytes_to_read);
    }

    ESP_LOGI(TAG, "Recording done, total collected: %d/%d", bytes_collected, bytes_to_read);

    // stop the feed since we no longer need the data
    stop_feed();

    for (int i = 0; i < 3; i++) {
        ESP_LOGI(TAG, "try writing to file: %s", filePath);

        esp_err_t res = write_file(filePath, audio_buffer, bytes_collected);
        if (res == ESP_OK) {
            sr_trigger_event(RECORDING_SUCCESS);
            break;
        } else if (i == 2) {
            sr_trigger_event(RECORDING_FAIL);
            break;
        }
    }

    free(temp_buffer);  // Free temp buffer after loop
    free(audio_buffer);

    return ESP_OK;
}

recording_result_t* record_to_buffer() {
    recording_result_t *result = (recording_result_t *)malloc(sizeof(recording_result_t));
    if (!result) {
        ESP_LOGE(TAG, "Failed to allocate recording_result_t");
        stop_feed();
        sr_trigger_event(RECORDING_FAIL);
        return NULL;
    }
    result->data_buffer = NULL;
    result->data_buffer_size = 0;

    size_t bytes_read;
    int16_t *audio_buffer = malloc(BUFFER_SIZE);
    if (!audio_buffer) {
        ESP_LOGE(TAG, "Failed to allocate memory for audio buffer (%d bytes)", BUFFER_SIZE);
        stop_feed();
        free(result);
        sr_trigger_event(RECORDING_FAIL);
        return NULL;
    }

    // Get AFE chunk size and allocate temp buffer
    int afe_chunk_size = afe_handle->get_fetch_chunksize(afe_data);
    int16_t *temp_buffer = (int16_t *)malloc(afe_chunk_size * sizeof(int16_t));
    if (!temp_buffer) {
        ESP_LOGE(TAG, "Failed to allocate temporary buffer (%d bytes)", afe_chunk_size * sizeof(int16_t));
        stop_feed();
        free(audio_buffer);
        free(result);
        sr_trigger_event(RECORDING_FAIL);
        return NULL;
    }

    // Calculate total samples needed for 3 seconds
    int total_samples = SAMPLE_RATE * RECORD_SECONDS;
    int bytes_to_read = total_samples * sizeof(int16_t);
    int bytes_collected = 0;

    ESP_LOGI(TAG, "Starting 3-second audio recording, AFE chunk size: %d bytes", afe_chunk_size * sizeof(int16_t));

    // Read audio data from AFE
    while (bytes_collected < bytes_to_read) {
        afe_fetch_result_t *res = afe_handle->fetch(afe_data);
        if (!res || res->ret_value == ESP_FAIL) {
            ESP_LOGE(TAG, "AFE fetch error");
            stop_feed();
            free(temp_buffer);
            free(audio_buffer);
            free(result);
            sr_trigger_event(RECORDING_FAIL);
            return NULL;
        }

        // Copy AFE data to audio buffer
        size_t chunk_size = (bytes_to_read - bytes_collected) > afe_chunk_size * sizeof(int16_t) ? 
                            afe_chunk_size * sizeof(int16_t) : (bytes_to_read - bytes_collected);
        memcpy(temp_buffer, res->data, chunk_size);
        memcpy(audio_buffer + (bytes_collected / sizeof(int16_t)), temp_buffer, chunk_size);
        bytes_read = chunk_size;

        bytes_collected += bytes_read;
        // ESP_LOGI(TAG, "Read %d bytes from AFE, total collected: %d/%d", bytes_read, bytes_collected, bytes_to_read);
    }

    ESP_LOGI(TAG, "Recording done, total collected: %d/%d", bytes_collected, bytes_to_read);

    // Stop the feed since we no longer need the data
    stop_feed();

    // Create WAV header
    wav_header_t wav_header = WAV_HEADER_PCM_DEFAULT(
        bytes_collected,  // wav_sample_size
        BITS_PER_SAMPLE,  // wav_sample_bits
        SAMPLE_RATE,      // wav_sample_rate
        CHANNELS          // wav_channel_num
    );

    // Allocate output buffer for WAV file (header + audio data)
    size_t wav_buffer_size = sizeof(wav_header_t) + bytes_collected;
    uint8_t *wav_buffer = (uint8_t *)malloc(wav_buffer_size);
    if (!wav_buffer) {
        ESP_LOGE(TAG, "Failed to allocate WAV buffer (%d bytes)", wav_buffer_size);
        free(temp_buffer);
        free(audio_buffer);
        free(result);
        sr_trigger_event(RECORDING_FAIL);
        return NULL;
    }

    // Copy header and audio data to output buffer
    memcpy(wav_buffer, &wav_header, sizeof(wav_header_t));
    memcpy(wav_buffer + sizeof(wav_header_t), audio_buffer, bytes_collected);

    free(temp_buffer);
    free(audio_buffer);

    ESP_LOGI(TAG, "WAV buffer created, size: %d bytes", wav_buffer_size);
    sr_trigger_event(RECORDING_SUCCESS);

    result->data_buffer = wav_buffer;
    result->data_buffer_size = wav_buffer_size;
    
    return result;
}

void record_task(void *arg) {
    record_to_file(arg);
    vTaskDelete(NULL);
}

recording_result_t* wav_record() {
    start_feed();
    return record_to_buffer();
}

// --------------------- feed process ----------------------------------------
esp_err_t bsp_get_feed_data(int16_t *buffer, int buffer_len) {
    esp_err_t ret = ESP_OK;
    size_t bytes_read;
    int audio_chunksize = buffer_len / (sizeof(int32_t));

    ret = i2s_channel_read(rx_handle, buffer, buffer_len, &bytes_read, portMAX_DELAY);

    int32_t *tmp_buff = buffer;
    for (int i = 0; i < audio_chunksize; i++) {
        tmp_buff[i] = tmp_buff[i] >> 14;
        // 32:8 is the effective bit, 8:0 is the lower 8 bits, all are
        // 0, the input of AFE is 16-bit voice data, and 29:13 bits are
        // used to amplify the voice signal.
    }

    return ret;
}

void feed_task(void *arg) {
    int audio_chunksize = afe_handle->get_feed_chunksize(afe_data);
    int nch = afe_handle->get_channel_num(afe_data);
    int feed_channel = 2;
    assert(nch <= 2);

    int16_t *i2s_buff = malloc(audio_chunksize * sizeof(int16_t) * feed_channel);
    assert(i2s_buff);

    sr_trigger_event(SR_FEED_START);

    while (is_feed_active) {
        bsp_get_feed_data(i2s_buff, audio_chunksize * sizeof(int16_t) * feed_channel);
        afe_handle->feed(afe_data, i2s_buff);
    }

    if (i2s_buff) {
        free(i2s_buff);
        i2s_buff = NULL;
    }

    ESP_LOGI(TAG, "Feeding task stopped");

    sr_trigger_event(SR_FEED_STOP);
    vTaskDelete(NULL);
}

void start_feed() {
    // prevent multiple feed calls
    if (!is_feed_active) {
        is_feed_active = true;
        xTaskCreatePinnedToCore(&feed_task, "feed", 10 * 1024, NULL, 5, NULL, 1);
    } else {
        ESP_LOGI(TAG, "feed already in progress");
    }
}

void stop_feed() {
    is_feed_active = false;
    afe_handle->reset_buffer(afe_data);
}

// --------------------- wakeword process ----------------------------------------
void wakeup_word_detect_task(void *arg) {
    esp_afe_sr_data_t *afe_data = arg;
    //     int detect_flag = false;
    // int afe_chunksize = afe_handle->get_fetch_chunksize(afe_data);

    ESP_LOGI(TAG, "wakeup word detect start");

    sr_trigger_event(SR_WAKEWORD_START);
    while (true && continue_wakeword_detection) {
        afe_fetch_result_t *res = afe_handle->fetch(afe_data);
        if (!res || res->ret_value == ESP_FAIL) {
            ESP_LOGE(TAG, "data fetch error");
            break;
        }

        if (res->wakeup_state == WAKENET_DETECTED) {
            ESP_LOGI(TAG, "WAKEWORD DETECTED");

            sr_trigger_event(SR_WAKEWORD_DETECTED);

            // stop task if wakeup word was detected
            if (wakeword_detection_stop) {
                break;
            }
        }
    }

    stop_feed();

    ESP_LOGI(TAG, "wakeup word detect exit");

    sr_trigger_event(SR_WAKEWORD_STOP);
    vTaskDelete(NULL);
}

void start_wakeup_listener() {
    start_feed();
    xTaskCreatePinnedToCore(&wakeup_word_detect_task, "detect", 8 * 1024, (void *)afe_data, 10, NULL, 1);
}

void stop_wakeup_listener() {
    continue_wakeword_detection = false;
}

// --------------------- mic init ----------------------------------------
esp_err_t init_microphone(i2s_std_gpio_config_t config) {
    esp_err_t ret_val = ESP_OK;

    /* RX channel will be registered on our second I2S (for now)*/
    i2s_chan_config_t rx_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    i2s_new_channel(&rx_chan_cfg, NULL, &rx_handle);
    i2s_std_config_t std_rx_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = GPIO_NUM_5,
            .ws = GPIO_NUM_6,
            .dout = I2S_GPIO_UNUSED,
            .din = GPIO_NUM_4,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    std_rx_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
    // ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &std_rx_cfg));
    // ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));

    ret_val |= i2s_channel_init_std_mode(rx_handle, &std_rx_cfg);
    ret_val |= i2s_channel_enable(rx_handle);

    return ret_val;
}

// --------------------- init process ----------------------------------------

esp_afe_sr_iface_t sr_init(afe_config_t config, i2s_std_gpio_config_t micConfig) {
    init_microphone(micConfig);

    srmodel_list_t *models = esp_srmodel_init("model");

    ESP_LOGI(TAG, "model count [%d]", models->num);

    for (int i = 0; i < models->num; i++) {
        ESP_LOGI(TAG, "listing model [%s]", models->model_name[i]);
    }

    config.wakenet_init = true;
    config.wakenet_model_name = esp_srmodel_filter(models, ESP_WN_PREFIX, NULL);

    if (config.wakenet_model_name == NULL) {
        ESP_LOGE(TAG, "failed to load WN model");
    }

    ESP_LOGI(TAG, "wake word model used [%s]", config.wakenet_model_name);

    afe_handle = (esp_afe_sr_iface_t *)&ESP_AFE_SR_HANDLE;
    afe_data = afe_handle->create_from_config(&config);

    sr_trigger_event(SR_SYSTEM_READY);
    return *afe_handle;
}
