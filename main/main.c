#include "includes/main.h"
#include "includes/test_json_handler.h"
#include "includes/test_http.h"
#include "includes/client_registry.h"
#include "includes/wifi_ap.h"
#include "includes/detection_pipeline.h"
#include "includes/http_server.h"
#include "includes/servo_gate.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"

#if !TEST_MODE
static const char *TAG = "MAIN";
#endif

void app_main(void)
{
#if TEST_MODE

    // Run existing JSON tests when TEST_MODE is enabled.
    test_json_parse();

#else

    // =====================================================
    // STEP 1: INITIALIZE FLASH STORAGE
    // =====================================================

    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {

        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);


    // =====================================================
    // STEP 2: INITIALIZE CLIENT REGISTRY
    // =====================================================

    client_registry_init();


    // =====================================================
    // STEP 3: INITIALIZE SG90 SERVO
    // =====================================================

    ESP_LOGI(TAG, "Initializing SG90 servo");

    ESP_ERROR_CHECK(servo_gate_init());


    // =====================================================
    // STEP 4: TEST SERVO MOVEMENT
    // =====================================================

    ESP_LOGI(TAG, "Starting SG90 servo test");

    // Wait 1 second before triggering the gate.
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Request one gate-opening cycle.
    esp_err_t gate_err = servo_gate_trigger();

    if (gate_err != ESP_OK) {

        ESP_LOGW(TAG,
                 "Unable to queue gate request: %s",
                 esp_err_to_name(gate_err));

    } else {

        ESP_LOGI(TAG,
                 "Servo gate test request queued successfully");
    }


    // =====================================================
    // STEP 5: INITIALIZE WI-FI
    // =====================================================

    esp_err_t wifi_ret = wifi_init();

    if (wifi_ret != ESP_OK) {

        ESP_LOGE(TAG, "Failed to initialize Wi-Fi");
        return;
    }


    // =====================================================
    // STEP 6: START DETECTION PIPELINE
    // =====================================================

    ESP_ERROR_CHECK(detection_pipeline_start());


    // =====================================================
    // STEP 7: START HTTP SERVER
    // =====================================================

    ESP_ERROR_CHECK(http_server_start());


    // =====================================================
    // STEP 8: OPTIONAL HTTP TEST
    // =====================================================

#if HTTP_TEST_MODE

    xTaskCreate(http_test_task,
                "http_test",
                8192,
                NULL,
                5,
                NULL);

#endif

#endif
}