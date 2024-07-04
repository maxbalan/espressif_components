#include "HttpHelper.h"

#ifndef DEBUG_ENABLED
#define DEBUG_ENABLED
#endif

#ifdef DEBUG_ENABLED
static const char *TAG = "Http Client >>> ";
#endif

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

void http_client_download_file(http_client_config config) {
    FILE* f = fopen(config.download_config.filePath, "w");
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

    ESP_LOGI(TAG, "Downloaded %d bytes to %s", (int)total_len, config.download_config.filePath);
}

cJSON* http_client_read_json_body(http_client_config config) {
   esp_http_client_handle_t client = init_connection(config);
    if (client == NULL) {
        return NULL;
    }

    int64_t total_len = esp_http_client_fetch_headers(client);
    ESP_LOGI(TAG, "LEN %jd", total_len);

    if (total_len > 1024) {
        ESP_LOGE(TAG, "response is too large [ %jd ]", total_len);
        esp_http_client_cleanup(client);
     //    xEventGroupSetBits(http_download_group, HTTP_DOWNLOAD_FAIL);
        return NULL;
    }

    // read json 
    char buffer[1024];
    esp_http_client_read(client, buffer, sizeof(buffer));

    return cJSON_Parse(buffer);
}