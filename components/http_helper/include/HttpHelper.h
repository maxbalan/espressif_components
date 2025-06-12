#pragma once

#include "cJSON.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_vfs_fat.h"

// Response type enumeration
typedef enum {
    NONE = 0x00,
    JSON = 0x01,
} http_response_type;

// Response handler configuration
typedef struct {
    http_response_type type;
    int size;
} response_handler;

// File upload configuration (unused in buffer mode)
typedef struct {
    const char* path;
} http_client_upload_file_t;

typedef struct {
    uint8_t* data_buffer;
    uint32_t data_buffer_size;
} http_client_upload_buffer_t;

// HTTP client configuration
typedef struct {
    bool enable_read_logs;
    const char* url;
    esp_http_client_method_t method;
    struct http_client_upload_t {
        http_client_upload_file_t file_config;
        http_client_upload_buffer_t buffer_config;
    } upload;
    struct http_client_download_t {
        http_client_upload_file_t file_config;
        http_client_upload_buffer_t buffer_config;
    } download;
    response_handler response_handler;
} http_client_config;

// HTTP client JSON response
typedef struct {
    int http_status_code;
    cJSON* json;
} http_client_json_response;

#define JSON_RESPONSE_NULL()   \
    {                          \
        .http_status_code = 0, \
        .json = NULL,          \
    }

#define HTTP_CLIENT_CONFIG_DEFAULT()       \
    {                                      \
        .enable_read_logs = false,         \
        .url = NULL,                       \
        .method = HTTP_METHOD_GET,         \
        .response_handler = {              \
            .type = NONE,                  \
            .size = 1024,                  \
        },                                 \
        .upload = {                        \
            .file_config = {.path = NULL}, \
            .buffer_config = {             \
                .data_buffer = NULL,       \
                .data_buffer_size = 0,     \
            },                             \
        },                                 \
        .download = {                      \
            .file_config = {               \
                .path = NULL},             \
            .buffer_config = {             \
                .data_buffer = NULL,       \
                .data_buffer_size = 0,     \
            },                             \
        }                                  \
    }

// events setup
typedef enum {
    FILE_UPLOAD_SUCCESS = 0,
    FILE_UPLOAD_FAIL,
} http_event_t;

http_client_json_response http_client_upload(http_client_config config);

void http_client_download_file(http_client_config config);

http_client_json_response http_client_request(http_client_config config);

void http_register_callback(esp_event_handler_t callback);
