idf_component_register(SRCS "src/MqttHelper.c"    
                    INCLUDE_DIRS "include"
                    REQUIRES esp_event mqtt
)