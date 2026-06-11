/* tests/test_util.h — tiny zero-dependency test framework
 *
 * Usage:
 *     ASSERT_TRUE(expr);
 *     ASSERT_EQ_INT(a, b);
 *     ASSERT_EQ_SIZE(a, b);
 *     ASSERT_NULL(ptr);
 *     RUN(test_foo);     // runs test_foo(), prints status
 *
 *     TEST_SUMMARY();    // prints totals, returns non-zero on fail
 */
#ifndef TEST_UTIL_H
#define TEST_UTIL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int g_pass;
extern int g_fail;

#define ASSERT_TRUE(expr) do {                                              \
    g_pass++;                                                                \
    if (!(expr)) {                                                           \
        g_fail++;                                                            \
        fprintf(stderr, "  FAIL: %s:%d  ASSERT_TRUE(%s)\n",                  \
                __FILE__, __LINE__, #expr);                                  \
    }                                                                        \
} while (0)

#define ASSERT_EQ_INT(a, b) do {                                             \
    long long _a = (long long)(a), _b = (long long)(b);                      \
    g_pass++;                                                                \
    if (_a != _b) {                                                          \
        g_fail++;                                                            \
        fprintf(stderr, "  FAIL: %s:%d  ASSERT_EQ_INT(%s, %s)  (got %lld vs %lld)\n", \
                __FILE__, __LINE__, #a, #b, _a, _b);                         \
    }                                                                        \
} while (0)

#define ASSERT_EQ_SIZE(a, b) do {                                            \
    size_t _a = (size_t)(a), _b = (size_t)(b);                               \
    g_pass++;                                                                \
    if (_a != _b) {                                                          \
        g_fail++;                                                            \
        fprintf(stderr, "  FAIL: %s:%d  ASSERT_EQ_SIZE(%s, %s)  (got %zu vs %zu)\n", \
                __FILE__, __LINE__, #a, #b, _a, _b);                         \
    }                                                                        \
} while (0)

#define ASSERT_NULL(p) do {                                                  \
    g_pass++;                                                                \
    const void *_p = (const void *)(p);                                      \
    if (_p != NULL) {                                                        \
        g_fail++;                                                            \
        fprintf(stderr, "  FAIL: %s:%d  ASSERT_NULL(%s)  (got %p)\n",        \
                __FILE__, __LINE__, #p, _p);                                 \
    }                                                                        \
} while (0)

#define ASSERT_NOT_NULL(p) do {                                              \
    g_pass++;                                                                \
    const void *_p = (const void *)(p);                                      \
    if (_p == NULL) {                                                        \
        g_fail++;                                                            \
        fprintf(stderr, "  FAIL: %s:%d  ASSERT_NOT_NULL(%s)\n",              \
                __FILE__, __LINE__, #p);                                     \
    }                                                                        \
} while (0)

#define RUN(test_fn) do {                                                    \
    int _before = g_fail;                                                    \
    fprintf(stderr, "  [run] %s\n", #test_fn);                               \
    test_fn();                                                               \
    if (g_fail == _before) fprintf(stderr, "  [ok ] %s\n", #test_fn);         \
} while (0)

#define TEST_SUMMARY() do {                                                  \
    fprintf(stderr, "\n%d passed, %d failed\n", g_pass, g_fail);             \
} while (0)

#endif /* TEST_UTIL_H */
