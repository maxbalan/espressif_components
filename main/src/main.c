
#include "esp_event.h"
#include "esp_mac.h"
#include "esp_log.h"


ESP_EVENT_DECLARE_BASE(TTT);

ESP_EVENT_DEFINE_BASE(TTT);
static const char *TAG = "MAIN >>> ";

static void sr_callback(void *arg,
                        esp_event_base_t event_base,
                        int32_t event,
                        void *event_data) {

        ESP_LOGI(TAG, "sr is event recevived");
}

void app_main(void) {
    ESP_ERROR_CHECK(esp_event_handler_instance_register(TTT,
                                                        ESP_EVENT_ANY_ID,
                                                        &sr_callback,
                                                        NULL,
                                                        NULL));

         esp_event_post(
        TTT,      // Event base
        1,         // Event ID
        NULL,          // Event data (if any)
        0,             // Event data size
        portMAX_DELAY  // Max delay to post the event
    );
}