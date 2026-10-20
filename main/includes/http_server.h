#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "esp_err.h"
#include "esp_http_server.h"

// The ESP32's own HTTP server (port ESP_HTTP_PORT on 192.168.4.1):
//   POST /api/detections   Pi sends {"plate":"..."}
//                          -> 202 {"status":"queued","plate":"..."}
//   GET  /api/status       -> 200 {"uptime_ms":..,"clients":[..],"recent_detections":[..]}
// Errors are JSON too: {"status":"error","error":"..."}.
esp_err_t http_server_start(void);

// Adds another route (e.g. the webpage) after http_server_start().
esp_err_t http_server_register(const httpd_uri_t *uri);

#endif // HTTP_SERVER_H
