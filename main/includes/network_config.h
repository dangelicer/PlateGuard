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

// ---- Dashboard website (web_proxy.c) -------------------------------------------
// A laptop on this Wi-Fi opens http://192.168.4.1/ ; the ESP forwards every
// /dashboard/... request to the database server and relays the answer back in
// WEB_PROXY_CHUNK_SIZE pieces. The Flask app serves the site under this prefix.
#define WEB_PROXY_PREFIX        "/dashboard"
#define WEB_PROXY_CHUNK_SIZE    1024    // bytes of a page held in RAM at once
// The HTTP server handles one request at a time, so a slow database server
// also delays the Pi's detections; keep this short.
#define WEB_PROXY_TIMEOUT_MS    3000

// ---- Known devices -------------------------------------------------------------
// Roles come from each device's Wi-Fi MAC address, so they don't depend on join
// order, hostname or IP. IPs are whatever DHCP hands out; the ESP looks each
// device's current IP up by MAC when it needs it. Any other device is "unknown"
// (e.g. a laptop that will just view the webpage).
//
// The device's MAC must not be randomized on this network (Windows: Wi-Fi >
// my-esp > "Random hardware addresses" off; Raspberry Pi OS doesn't randomize
// by default).
#define DETECTOR_NAME           "IanSafeGuard"      // Raspberry Pi 4 with the camera
#define DETECTOR_MAC            { 0x98, 0xfe, 0x54, 0x36, 0x73, 0xa7 }

#define DB_SERVER_NAME          "Daniels-Laptop"    // runs the Flask backend
#define DB_SERVER_MAC           { 0xbc, 0x03, 0x58, 0x02, 0x71, 0xe1 }

#endif // NETWORK_CONFIG_H
