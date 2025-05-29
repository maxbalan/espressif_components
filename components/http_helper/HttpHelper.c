#include "HttpHelper.h"

static const char* TAG = "Http Client >>> ";

// --------------------- callback process ----------------------------------------
ESP_EVENT_DECLARE_BASE(HTTP_EVENT);
ESP_EVENT_DEFINE_BASE(HTTP_EVENT);

void http_register_callback(esp_event_handler_t callback) {
    ESP_ERROR_CHECK(esp_event_handler_instance_register(HTTP_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        callback,
                                                        NULL,
                                                        NULL));
}

void http_trigger_event(http_event_t event, char* event_data) {
    int l = 0;
    if (event_data != NULL) {
        l = strlen(event_data) + 1;
    }

    esp_event_post(
        HTTP_EVENT,    // Event base
        event,         // Event ID
        event_data,    // Event data (if any)
        l,             // Event data size
        portMAX_DELAY  // Max delay to post the event
    );
}

// private methods
esp_http_client_handle_t init_connection(http_client_config config, int content_length) {
    esp_http_client_config_t clientConfig = {
        .url = config.url,
        .method = config.method,
    };

    esp_http_client_handle_t client = esp_http_client_init(&clientConfig);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return NULL;
    }

    esp_err_t err = esp_http_client_open(client, content_length);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        //    xEventGroupSetBits(http_download_group, HTTP_DOWNLOAD_FAIL);
        return NULL;
    }

    return client;
}

http_client_json_response read_json_response(response_handler config, esp_http_client_handle_t client) {
    int64_t resp_length = esp_http_client_fetch_headers(client);
    ESP_LOGI(TAG, "response body size %jd", resp_length);

    http_client_json_response response = JSON_RESPONSE_NULL();

    // check if response is of the expected size
    if (resp_length > config.size) {
        ESP_LOGE(TAG, "response body is too large [ %jd ] expected [ %d ]", resp_length, config.size);
        esp_http_client_cleanup(client);
        return response;
    }

    int status_code = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG, "response status code: %d", status_code);

    if (resp_length > 0) {
        char response_buffer[resp_length + 1];
        int bytes_received = esp_http_client_read_response(client, response_buffer, (int)resp_length);
        if (bytes_received <= 0) {
            ESP_LOGE(TAG, "failed to read response");
            // Handle error
        } else {
            response_buffer[bytes_received] = '\0';  // Null-terminate the response
            ESP_LOGI(TAG, "received response: %s", response_buffer);

            response.json = cJSON_Parse(response_buffer);
        }
    } else {
        ESP_LOGE(TAG, "failed to read response headers");
        // Handle error
    }

    response.http_status_code = status_code;

    return response;
}

void http_client_download_file(http_client_config config) {
    FILE* f = fopen(config.file_location.path, "wb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }

    esp_http_client_handle_t client = init_connection(config, 0);
    if (client == NULL) {
        return;
    }

    int64_t total_len = esp_http_client_fetch_headers(client);
    ESP_LOGI(TAG, "LEN %jd", total_len);

    // Dynamically allocate buffer so it can go to PSRAM
    size_t buffer_size = 2048;
    char* buffer = (char*)malloc(buffer_size);
    if (buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate buffer");
        fclose(f);
        esp_http_client_cleanup(client);
        return;
    }

    int read_len;
    int64_t total_read = 0;
    while ((read_len = esp_http_client_read(client, buffer, buffer_size)) > 0) {
        fwrite(buffer, 1, read_len, f);

        if (config.enable_read_logs) {
            total_read += read_len;
            ESP_LOGI(TAG, "downloaded bytes %jd", total_read);
        }
    }

    // Free the buffer after use
    free(buffer);
    fclose(f);
    esp_http_client_cleanup(client);

    ESP_LOGI(TAG, "Downloaded %jd bytes to %s", total_len, config.file_location.path);
}

http_client_json_response http_client_request(http_client_config config) {
    http_client_json_response response = JSON_RESPONSE_NULL();
    esp_http_client_handle_t client = init_connection(config, 0);
    if (client == NULL) {
        return response;
    }

    http_client_json_response r = JSON_RESPONSE_NULL();
    if (config.response_handler.type == JSON) {
        r = read_json_response(config.response_handler, client);
    }

    // Clean up
    esp_http_client_cleanup(client);

    return r;
}

http_client_json_response http_client_upload_file(http_client_config config) {
    http_client_json_response response = JSON_RESPONSE_NULL();

    // open file for read
    ESP_LOGI(TAG, "Opening file [%s]", config.file_location.path);

    FILE* file = fopen(config.file_location.path, "rb");
    if (file == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return response;
    }

    ESP_LOGI(TAG, "Init http connection [%s]", config.url);

    // calculate file size
    fseek(file, 0, SEEK_END);
    long content_length = ftell(file);
    fseek(file, 0, SEEK_SET);

    ESP_LOGI(TAG, "upload size: %ld", content_length);

    // Open the HTTP connection
    esp_http_client_handle_t client = init_connection(config, (int)content_length);
    if (client == NULL) {
        return response;
    }

    // Read and send the file
    char buffer[1024];
    size_t bytes_read;
    int counter = 0;
    esp_err_t http_ret = 0;

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        http_ret = esp_http_client_write(client, buffer, bytes_read);
        if (http_ret < 0) {
            ESP_LOGE(TAG, "Failed to send chunk: %s", esp_err_to_name(http_ret));
            // Handle error
            break;
        } else {
            counter += bytes_read;
            ESP_LOGI(TAG, "uploaded: %d", counter);
        }
    }

    // close file as no longer needed
    fclose(file);

    if (http_ret < 0) {
        ESP_LOGE(TAG, "file upload failed: %d", http_ret);

        esp_http_client_cleanup(client);

        return response;
    }

    ESP_LOGI(TAG, "uploaded done: %d bytes left to upload", (counter - (int)content_length));

    // read response if one is expected
    if (config.response_handler.type == JSON) {
        response = read_json_response(config.response_handler, client);
    }

    // Clean up
    esp_http_client_cleanup(client);

    return response;
}
