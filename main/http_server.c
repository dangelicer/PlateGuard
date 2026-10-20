#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "lwip/sockets.h"
#include "cJSON.h"
#include "includes/http_server.h"
#include "includes/client_registry.h"
#include "includes/detection_pipeline.h"
#include "includes/json_handler.h"
#include "includes/network_config.h"

#define MAX_BODY_LEN        1024   // largest detection JSON we accept
#define RECV_MAX_RETRIES    3

static const char *TAG = "HTTP_SERVER";

static httpd_handle_t s_server;

// Sends json (freed here) with the given status line, or a 500 if json is NULL.
static esp_err_t send_json(httpd_req_t *req, const char *status, char *json)
{
    if (json == NULL) {
        return httpd_resp_send_500(req);
    }
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "application/json");
    esp_err_t err = httpd_resp_sendstr(req, json);
    cJSON_free(json);
    return err;
}

static esp_err_t send_error(httpd_req_t *req, const char *status, const char *msg)
{
    ESP_LOGW(TAG, "%s %s -> %s (%s)",
             req->method == HTTP_POST ? "POST" : "GET", req->uri, status, msg);
    return send_json(req, status, build_detection_ack_json("error", NULL, msg));
}

// Reads the whole request body into buf (NUL-terminated). Caller makes sure
// req->content_len < buf_size. Returns false if the client stalls or drops.
static bool recv_body(httpd_req_t *req, char *buf, size_t buf_size)
{
    size_t total = 0;
    int retries = 0;
    while (total < req->content_len) {
        int r = httpd_req_recv(req, buf + total, req->content_len - total);
        if (r == HTTPD_SOCK_ERR_TIMEOUT && ++retries <= RECV_MAX_RETRIES) {
            continue;
        }
        if (r <= 0) {
            return false;
        }
        total += r;
    }
    buf[total] = '\0';
    return true;
}

// IPv4 address of whoever sent this request (0.0.0.0 if it can't be read).
static esp_ip4_addr_t peer_ip(httpd_req_t *req)
{
    esp_ip4_addr_t ip = { .addr = 0 };
    struct sockaddr_storage addr;
    socklen_t len = sizeof(addr);

    if (getpeername(httpd_req_to_sockfd(req), (struct sockaddr *)&addr, &len) == 0) {
        if (addr.ss_family == AF_INET) {
            ip.addr = ((struct sockaddr_in *)&addr)->sin_addr.s_addr;
#if CONFIG_LWIP_IPV6
        } else if (addr.ss_family == AF_INET6) {
            // IPv4-mapped IPv6 address (::ffff:a.b.c.d)
            ip.addr = ((struct sockaddr_in6 *)&addr)->sin6_addr.un.u32_addr[3];
#endif
        }
    }
    return ip;
}

static esp_err_t detections_post_handler(httpd_req_t *req)
{
    if (req->content_len == 0) {
        return send_error(req, "400 Bad Request", "empty body");
    }
    if (req->content_len > MAX_BODY_LEN) {
        return send_error(req, "413 Payload Too Large", "body larger than 1024 bytes");
    }

    char body[MAX_BODY_LEN + 1];
    if (!recv_body(req, body, sizeof(body))) {
        return ESP_FAIL; // connection is broken; httpd closes it
    }

    detection_t det = { 0 };
    if (!parse_detection_json(body, det.plate, sizeof(det.plate))) {
        return send_error(req, "400 Bad Request",
                          "expected JSON with a non-empty string 'plate' (max 31 chars)");
    }

    det.source_ip = peer_ip(req);
    client_registry_set_role_by_ip(det.source_ip, CLIENT_ROLE_DETECTOR);

    if (detection_pipeline_submit(&det) != ESP_OK) {
        return send_error(req, "503 Service Unavailable", "detection queue full, retry later");
    }

    ESP_LOGI(TAG, "detection \"%s\" from " IPSTR " queued",
             det.plate, IP2STR(&det.source_ip));
    return send_json(req, "202 Accepted", build_detection_ack_json("queued", det.plate, NULL));
}

static esp_err_t status_get_handler(httpd_req_t *req)
{
    client_info_t clients[AP_MAX_CLIENTS];
    detection_record_t recent[DETECTION_RECENT_MAX];

    size_t n_clients = client_registry_snapshot(clients, AP_MAX_CLIENTS);
    size_t n_recent = detection_pipeline_recent(recent, DETECTION_RECENT_MAX);

    char *json = build_status_json(esp_timer_get_time() / 1000,
                                   clients, n_clients, recent, n_recent);
    return send_json(req, "200 OK", json);
}

static const httpd_uri_t s_routes[] = {
    { .uri = "/api/detections", .method = HTTP_POST, .handler = detections_post_handler },
    { .uri = "/api/status",     .method = HTTP_GET,  .handler = status_get_handler },
};

esp_err_t http_server_start(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = ESP_HTTP_PORT;
    cfg.stack_size = 8192;        // handlers keep the request body and status snapshot on the stack
    cfg.lru_purge_enable = true;  // with many viewers, drop the idlest connection instead of refusing

    esp_err_t err = httpd_start(&s_server, &cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to start HTTP server: %s", esp_err_to_name(err));
        return err;
    }

    for (size_t i = 0; i < sizeof(s_routes) / sizeof(s_routes[0]); i++) {
        err = http_server_register(&s_routes[i]);
        if (err != ESP_OK) {
            return err;
        }
    }
    ESP_LOGI(TAG, "HTTP server listening on port %d", ESP_HTTP_PORT);
    return ESP_OK;
}

esp_err_t http_server_register(const httpd_uri_t *uri)
{
    esp_err_t err = httpd_register_uri_handler(s_server, uri);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to register %s: %s", uri->uri, esp_err_to_name(err));
    }
    return err;
}
