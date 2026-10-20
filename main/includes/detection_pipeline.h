#ifndef DETECTION_PIPELINE_H
#define DETECTION_PIPELINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_netif_ip_addr.h"

// Detections arrive from the HTTP server (sent by the Pi), are queued, and a
// worker task checks each plate with the database server. The last few
// results are kept for /api/status (and later, the webpage).

#define DETECTION_PLATE_LEN     32
#define DETECTION_RECENT_MAX    10

typedef struct {
    char plate[DETECTION_PLATE_LEN];
    esp_ip4_addr_t source_ip;   // who sent it (0.0.0.0 if unknown)
} detection_t;

typedef enum {
    DB_RESULT_PENDING = 0,      // queued, not checked yet
    DB_RESULT_AUTHORIZED,       // database says the plate is AUTHORIZED
    DB_RESULT_UNAUTHORIZED,     // database says the plate is UNAUTHORIZED
    DB_RESULT_NO_DB_SERVER,     // no database server connected
    DB_RESULT_DB_ERROR,         // database server connected but the request failed
} db_result_t;

typedef struct {
    detection_t detection;
    db_result_t result;
    int64_t received_ms;        // ms since boot
} detection_record_t;

// Creates the queue and worker task.
esp_err_t detection_pipeline_start(void);

// Queues a detection without blocking. Returns ESP_ERR_NO_MEM if the queue is full.
esp_err_t detection_pipeline_submit(const detection_t *detection);

// Copies up to max recent detections into out, newest first. Returns how many.
size_t detection_pipeline_recent(detection_record_t *out, size_t max);

// True only for DB_RESULT_AUTHORIZED: every other result, including errors
// and "no database server", must be treated as not authorized.
bool db_result_is_authorized(db_result_t result);

const char *db_result_name(db_result_t result);

#endif // DETECTION_PIPELINE_H
