#ifndef DB_CLIENT_H
#define DB_CLIENT_H

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

// HTTP client for the database server (the Flask app on whichever connected
// device has the db_server role in the client registry).

// Asks the database server whether a plate is authorized (the server also
// counts the lookup). *authorized_out is only valid when this returns ESP_OK.
// Returns ESP_ERR_INVALID_STATE if no database server is connected.
esp_err_t check_plate_with_server(const char *plate, bool *authorized_out);

// Lower-level version of check_plate_with_server(): sends the plate and copies
// the server's raw response body into resp_buf without interpreting it.
// Returns ESP_OK only on an HTTP 200, but resp_buf is filled for any status.
esp_err_t post_plate_check(const char *plate, char *resp_buf, size_t resp_buf_size);

#endif // DB_CLIENT_H
