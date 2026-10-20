#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "includes/db_client.h"
#include "includes/client_registry.h"
#include "includes/json_handler.h"
#include "includes/network_config.h"

#define HTTP_TIMEOUT_MS         5000
#define HTTP_RESP_BUF_SIZE      256

static const char *TAG = "DB_CLIENT";

// POSTs a JSON body to the given URL and copies the response body into
// resp_buf (always NUL-terminated). Returns ESP_OK only on an HTTP 200.
static esp_err_t http_post_json(const char *url, const char *body, char *resp_buf, size_t resp_buf_size)
{
    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = HTTP_TIMEOUT_MS,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client == NULL) {
        return ESP_FAIL;
    }
    esp_http_client_set_header(client, "Content-Type", "application/json");

    int body_len = strlen(body);
    esp_err_t err = esp_http_client_open(client, body_len); // connect and send headers
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to connect to %s: %s", url, esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    if (esp_http_client_write(client, body, body_len) != body_len) { // send JSON body
        ESP_LOGE(TAG, "failed to send request body");
        err = ESP_FAIL;
    } else if (esp_http_client_fetch_headers(client) < 0) { // wait for response headers
        ESP_LOGE(TAG, "failed to read response headers");
        err = ESP_FAIL;
    } else {
        int len = esp_http_client_read_response(client, resp_buf, resp_buf_size - 1);
        resp_buf[len > 0 ? len : 0] = '\0';
        int status = esp_http_client_get_status_code(client);
        if (status != 200) {
            ESP_LOGE(TAG, "server returned HTTP %d: %s", status, resp_buf);
            err = ESP_FAIL;
        }
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return err;
}

esp_err_t post_plate_check(const char *plate, char *resp_buf, size_t resp_buf_size)
{
    client_info_t server;
    if (!client_registry_find_role(CLIENT_ROLE_DB_SERVER, &server)) {
        return ESP_ERR_INVALID_STATE; // database server not connected
    }

    char url[64];
    snprintf(url, sizeof(url), "http://" IPSTR ":%d" DB_CHECK_PLATE_PATH,
             IP2STR(&server.ip), DB_SERVER_PORT);

    char *body = build_plate_check_json(plate);
    if (body == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = http_post_json(url, body, resp_buf, resp_buf_size);
    cJSON_free(body);
    return err;
}

esp_err_t check_plate_with_server(const char *plate, bool *authorized_out)
{
    char resp[HTTP_RESP_BUF_SIZE];
    esp_err_t err = post_plate_check(plate, resp, sizeof(resp));
    if (err != ESP_OK) {
        return err;
    }

    if (!parse_plate_check_response(resp, authorized_out)) {
        ESP_LOGE(TAG, "unexpected response from server: %s", resp);
        return ESP_ERR_INVALID_RESPONSE;
    }
    return ESP_OK;
}
