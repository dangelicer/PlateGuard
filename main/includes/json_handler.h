#ifndef JSON_HANDLER_H
#define JSON_HANDLER_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "client_registry.h"
#include "detection_pipeline.h"

// Parses a detection from the Pi, e.g. {"plate":"TXA1234"}, copying the plate
// into plate_out. Other fields are ignored. Returns false if the buffer isn't
// valid JSON, "plate" is missing/not a string/empty, or the plate doesn't fit
// in plate_out.
bool parse_detection_json(const char *json_buf, char *plate_out, size_t plate_out_size);

// Builds the request body sent to the Flask /api/check_plate endpoint, e.g.
// {"plate":"TXA1234"}. Returns a heap string the caller must release with
// cJSON_free(), or NULL on allocation failure.
char *build_plate_check_json(const char *plate);

// Parses the Flask /api/check_plate response, e.g.
// {"plate":"TXA1234","authorized":true,"status":"AUTHORIZED","attempts":3,...},
// reading only "authorized". Returns false if the buffer isn't valid JSON or
// "authorized" is missing/not a boolean; callers must then treat the plate as
// not authorized.
bool parse_plate_check_response(const char *json_buf, bool *authorized_out);

// Builds the ESP's reply to POST /api/detections, e.g.
// {"status":"queued","plate":"TXA1234"} or {"status":"error","error":"..."}.
// plate and error may be NULL to leave them out. Release with cJSON_free().
char *build_detection_ack_json(const char *status, const char *plate, const char *error);

// Builds the ESP's reply to GET /api/status: uptime, connected clients and
// recent detections. Release with cJSON_free(); NULL on allocation failure.
char *build_status_json(int64_t uptime_ms,
                        const client_info_t *clients, size_t n_clients,
                        const detection_record_t *recent, size_t n_recent);

#endif // JSON_HANDLER_H
