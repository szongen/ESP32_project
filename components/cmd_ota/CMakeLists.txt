idf_component_register(SRCS "simple_ota.c" "cmd_ota.c" 
                    INCLUDE_DIRS .
                    REQUIRES console spi_flash app_update esp_https_ota)
