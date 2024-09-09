#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mqtt_client.h"


void mqtt_init(esp_mqtt_client_config_t mqtt_cfg, esp_event_handler_t callback);

void mqtt_register_callback(esp_event_handler_t callback);

void mqtt_publish(char* topic, char* cmd);