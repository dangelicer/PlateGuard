#ifndef WIFI_AP_H
#define WIFI_AP_H

#include "esp_err.h"

// Starts the soft-AP (and its DHCP server) and blocks until it is up or 5 s
// pass. Connecting/leaving stations are recorded in the client registry, so
// client_registry_init() must be called first.
esp_err_t wifi_init(void);

#endif // WIFI_AP_H
