#ifndef CLIENT_REGISTRY_H
#define CLIENT_REGISTRY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_netif_ip_addr.h"

// What a connected station is used for. See network_config.h for how roles
// are assigned.
typedef enum {
    CLIENT_ROLE_UNKNOWN = 0,
    CLIENT_ROLE_DETECTOR,    // Raspberry Pi: sends plate detections to us
    CLIENT_ROLE_DB_SERVER,   // runs the Flask app with the plate database
    CLIENT_ROLE_VIEWER,      // loads the status webpage
} client_role_t;

typedef struct {
    uint8_t mac[6];
    esp_ip4_addr_t ip;       // 0.0.0.0 until DHCP assigns one
    char hostname[32];       // from DHCP option 12, may be empty
    client_role_t role;
} client_info_t;

// Must be called before Wi-Fi starts. All other functions are thread-safe.
void client_registry_init(void);

// Called from the Wi-Fi/IP event handlers.
void client_registry_on_connect(const uint8_t mac[6]);
void client_registry_on_ip_assigned(const uint8_t mac[6], esp_ip4_addr_t ip, const char *hostname);
void client_registry_on_disconnect(const uint8_t mac[6]);

// Copies the first connected client with this role and an IP into *out.
// Returns false if there is none.
bool client_registry_find_role(client_role_t role, client_info_t *out);

// Gives the client with this IP a role it has proven by its behaviour (e.g.
// it sent us a detection). Only upgrades UNKNOWN/VIEWER clients, so a
// hostname-assigned role is never overwritten.
void client_registry_set_role_by_ip(esp_ip4_addr_t ip, client_role_t role);

// Copies up to max connected clients into out. Returns how many were copied.
size_t client_registry_snapshot(client_info_t *out, size_t max);

const char *client_role_name(client_role_t role);

#endif // CLIENT_REGISTRY_H
