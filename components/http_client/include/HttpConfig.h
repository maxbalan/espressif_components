#include "cJSON.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_mac.h"

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
    const char* url;
    esp_http_client_method_t method;
    file_location file_location;
    response_handler response_handler;
} http_client_config;

typedef struct {
    int http_status_code;
    cJSON* json;
} http_client_json_response;

#define JSON_RESPONSE_NULL(VOID) { \
        .http_status_code = 0, \
        .json = NULL \
    }

#define HTTP_CLIENT_CONFIG_DEFAULT(VOID) { \
        .url = NULL, \
        .method = HTTP_METHOD_GET, \
        .file_location = NULL, \
        .response_handler = { \
            .type = NONE, \
            .size = 1024, \
        } \
    }
