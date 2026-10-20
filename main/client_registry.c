#include <string.h>
#include <strings.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "includes/client_registry.h"
#include "includes/network_config.h"

static const char *TAG = "REGISTRY";

typedef struct {
    bool in_use;
    client_info_t info;
} slot_t;

typedef struct {
    const char *hostname_prefix;
    client_role_t role;
} role_rule_t;

static const role_rule_t s_role_rules[] = {
    { DB_SERVER_HOSTNAME, CLIENT_ROLE_DB_SERVER },
    { DETECTOR_HOSTNAME,  CLIENT_ROLE_DETECTOR },
};

static slot_t s_slots[AP_MAX_CLIENTS];
static SemaphoreHandle_t s_lock;

static client_role_t role_from_hostname(const char *hostname)
{
    for (size_t i = 0; i < sizeof(s_role_rules) / sizeof(s_role_rules[0]); i++) {
        const char *prefix = s_role_rules[i].hostname_prefix;
        if (strncasecmp(hostname, prefix, strlen(prefix)) == 0) {
            return s_role_rules[i].role;
        }
    }
    return CLIENT_ROLE_UNKNOWN;
}

// caller must hold s_lock
static slot_t *find_by_mac(const uint8_t mac[6])
{
    for (size_t i = 0; i < AP_MAX_CLIENTS; i++) {
        if (s_slots[i].in_use && memcmp(s_slots[i].info.mac, mac, 6) == 0) {
            return &s_slots[i];
        }
    }
    return NULL;
}

// caller must hold s_lock; returns the existing slot for this MAC or a fresh one
static slot_t *find_or_alloc(const uint8_t mac[6])
{
    slot_t *slot = find_by_mac(mac);
    if (slot != NULL) {
        return slot;
    }
    for (size_t i = 0; i < AP_MAX_CLIENTS; i++) {
        if (!s_slots[i].in_use) {
            memset(&s_slots[i], 0, sizeof(s_slots[i]));
            s_slots[i].in_use = true;
            memcpy(s_slots[i].info.mac, mac, 6);
            return &s_slots[i];
        }
    }
    return NULL; // can't happen while AP_MAX_CLIENTS matches the AP config
}

void client_registry_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    memset(s_slots, 0, sizeof(s_slots));
}

void client_registry_on_connect(const uint8_t mac[6])
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    slot_t *slot = find_or_alloc(mac);
    if (slot != NULL) {
        slot->info.ip.addr = 0;   // a reconnecting station gets a fresh DHCP lease
    }
    xSemaphoreGive(s_lock);

    if (slot == NULL) {
        ESP_LOGE(TAG, MACSTR " joined but the registry is full", MAC2STR(mac));
    } else {
        ESP_LOGI(TAG, MACSTR " joined", MAC2STR(mac));
    }
}

void client_registry_on_ip_assigned(const uint8_t mac[6], esp_ip4_addr_t ip, const char *hostname)
{
    client_info_t copy;
    bool ok = false;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    slot_t *slot = find_or_alloc(mac);
    if (slot != NULL) {
        slot->info.ip = ip;
        strncpy(slot->info.hostname, hostname ? hostname : "", sizeof(slot->info.hostname) - 1);
        slot->info.hostname[sizeof(slot->info.hostname) - 1] = '\0';
        slot->info.role = role_from_hostname(slot->info.hostname);
        copy = slot->info;
        ok = true;
    }
    xSemaphoreGive(s_lock);

    if (ok) {
        ESP_LOGI(TAG, MACSTR " \"%s\" got IP " IPSTR " -> role: %s",
                 MAC2STR(mac), copy.hostname, IP2STR(&copy.ip), client_role_name(copy.role));
    }
}

void client_registry_on_disconnect(const uint8_t mac[6])
{
    client_role_t role = CLIENT_ROLE_UNKNOWN;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    slot_t *slot = find_by_mac(mac);
    if (slot != NULL) {
        role = slot->info.role;
        slot->in_use = false;
    }
    xSemaphoreGive(s_lock);

    ESP_LOGI(TAG, MACSTR " left (role: %s)", MAC2STR(mac), client_role_name(role));
}

bool client_registry_find_role(client_role_t role, client_info_t *out)
{
    bool found = false;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    for (size_t i = 0; i < AP_MAX_CLIENTS && !found; i++) {
        if (s_slots[i].in_use && s_slots[i].info.role == role && s_slots[i].info.ip.addr != 0) {
            *out = s_slots[i].info;
            found = true;
        }
    }
    xSemaphoreGive(s_lock);
    return found;
}

void client_registry_set_role_by_ip(esp_ip4_addr_t ip, client_role_t role)
{
    if (ip.addr == 0) {
        return;
    }

    bool changed = false;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    for (size_t i = 0; i < AP_MAX_CLIENTS; i++) {
        client_info_t *c = &s_slots[i].info;
        if (s_slots[i].in_use && c->ip.addr == ip.addr &&
            (c->role == CLIENT_ROLE_UNKNOWN || c->role == CLIENT_ROLE_VIEWER) && c->role != role) {
            c->role = role;
            changed = true;
        }
    }
    xSemaphoreGive(s_lock);

    if (changed) {
        ESP_LOGI(TAG, IPSTR " is now role: %s", IP2STR(&ip), client_role_name(role));
    }
}

size_t client_registry_snapshot(client_info_t *out, size_t max)
{
    size_t n = 0;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    for (size_t i = 0; i < AP_MAX_CLIENTS && n < max; i++) {
        if (s_slots[i].in_use) {
            out[n++] = s_slots[i].info;
        }
    }
    xSemaphoreGive(s_lock);
    return n;
}

const char *client_role_name(client_role_t role)
{
    switch (role) {
    case CLIENT_ROLE_DETECTOR:  return "detector";
    case CLIENT_ROLE_DB_SERVER: return "db_server";
    case CLIENT_ROLE_VIEWER:    return "viewer";
    default:                    return "unknown";
    }
}
