#include "includes/main.h"

#if TEST_MODE

#include "unity.h"
#include "includes/json_handler.h"
#include "includes/test_json_handler.h"

static void test_valid_buffer(void)
{
    const char *buf = "{\"plate\":\"TXA1234\"}";
    char plate[32];

    bool ok = parse_detection_json(buf, plate, sizeof(plate));

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("TXA1234", plate);
}

static void test_extra_fields_ignored(void)
{
    // older payload format with confidence/car_info still parses
    const char *buf =
        "{\"plate\":\"TXA1234\",\"confidence\":0.87,"
        "\"car_info\":{\"make\":\"Toyota\",\"model\":\"Camry\",\"color\":\"Silver\"}}";
    char plate[32];

    bool ok = parse_detection_json(buf, plate, sizeof(plate));

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("TXA1234", plate);
}

static void test_empty_buffer(void)
{
    const char *buf = "";
    char plate[32];

    bool ok = parse_detection_json(buf, plate, sizeof(plate));

    TEST_ASSERT_FALSE(ok);
}

static void test_missing_plate(void)
{
    const char *buf = "{\"number\":\"TXA1234\"}";
    char plate[32];

    bool ok = parse_detection_json(buf, plate, sizeof(plate));

    TEST_ASSERT_FALSE(ok);
}

static void test_malformed_plate(void)
{
    // "plate" is a number instead of a string
    const char *buf = "{\"plate\":1234}";
    char plate[32];

    bool ok = parse_detection_json(buf, plate, sizeof(plate));

    TEST_ASSERT_FALSE(ok);
}

static void test_empty_plate(void)
{
    const char *buf = "{\"plate\":\"\"}";
    char plate[32];

    bool ok = parse_detection_json(buf, plate, sizeof(plate));

    TEST_ASSERT_FALSE(ok);
}

static void test_plate_too_long(void)
{
    // 8 characters don't fit in an 8-byte buffer (needs room for '\0')
    const char *buf = "{\"plate\":\"ABCD1234\"}";
    char plate[8];

    bool ok = parse_detection_json(buf, plate, sizeof(plate));

    TEST_ASSERT_FALSE(ok);
}

static void test_plate_exactly_fits(void)
{
    const char *buf = "{\"plate\":\"ABC1234\"}";
    char plate[8];

    bool ok = parse_detection_json(buf, plate, sizeof(plate));

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("ABC1234", plate);
}

static void test_response_authorized(void)
{
    const char *buf =
        "{\"attempts\":3,\"authorized\":true,\"exists\":true,"
        "\"plate\":\"TXA1234\",\"status\":\"AUTHORIZED\",\"timestamp\":\"2026-10-19T12:14:33-05:00\"}";
    bool authorized = false;

    TEST_ASSERT_TRUE(parse_plate_check_response(buf, &authorized));
    TEST_ASSERT_TRUE(authorized);
}

static void test_response_unauthorized(void)
{
    // a plate that is in the database is still not authorized unless the admin says so
    const char *buf = "{\"authorized\":false,\"exists\":true,\"status\":\"UNAUTHORIZED\"}";
    bool authorized = true;

    TEST_ASSERT_TRUE(parse_plate_check_response(buf, &authorized));
    TEST_ASSERT_FALSE(authorized);
}

static void test_response_missing_authorized(void)
{
    // old backend format: "exists" alone must not be read as authorized
    const char *buf = "{\"plate\":\"TXA1234\",\"exists\":true}";
    bool authorized = false;

    TEST_ASSERT_FALSE(parse_plate_check_response(buf, &authorized));
}

static void test_response_authorized_not_bool(void)
{
    const char *buf = "{\"authorized\":\"true\"}";
    bool authorized = false;

    TEST_ASSERT_FALSE(parse_plate_check_response(buf, &authorized));
}

static void test_response_invalid_json(void)
{
    bool authorized = false;

    TEST_ASSERT_FALSE(parse_plate_check_response("{\"error\":", &authorized));
}

void test_json_parse(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_valid_buffer);
    RUN_TEST(test_extra_fields_ignored);
    RUN_TEST(test_empty_buffer);
    RUN_TEST(test_missing_plate);
    RUN_TEST(test_malformed_plate);
    RUN_TEST(test_empty_plate);
    RUN_TEST(test_plate_too_long);
    RUN_TEST(test_plate_exactly_fits);
    RUN_TEST(test_response_authorized);
    RUN_TEST(test_response_unauthorized);
    RUN_TEST(test_response_missing_authorized);
    RUN_TEST(test_response_authorized_not_bool);
    RUN_TEST(test_response_invalid_json);

    UNITY_END();
}

#endif // TEST_MODE
