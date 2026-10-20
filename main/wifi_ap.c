#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "includes/wifi_ap.h"
#include "includes/client_registry.h"
#include "includes/network_config.h"

#define AP_STARTED_BIT BIT0
static EventGroupHandle_t s_wifi_events;

static const char *TAG = "WIFI_AP";

// does different things based on the event type, only for WIFI_EVENT events
static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base != WIFI_EVENT) return; // ignore non-WIFI_EVENT events
    switch (id) {
    case WIFI_EVENT_AP_START: // AP is up
        xEventGroupSetBits(s_wifi_events, AP_STARTED_BIT);   // wakes wifi_init
        break;
    case WIFI_EVENT_AP_STACONNECTED: { // a station has connected to the AP
        wifi_event_ap_staconnected_t *e = data;
        client_registry_on_connect(e->mac);
        break; }
    case WIFI_EVENT_AP_STADISCONNECTED: { // a station has disconnected from the AP
        wifi_event_ap_stadisconnected_t *e = data;
        client_registry_on_disconnect(e->mac);
        break; }
    }
}

// fires when the AP's DHCP server hands an IP to a station
static void ip_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base != IP_EVENT || id != IP_EVENT_ASSIGNED_IP_TO_CLIENT) return;
    ip_event_assigned_ip_to_client_t *e = data;
    client_registry_on_ip_assigned(e->mac, e->ip, e->hostname);
}

esp_err_t wifi_init(void)
{
    // get the wifi started and wait for it to be ready. This function is blocking
    s_wifi_events = xEventGroupCreate();      // create event group for wifi events

    ESP_ERROR_CHECK(esp_netif_init()); // initialize TCP/IP stack
    ESP_ERROR_CHECK(esp_event_loop_create_default()); // create default event loop
    esp_netif_create_default_wifi_ap(); // create default AP network interface (starts DHCP server)

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT(); // initialize Wi-Fi with default configuration
    ESP_ERROR_CHECK(esp_wifi_init(&cfg)); // initialize Wi-Fi driver

    // register event handler for all WIFI_EVENT events
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    // register event handler so we learn each station's IP once DHCP assigns it
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_ASSIGNED_IP_TO_CLIENT, &ip_event_handler, NULL, NULL));

    // configure AP settings
    wifi_config_t ap_cfg = { .ap = {
        .ssid = AP_SSID, .ssid_len = strlen(AP_SSID), .channel = AP_CHANNEL,
        .password = AP_PASSWORD, .max_connection = AP_MAX_CLIENTS,
        .authmode = WIFI_AUTH_WPA2_PSK } };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP)); // set Wi-Fi mode to AP
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg)); // set AP configuration
    ESP_ERROR_CHECK(esp_wifi_start()); // start Wi-Fi

    EventBits_t bits = xEventGroupWaitBits(s_wifi_events, AP_STARTED_BIT,
                                           pdFALSE,   // don't clear on exit
                                           pdTRUE,    // wait for all bits
                                           pdMS_TO_TICKS(5000)); // wait for AP to start or timeout

    if (bits & AP_STARTED_BIT) { // if AP started successfully
        ESP_LOGI(TAG, "AP \"%s\" up (max %d clients); safe to start servers/tasks",
                 AP_SSID, AP_MAX_CLIENTS);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "AP failed to start within 5 s");
        return ESP_ERR_TIMEOUT;
    }
}
