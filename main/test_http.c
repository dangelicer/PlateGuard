#include "includes/main.h"

#if HTTP_TEST_MODE

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_client.h"
#include "includes/db_client.h"
#include "includes/json_handler.h"
#include "includes/test_http.h"
#include "includes/network_config.h"

#define RESP_BUF_SIZE     256
#define RETRY_PERIOD_MS   2000

typedef enum {
    EXPECT_AUTHORIZED,     // server replies 200 with "authorized": true
    EXPECT_UNAUTHORIZED,   // server replies 200 with "authorized": false
    EXPECT_REJECTED,       // server replies with an error (e.g. 400)
} expectation_t;

typedef struct {
    const char *plate;
    expectation_t expect;
    const char *description;
} plate_test_t;

// Before running, authorize TXA1234 on the laptop (from plateguard_backend):
//   python manage.py set-status TXA1234 AUTHORIZED --create
// Every lookup is recorded by the server, so the other plates below will be
// added to the database as UNAUTHORIZED (and stay that way unless an admin
// changes them).
static const plate_test_t s_tests[] = {
    { "TXA1234",                          EXPECT_AUTHORIZED,   "admin-authorized plate" },
    { "ZZZ9999",                          EXPECT_UNAUTHORIZED, "plate never authorized" },
    { "txa1234",                          EXPECT_UNAUTHORIZED, "lowercase of an authorized plate (case-sensitive)" },
    { "TXA1234' OR '1'='1",               EXPECT_UNAUTHORIZED, "SQL injection attempt" },
    { "",                                 EXPECT_REJECTED,     "empty plate (server should reject)" },
    { "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456", EXPECT_REJECTED,    "33-char plate (server max is 31)" },
};

static const char *expect_str(expectation_t e)
{
    switch (e) {
    case EXPECT_AUTHORIZED:   return "authorized";
    case EXPECT_UNAUTHORIZED: return "unauthorized";
    default:                  return "rejected";
    }
}

// Blocks until the laptop is connected and Flask accepts connections. Probes
// with an empty plate, which the server rejects without touching the database.
static void wait_for_server(void)
{
    char resp[RESP_BUF_SIZE];
    while (1) {
        esp_err_t err = post_plate_check("", resp, sizeof(resp));
        if (err != ESP_ERR_INVALID_STATE && err != ESP_ERR_HTTP_CONNECT) {
            return; // got an HTTP response of some kind
        }
        printf("waiting for %s...\n",
               err == ESP_ERR_INVALID_STATE ? "database server (" DB_SERVER_HOSTNAME ") to join Wi-Fi" : "Flask server on port 5000");
        vTaskDelay(pdMS_TO_TICKS(RETRY_PERIOD_MS));
    }
}

void http_test_task(void *arg)
{
    printf("\n=== HTTP test: waiting for server ===\n");
    wait_for_server();

    const int total = sizeof(s_tests) / sizeof(s_tests[0]);
    int passed = 0;

    printf("\n=== HTTP test: running %d cases ===\n", total);
    for (int i = 0; i < total; i++) {
        const plate_test_t *t = &s_tests[i];
        char resp[RESP_BUF_SIZE] = "";

        esp_err_t err = post_plate_check(t->plate, resp, sizeof(resp));

        bool authorized = false;
        bool parsed = (err == ESP_OK) && parse_plate_check_response(resp, &authorized);

        const char *got;
        if (parsed) {
            got = authorized ? "authorized" : "unauthorized";
        } else if (err == ESP_FAIL && resp[0] != '\0') {
            got = "rejected"; // server answered with a non-200 status
        } else {
            got = esp_err_to_name(err);
        }

        bool ok = strcmp(got, expect_str(t->expect)) == 0;
        if (ok) {
            passed++;
        }

        printf("\n[%d/%d] %s\n", i + 1, total, t->description);
        printf("  sent plate : \"%s\"\n", t->plate);
        printf("  response   : %s\n", resp[0] ? resp : "(none)");
        printf("  expected   : %s\n", expect_str(t->expect));
        printf("  got        : %s  -> %s\n", got, ok ? "PASS" : "FAIL");
    }

    printf("\n=== HTTP test: %d/%d passed ===\n", passed, total);
    vTaskDelete(NULL);
}

#endif // HTTP_TEST_MODE
