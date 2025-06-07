#ifndef TEST_H
#define TEST_H

#include <stdio.h>
#include <stdlib.h>

#define TEST(name) void name(void)
#define ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "[FAIL] %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        exit(1); \
    } \
} while (0)

#define RUN(test) do { \
    printf("[RUN]  %s\n", #test); \
    test(); \
    printf("[PASS] %s\n", #test); \
} while (0)

#endif

