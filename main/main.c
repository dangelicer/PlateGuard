#include "includes/main.h"
#include "includes/test_json_handler.h"
#include "includes/test_http.h"
#include "includes/client_registry.h"
#include "includes/wifi_ap.h"
#include "includes/detection_pipeline.h"
#include "includes/http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"

#if !TEST_MODE
static const char *TAG = "MAIN";
#endif

void app_main(void)
{
#if TEST_MODE
    test_json_parse();
#else
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {  //if prev flash ended in a boot loop this should fix it
        ESP_ERROR_CHECK(nvs_flash_erase());   // partition is full or from an older IDF; wipe and retry
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    client_registry_init(); // must exist before Wi-Fi events start arriving

    esp_err_t wifi_ret = wifi_init();
    if (wifi_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize Wi-Fi");
        return;
    }

    ESP_ERROR_CHECK(detection_pipeline_start()); // checks queued detections with the database server
    ESP_ERROR_CHECK(http_server_start());        // receives detections from the Pi

#if HTTP_TEST_MODE
    xTaskCreate(http_test_task, "http_test", 8192, NULL, 5, NULL);
#endif
#endif
}
