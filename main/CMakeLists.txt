idf_component_register(SRCS "main.c" "json_handler.c" "test_json_handler.c" "test_http.c"
                         "wifi_ap.c" "client_registry.c" "db_client.c"
                         "http_server.c" "detection_pipeline.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES unity esp_wifi nvs_flash esp_event esp_netif esp_http_client
                                 esp_http_server esp_timer lwip)
