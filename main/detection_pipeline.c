#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "includes/detection_pipeline.h"
#include "includes/db_client.h"

#define DETECTION_QUEUE_LEN     8

static const char *TAG = "DETECTIONS";

static QueueHandle_t s_queue;

// ring buffer of recent results, newest at s_next - 1
static detection_record_t s_recent[DETECTION_RECENT_MAX];
static size_t s_recent_count;
static size_t s_next;
static SemaphoreHandle_t s_recent_lock;

static db_result_t check_with_db(const detection_t *d)
{
    bool authorized;
    esp_err_t err = check_plate_with_server(d->plate, &authorized);
    if (err == ESP_OK) {
        return authorized ? DB_RESULT_AUTHORIZED : DB_RESULT_UNAUTHORIZED;
    }
    return err == ESP_ERR_INVALID_STATE ? DB_RESULT_NO_DB_SERVER : DB_RESULT_DB_ERROR;
}

static void detection_task(void *arg)
{
    detection_record_t rec;
    while (1) {
        xQueueReceive(s_queue, &rec, portMAX_DELAY);

        rec.result = check_with_db(&rec.detection);
        if (db_result_is_authorized(rec.result)) {
            ESP_LOGI(TAG, "plate \"%s\" from " IPSTR " -> AUTHORIZED",
                     rec.detection.plate, IP2STR(&rec.detection.source_ip));
        } else {
            ESP_LOGW(TAG, "plate \"%s\" from " IPSTR " -> NOT authorized (%s)",
                     rec.detection.plate, IP2STR(&rec.detection.source_ip), db_result_name(rec.result));
        }

        // Future: act on the result here (open gate / raise alert, notify webpage
        // viewers, ...) using db_result_is_authorized(rec.result).

        xSemaphoreTake(s_recent_lock, portMAX_DELAY);
        s_recent[s_next] = rec;
        s_next = (s_next + 1) % DETECTION_RECENT_MAX;
        if (s_recent_count < DETECTION_RECENT_MAX) {
            s_recent_count++;
        }
        xSemaphoreGive(s_recent_lock);
    }
}

esp_err_t detection_pipeline_start(void)
{
    s_queue = xQueueCreate(DETECTION_QUEUE_LEN, sizeof(detection_record_t));
    s_recent_lock = xSemaphoreCreateMutex();
    if (s_queue == NULL || s_recent_lock == NULL) {
        return ESP_ERR_NO_MEM;
    }
    // 8 KB: the task makes HTTP requests to the database server
    if (xTaskCreate(detection_task, "detections", 8192, NULL, 5, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t detection_pipeline_submit(const detection_t *detection)
{
    detection_record_t rec = {
        .detection = *detection,
        .result = DB_RESULT_PENDING,
        .received_ms = esp_timer_get_time() / 1000,
    };
    return xQueueSend(s_queue, &rec, 0) == pdTRUE ? ESP_OK : ESP_ERR_NO_MEM;
}

size_t detection_pipeline_recent(detection_record_t *out, size_t max)
{
    size_t n = 0;

    xSemaphoreTake(s_recent_lock, portMAX_DELAY);
    for (; n < s_recent_count && n < max; n++) {
        size_t idx = (s_next + DETECTION_RECENT_MAX - 1 - n) % DETECTION_RECENT_MAX;
        out[n] = s_recent[idx];
    }
    xSemaphoreGive(s_recent_lock);
    return n;
}

bool db_result_is_authorized(db_result_t result)
{
    return result == DB_RESULT_AUTHORIZED;
}

const char *db_result_name(db_result_t result)
{
    switch (result) {
    case DB_RESULT_AUTHORIZED:   return "authorized";
    case DB_RESULT_UNAUTHORIZED: return "unauthorized";
    case DB_RESULT_NO_DB_SERVER: return "no_db_server";
    case DB_RESULT_DB_ERROR:     return "db_error";
    default:                     return "pending";
    }
}
