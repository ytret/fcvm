#pragma once

#include <stdbool.h> // IWYU pragma: keep
#include <stdio.h>   // IWYU pragma: keep
#include <stdlib.h>  // IWYU pragma: keep

#define D_PRINT(F)                                                             \
    do {                                                                       \
        printf("" F "\n");                                                     \
    } while (0);
#define D_PRINTF(F, ...)                                                       \
    do {                                                                       \
        printf("" F "\n", __VA_ARGS__);                                        \
    } while (0);

#define D_ASSERT(X)                D_ASSERT_IMPL(X, false, "%s", "")
#define D_ASSERTM(X, MSG)          D_ASSERT_IMPL(X, true, "%s", MSG)
#define D_ASSERTMF(X, MSGFMT, ...) D_ASSERT_IMPL(X, true, MSGFMT, __VA_ARGS__)
#define D_ASSERT_IMPL(X, HAS_MSG, MSGFMT, ...)                                 \
    do {                                                                       \
        if (!(X)) {                                                            \
            D_PRINT("Assertion failed:");                                      \
            if (HAS_MSG) { D_PRINTF(MSGFMT, __VA_ARGS__); }                    \
            D_PRINTF("  Expression: %s", "" #X);                               \
            D_PRINTF("        File: %s", __FILE__);                            \
            D_PRINTF("    Function: %s", __FUNCTION__);                        \
            D_PRINTF("        Line: %u", __LINE__);                            \
            abort();                                                           \
        }                                                                      \
    } while (0);

#define D_TODO() D_ASSERTM(false, "not implemented")
