idf_component_register(
    SRCS "wifi_prov.c" "device_settings.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_wifi
        esp_event
        esp_netif
        esp_http_server
        nvs_flash
        esp_driver_gpio
        log
        freertos
    PRIV_REQUIRES
        nordesems__esp-captive-portal
)
