#include "cJSON.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "HttpConfig.h"

http_client_json_response http_client_upload_file(http_client_config config);

void http_client_download_file(http_client_config config);

http_client_json_response http_client_request(http_client_config config);

