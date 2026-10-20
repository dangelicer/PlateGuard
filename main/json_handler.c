#include <stdio.h>
#include <string.h>
#include "cJSON.h"
#include "esp_mac.h"
#include "includes/json_handler.h"

bool parse_detection_json(const char *json_buf, char *plate_out, size_t plate_out_size)
{
    cJSON *root = cJSON_Parse(json_buf);
    if (root == NULL) {
        return false;
    }

    cJSON *plate = cJSON_GetObjectItemCaseSensitive(root, "plate");

    // reject rather than truncate: a cut-off plate would be looked up wrong
    if (!cJSON_IsString(plate) || plate->valuestring == NULL ||
        plate->valuestring[0] == '\0' || strlen(plate->valuestring) >= plate_out_size) {
        cJSON_Delete(root);
        return false;
    }

    strcpy(plate_out, plate->valuestring);

    cJSON_Delete(root);
    return true;
}

char *build_plate_check_json(const char *plate)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return NULL;
    }

    if (cJSON_AddStringToObject(root, "plate", plate) == NULL) {
        cJSON_Delete(root);
        return NULL;
    }

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_str;
}

bool parse_plate_check_response(const char *json_buf, bool *authorized_out)
{
    cJSON *root = cJSON_Parse(json_buf);
    if (root == NULL) {
        return false;
    }

    cJSON *authorized = cJSON_GetObjectItemCaseSensitive(root, "authorized");
    if (!cJSON_IsBool(authorized)) {
        cJSON_Delete(root);
        return false;
    }

    *authorized_out = cJSON_IsTrue(authorized);

    cJSON_Delete(root);
    return true;
}

char *build_detection_ack_json(const char *status, const char *plate, const char *error)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return NULL;
    }

    bool ok = cJSON_AddStringToObject(root, "status", status) != NULL;
    if (ok && plate != NULL) {
        ok = cJSON_AddStringToObject(root, "plate", plate) != NULL;
    }
    if (ok && error != NULL) {
        ok = cJSON_AddStringToObject(root, "error", error) != NULL;
    }

    char *json_str = ok ? cJSON_PrintUnformatted(root) : NULL;
    cJSON_Delete(root);
    return json_str;
}

char *build_status_json(int64_t uptime_ms,
                        const client_info_t *clients, size_t n_clients,
                        const detection_record_t *recent, size_t n_recent)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return NULL;
    }
    cJSON_AddNumberToObject(root, "uptime_ms", (double)uptime_ms);

    cJSON *client_arr = cJSON_AddArrayToObject(root, "clients");
    for (size_t i = 0; client_arr != NULL && i < n_clients; i++) {
        const client_info_t *c = &clients[i];
        cJSON *obj = cJSON_CreateObject();
        if (obj == NULL) {
            break;
        }
        char mac[18], ip[16];
        snprintf(mac, sizeof(mac), MACSTR, MAC2STR(c->mac));
        snprintf(ip, sizeof(ip), IPSTR, IP2STR(&c->ip));
        cJSON_AddStringToObject(obj, "mac", mac);
        if (c->ip.addr != 0) {
            cJSON_AddStringToObject(obj, "ip", ip);
        } else {
            cJSON_AddNullToObject(obj, "ip");
        }
        cJSON_AddStringToObject(obj, "hostname", c->hostname);
        cJSON_AddStringToObject(obj, "role", client_role_name(c->role));
        cJSON_AddItemToArray(client_arr, obj);
    }

    cJSON *det_arr = cJSON_AddArrayToObject(root, "recent_detections");
    for (size_t i = 0; det_arr != NULL && i < n_recent; i++) {
        const detection_record_t *r = &recent[i];
        cJSON *obj = cJSON_CreateObject();
        if (obj == NULL) {
            break;
        }
        char ip[16];
        snprintf(ip, sizeof(ip), IPSTR, IP2STR(&r->detection.source_ip));
        cJSON_AddStringToObject(obj, "plate", r->detection.plate);
        cJSON_AddStringToObject(obj, "source_ip", ip);
        cJSON_AddStringToObject(obj, "result", db_result_name(r->result));
        cJSON_AddNumberToObject(obj, "received_ms", (double)r->received_ms);
        cJSON_AddItemToArray(det_arr, obj);
    }

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_str;
}
