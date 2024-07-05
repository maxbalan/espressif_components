idf_component_register(SRCS "main.c" "../components/http_client/HttpHelper.c"
                    INCLUDE_DIRS "." "../components/http_client/include"
                    REQUIRES esp_http_client json
)
