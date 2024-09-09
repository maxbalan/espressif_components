set(requires
     esp-sr
     driver
     esp_event
     wifi_helper
     http_helper
     sdcard_helper
     sr_helper
     mqtt
    )


idf_component_register(SRCS "src/main.c"
                    INCLUDE_DIRS "include"
                    REQUIRES ${requires}
)
