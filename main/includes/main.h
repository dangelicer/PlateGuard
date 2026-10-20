#ifndef MAIN_H
#define MAIN_H

// Set to 1 to compile in and run the Unity test suite from app_main instead
// of the normal startup flow. Set to 0 for a normal build.
#define TEST_MODE 0

// Set to 1 to also run the database-server HTTP test (test_http.c) once the
// laptop running Flask connects. Everything else starts as normal.
#define HTTP_TEST_MODE 0

#endif // MAIN_H
