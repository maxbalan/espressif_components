idf_component_register(SRCS "src/SrHelper.c"    
                    INCLUDE_DIRS "include"
                    REQUIRES esp-sr esp_event driver synchroniser fatfs esp_timer
)