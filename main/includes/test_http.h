#ifndef TEST_HTTP_H
#define TEST_HTTP_H

// FreeRTOS task that waits for the laptop + Flask server, sends a fixed set of
// test plates, and prints each raw response and a pass/fail line. Compiled to
// an empty translation unit when HTTP_TEST_MODE is 0 (see main.h).
void http_test_task(void *arg);

#endif // TEST_HTTP_H
