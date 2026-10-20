#ifndef NETWORK_CONFIG_H
#define NETWORK_CONFIG_H

// ---- Soft-AP ---------------------------------------------------------------
#define AP_SSID             "my-esp"
#define AP_PASSWORD         "password123"
#define AP_CHANNEL          6
// ESP32 soft-AP limit: 15 stations minus the key slots reserved for ESP-NOW
// (CONFIG_ESP_WIFI_ESPNOW_MAX_ENCRYPT_NUM=7) leaves 10. Keep
// CONFIG_LWIP_DHCPS_MAX_STATION_NUM >= this in sdkconfig.
#define AP_MAX_CLIENTS      10

// ---- ESP32's own HTTP server -------------------------------------------------
// The ESP is always 192.168.4.1 on its own AP, so clients can hard-code it:
//   Pi:      POST http://192.168.4.1/api/detections
//   Anyone:  GET  http://192.168.4.1/api/status
#define ESP_HTTP_PORT       80

// ---- Database server (Flask app) --------------------------------------------
#define DB_SERVER_PORT          5000
#define DB_CHECK_PLATE_PATH     "/api/check_plate"

// ---- Role assignment ----------------------------------------------------------
// Devices are given a role from the hostname they send in their DHCP request
// (case-insensitive prefix match). Anything that doesn't match is "unknown"
// (e.g. a laptop that will just view the webpage). A device that POSTs to
// /api/detections is also marked as the detector regardless of hostname.
#define DB_SERVER_HOSTNAME      "Daniels-Laptop"
#define DETECTOR_HOSTNAME       "raspberrypi"

#endif // NETWORK_CONFIG_H
