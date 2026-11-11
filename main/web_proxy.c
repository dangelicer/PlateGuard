#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "esp_log.h"
#include "esp_http_client.h"
#include "includes/web_proxy.h"
#include "includes/http_server.h"
#include "includes/client_registry.h"
#include "includes/network_config.h"

static const char *TAG = "WEB_PROXY";

// Response headers from the database server that we pass on to the browser.
// esp_http_server keeps pointers to these until the response is finished, so
// they live in the handler's stack frame for the whole request.
typedef struct {
    char content_type[64];
    char location[128];
    char cache_control[64];
} upstream_headers_t;

static void copy_header(char *dst, size_t dst_size, const char *value)
{
    strncpy(dst, value, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

// Called by esp_http_client for each response header from the database server.
static esp_err_t on_client_event(esp_http_client_event_t *evt)
{
    if (evt->event_id != HTTP_EVENT_ON_HEADER || evt->user_data == NULL) {
        return ESP_OK;
    }
    upstream_headers_t *h = evt->user_data;
    if (strcasecmp(evt->header_key, "Content-Type") == 0) {
        copy_header(h->content_type, sizeof(h->content_type), evt->header_value);
    } else if (strcasecmp(evt->header_key, "Location") == 0) {
        // Flask may send an absolute URL with the laptop's address
        // (http://192.168.4.x:5000/dashboard/); the browser must stay on the ESP,
        // so keep only the path.
        const char *v = evt->header_value;
        const char *scheme = strstr(v, "://");
        if (scheme != NULL) {
            const char *path = strchr(scheme + 3, '/');
            v = path != NULL ? path : "/";
        }
        copy_header(h->location, sizeof(h->location), v);
    } else if (strcasecmp(evt->header_key, "Cache-Control") == 0) {
        copy_header(h->cache_control, sizeof(h->cache_control), evt->header_value);
    }
    return ESP_OK;
}

// httpd_resp_set_status() needs a full status line such as "404 Not Found".
static const char *status_line(int code)
{
    switch (code) {
    case 200: return "200 OK";
    case 204: return "204 No Content";
    case 301: return "301 Moved Permanently";
    case 302: return "302 Found";
    case 303: return "303 See Other";
    case 304: return "304 Not Modified";
    case 307: return "307 Temporary Redirect";
    case 308: return "308 Permanent Redirect";
    case 400: return "400 Bad Request";
    case 403: return "403 Forbidden";
    case 404: return "404 Not Found";
    case 405: return "405 Method Not Allowed";
    case 500: return "500 Internal Server Error";
    case 503: return "503 Service Unavailable";
    default:  return "502 Bad Gateway";
    }
}

static esp_err_t send_plain_error(httpd_req_t *req, const char *status, const char *msg)
{
    ESP_LOGW(TAG, "GET %s -> %s (%s)", req->uri, status, msg);
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_set_hdr(req, "Retry-After", "5");
    return httpd_resp_sendstr(req, msg);
}

static esp_err_t redirect_to_dashboard(httpd_req_t *req)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", WEB_PROXY_PREFIX "/");
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t proxy_get_handler(httpd_req_t *req)
{
    client_info_t server;
    if (!client_registry_find_role(CLIENT_ROLE_DB_SERVER, &server)) {
        return send_plain_error(req, "503 Service Unavailable",
                                "PlateGuard: the database server (" DB_SERVER_NAME ") is not "
                                "connected to this Wi-Fi yet. This page will work once it is.");
    }

    // req->uri is the path plus query string, e.g. /dashboard/api/status
    char url[32 + sizeof(req->uri)];
    snprintf(url, sizeof(url), "http://" IPSTR ":%d%s", IP2STR(&server.ip), DB_SERVER_PORT, req->uri);

    upstream_headers_t hdrs = { 0 };
    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = WEB_PROXY_TIMEOUT_MS,
        .disable_auto_redirect = true,   // pass redirects to the browser instead
        .event_handler = on_client_event,
        .user_data = &hdrs,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client == NULL) {
        return send_plain_error(req, "500 Internal Server Error", "out of memory");
    }

    esp_err_t ret = ESP_OK;
    if (esp_http_client_open(client, 0) != ESP_OK) {
        ret = send_plain_error(req, "502 Bad Gateway",
                               "PlateGuard: could not reach the database server.");
        goto done;
    }
    if (esp_http_client_fetch_headers(client) < 0) {
        ret = send_plain_error(req, "502 Bad Gateway",
                               "PlateGuard: the database server did not answer.");
        goto done;
    }

    int status = esp_http_client_get_status_code(client);
    httpd_resp_set_status(req, status_line(status));
    if (hdrs.content_type[0] != '\0') {
        httpd_resp_set_type(req, hdrs.content_type);
    }
    if (hdrs.location[0] != '\0') {
        httpd_resp_set_hdr(req, "Location", hdrs.location);
    }
    if (hdrs.cache_control[0] != '\0') {
        httpd_resp_set_hdr(req, "Cache-Control", hdrs.cache_control);
    }

    // Relay the body a chunk at a time: read up to WEB_PROXY_CHUNK_SIZE bytes
    // from Flask, send them to the browser as one HTTP chunk, repeat. The
    // browser joins the chunks back into the full page/stylesheet/JSON.
    char chunk[WEB_PROXY_CHUNK_SIZE];
    size_t total = 0;
    while (1) {
        int n = esp_http_client_read(client, chunk, sizeof(chunk));
        if (n < 0) {
            // Headers are already out, so we can't send an error page; drop
            // the connection so the browser sees an incomplete response.
            ESP_LOGE(TAG, "GET %s: read from database server failed after %u bytes",
                     req->uri, (unsigned)total);
            ret = ESP_FAIL;
            goto done;
        }
        if (n == 0) {
            break;   // whole body relayed
        }
        if (httpd_resp_send_chunk(req, chunk, n) != ESP_OK) {
            ESP_LOGW(TAG, "GET %s: viewer closed the connection", req->uri);
            ret = ESP_FAIL;
            goto done;
        }
        total += n;
    }
    ret = httpd_resp_send_chunk(req, NULL, 0);   // terminating zero-length chunk
    ESP_LOGI(TAG, "GET %s -> %d (%u bytes)", req->uri, status, (unsigned)total);

done:
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return ret;
}

static const httpd_uri_t s_routes[] = {
    { .uri = "/",                       .method = HTTP_GET, .handler = redirect_to_dashboard },
    { .uri = WEB_PROXY_PREFIX,          .method = HTTP_GET, .handler = redirect_to_dashboard },
    { .uri = WEB_PROXY_PREFIX "/*",     .method = HTTP_GET, .handler = proxy_get_handler },
};

esp_err_t web_proxy_start(void)
{
    for (size_t i = 0; i < sizeof(s_routes) / sizeof(s_routes[0]); i++) {
        esp_err_t err = http_server_register(&s_routes[i]);
        if (err != ESP_OK) {
            return err;
        }
    }
    ESP_LOGI(TAG, "dashboard at http://192.168.4.1/ (proxied from the database server, %d-byte chunks)",
             WEB_PROXY_CHUNK_SIZE);
    return ESP_OK;
}
