#include "HttpHelper.h"

#ifndef DEBUG_ENABLED
#define DEBUG_ENABLED
#endif

#ifdef DEBUG_ENABLED
static const char* TAG = "Http Client >>> ";
#endif

// private methods
esp_http_client_handle_t init_connection(http_client_config config) {
    esp_http_client_config_t clientConfig = {
        .url = config.url,
        .method = config.method,
    };
    esp_http_client_handle_t client = esp_http_client_init(&clientConfig);

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        //    xEventGroupSetBits(http_download_group, HTTP_DOWNLOAD_FAIL);
        return NULL;
    }

    return client;
}

http_client_json_response read_json_response(response_handler config, esp_http_client_handle_t client) {
    int64_t total_len = esp_http_client_fetch_headers(client);
    ESP_LOGI(TAG, "response body size %jd", total_len);

    http_client_json_response response = JSON_RESPONSE_NULL();

    // check if response is of the expected size
    if (total_len > config.size) {
        ESP_LOGE(TAG, "response body is too large [ %jd ] expected [ %d ]", total_len, config.size);
        esp_http_client_cleanup(client);
        return response;
    }

    int status_code = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG, "response status code: %d", status_code);

    // read json
    char buffer[config.size];
    esp_http_client_read(client, buffer, sizeof(buffer));

    // Read the response from the server
    int64_t resp_length = esp_http_client_fetch_headers(client);

    if (resp_length > 0) {
        char response_buffer[resp_length + 1];
        int bytes_received = esp_http_client_read_response(client, response_buffer, (int)resp_length);
        if (bytes_received <= 0) {
            ESP_LOGE(TAG, "Failed to read response");
            // Handle error
        } else {
            response_buffer[bytes_received] = '\0';  // Null-terminate the response
            ESP_LOGI(TAG, "Received response: %s", response_buffer);

            response.json = cJSON_Parse(buffer);
        }
    } else {
        ESP_LOGE(TAG, "Failed to read response headers");
        // Handle error
    }

    response.http_status_code = status_code;

    return response;
}

void http_client_download_file(http_client_config config) {
    FILE* f = fopen(config.file_location.path, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }

    esp_http_client_handle_t client = init_connection(config);
    if (client == NULL) {
        return;
    }

    int64_t total_len = esp_http_client_fetch_headers(client);
    ESP_LOGI(TAG, "LEN %jd", total_len);

    char buffer[2048];
    int read_len;

    while ((read_len = esp_http_client_read(client, buffer, sizeof(buffer))) > 0) {
        fwrite(buffer, 1, read_len, f);
        // ESP_LOGI(TAG, "Remaining %d", remaining);
    }

    fclose(f);
    esp_http_client_cleanup(client);

    ESP_LOGI(TAG, "Downloaded %d bytes to %s", (int)total_len, config.file_location.path);
}

http_client_json_response http_client_request(http_client_config config) {
    http_client_json_response response = JSON_RESPONSE_NULL();
    esp_http_client_handle_t client = init_connection(config);
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
    FILE* file = fopen(config.file_location.path, "rb");
    if (file == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return response;
    }

    esp_http_client_handle_t client = init_connection(config);
    if (client == NULL) {
        return response;
    }

    // calculate file size
    fseek(file, 0, SEEK_END);
    long content_length = ftell(file);
    fseek(file, 0, SEEK_SET);

    ESP_LOGI(TAG, "upload size: %ld", content_length);

    // Open the HTTP connection
    esp_err_t http_ret = esp_http_client_open(client, (int)content_length);
    if (http_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(http_ret));
        fclose(file);
        esp_http_client_cleanup(client);
        // Handle error
        return response;
    }

    // Read and send the file
    char buffer[1024];
    size_t bytes_read;
    int counter = 0;
    http_ret = 0;
    
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        http_ret = esp_http_client_write(client, buffer, bytes_read);
        if (http_ret == -1) {
            ESP_LOGE(TAG, "Failed to send chunk: %s", esp_err_to_name(http_ret));
            // Handle error
            break;
        } else {
            counter += bytes_read;
            ESP_LOGE(TAG, "uploaded: %d", counter);
        }
    }

    //close file as no longer needed
    fclose(file);

    if (http_ret != 0) {
        ESP_LOGE(TAG, "file upload failed: %d", http_ret);
        
        esp_http_client_cleanup(client);

        return response;
    }

    ESP_LOGI(TAG, "uploaded Done: %d", (counter - (int)content_length));

    // read response if one is expected
    if (config.response_handler.type == JSON) {
        response = read_json_response(config.response_handler, client);
    }

    // Clean up
    esp_http_client_cleanup(client);

    return response;
}
