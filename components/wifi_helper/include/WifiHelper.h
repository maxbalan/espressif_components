#include <esp_wifi.h>
#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "freertos/event_groups.h"
#include "lwip/err.h"

void connectWifi(wifi_config_t wifiConfig);