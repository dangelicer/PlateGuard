#ifndef WEB_PROXY_H
#define WEB_PROXY_H

#include "esp_err.h"

// Serves the dashboard website to laptops on the ESP's Wi-Fi by forwarding
// their requests to the database server (Flask, which hosts the site under
// WEB_PROXY_PREFIX) and relaying each response back in small chunks, so the
// ESP never holds more than WEB_PROXY_CHUNK_SIZE bytes of a page at once.
//
//   GET /                -> 302 to /dashboard/
//   GET /dashboard       -> 302 to /dashboard/
//   GET /dashboard/...   -> GET http://<db server>:DB_SERVER_PORT/dashboard/...
//                           streamed back with chunked transfer encoding
//
// If no database server is connected, viewers get a 503 page instead.
// Call after http_server_start().
esp_err_t web_proxy_start(void);

#endif // WEB_PROXY_H
