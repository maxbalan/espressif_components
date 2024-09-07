set(requires
     esp-sr
     driver
     wifi_helper
     http_helper
     sdcard_helper
     sr_helper
    )


idf_component_register(SRCS "src/main.c"
                    INCLUDE_DIRS "include"
                    REQUIRES ${requires}
)
