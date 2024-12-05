#include "MqttHelper.h"

static const char* TAG = "MQTT >>>";

esp_mqtt_client_handle_t mqtt_client;
char* mqtt_helper_topic;


void mqtt_init(esp_mqtt_client_config_t mqtt_cfg, esp_event_handler_t callback) {
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    mqtt_register_callback(callback);
    esp_mqtt_client_start(mqtt_client);
}

void mqtt_subscribe(char* sub_topic) {
    esp_mqtt_client_subscribe(mqtt_client, sub_topic, 0);
}

void mqtt_register_callback(esp_event_handler_t callback) {
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, callback, NULL);
}

void mqtt_publish_topic(char* topic) {
    mqtt_helper_topic = strdup(topic);
    ESP_LOGI(TAG, "publish topic register: %s", mqtt_helper_topic);
}

void mqtt_publish(char* cmd) {
    esp_mqtt_client_publish(mqtt_client, mqtt_helper_topic, cmd, 0, 1, 0);
    free(cmd);
}