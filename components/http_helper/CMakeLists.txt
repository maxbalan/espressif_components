set(srcs "HttpHelper.c")
set(include_dirs "include")

idf_component_register(SRCS ${srcs}      
                    INCLUDE_DIRS ${include_dirs}
                    REQUIRES esp_http_client json fatfs
)