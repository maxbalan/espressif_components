#include "cJSON.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_mac.h"

#ifndef __http_helper_h__
#define __http_helper_h__

typedef enum {
    NONE = 0x00,
    JSON = 0x01,
} http_response_type;

typedef struct {
    http_response_type type;
    int size;
} response_handler;

typedef struct {
    const char* path;
} file_location;

typedef struct {
    bool enable_read_logs;
    const char* url;
    esp_http_client_method_t method;
    file_location file_location;
    response_handler response_handler;
} http_client_config;

typedef struct {
    int http_status_code;
    cJSON* json;
} http_client_json_response;

#define JSON_RESPONSE_NULL()   \
    {                          \
        .http_status_code = 0, \
        .json = NULL}

#define HTTP_CLIENT_CONFIG_DEFAULT()     \
    {                                    \
        .enable_read_logs = false,       \
        .url = NULL,                     \
        .method = HTTP_METHOD_GET,       \
        .response_handler = {            \
            .type = NONE,                \
            .size = 1024},               \
        .file_location = {.path = NULL}, \
    }

// events setup
typedef enum {
    FILE_UPLOAD_SUCCESS = 0,
    FILE_UPLOAD_FAIL,
} http_event_t;

#endif

http_client_json_response http_client_upload_file(http_client_config config);

void http_client_download_file(http_client_config config);

http_client_json_response http_client_request(http_client_config config);

void http_register_callback(esp_event_handler_t callback);
