#include "esp_http_client.h"
#include "cJSON.h"
#include "esp_log.h"

typedef struct http_client_download_config {
    const char *filePath;
} http_client_download_config;

typedef struct http_client_config {
    const char *url;
    esp_http_client_method_t method;
    http_client_download_config download_config;
} http_client_config;

void http_client_download_file(http_client_config config);

cJSON* http_client_read_json_body(http_client_config config);
