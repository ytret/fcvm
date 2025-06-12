/**
 * @file portability.h
 * Support header for portability between systems, compilers and editors.
 */

#pragma once

#if defined(__has_builtin)
#    if __has_builtin(__builtin_ctz)
#        define HAS_BUILTIN_CTZ 1
#    endif
#elif defined(__GNUC__) || defined(__clang__)
#    define HAS_BUILTIN_CTZ 1
#endif

#if HAS_BUILTIN_CTZ
#    define stdc_first_trailing_one(x) (1 + __builtin_ctz(x))
#endif
