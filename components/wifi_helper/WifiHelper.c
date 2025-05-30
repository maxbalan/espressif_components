#include "WifiHelper.h"

#ifndef DEBUG_ENABLED
#define DEBUG_ENABLED
#endif

#ifdef DEBUG_ENABLED
static const char *TAG = "WiFi >>> ";
#endif

static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "Station started");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Disconnected from Wi-Fi, retrying...");
        esp_wifi_connect();
    } else {
        ESP_LOGW(TAG, "event unknown %ld", event_id);
    }
}

void connectWifi(wifi_config_t wifiConfig, const char *deviceHostname) {
    ESP_LOGI(TAG, "[%s] is connecting WIFI SSID:%s password:%s", deviceHostname, wifiConfig.sta.ssid, wifiConfig.sta.password);
    esp_event_loop_create_default();

    ESP_ERROR_CHECK(esp_netif_init());

    /* Initialize event group */
    s_wifi_event_group = xEventGroupCreate();

    /* Register Event handler */
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    //     init wifi
    esp_netif_create_default_wifi_sta();

    // set host name
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif) {
        esp_err_t err = esp_netif_set_hostname(netif, deviceHostname);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Hostname set to: %s", deviceHostname);
        } else {
            ESP_LOGE(TAG, "Failed to set hostname: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGE(TAG, "Failed to retrieve network interface for setting hostname");
    }

    // wifi init
    wifi_init_config_t wifiInitConfig = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifiInitConfig));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifiConfig));

    // connect WIFI
    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s", wifiConfig.sta.ssid, wifiConfig.sta.password);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s", wifiConfig.sta.ssid, wifiConfig.sta.password);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
        return;
    }
}