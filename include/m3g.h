#ifndef m3g_h
#define m3g_h

/*
 * m3g — JSR-184 / M3G tools.
 *
 * C++ API (preferred):
 *   #include <m3g.hpp>
 *
 * This header is a C-compatible version/metadata stub; under C++ it also
 * pulls the full public API from the single amalgamated header.
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define M3G_VERSION_MAJOR 0
#define M3G_VERSION_MINOR 1
#define M3G_VERSION_PATCH 0

/* M3G file identifier length: "\xABJSRI184\xBB\r\n\x1A\n". */
#define M3G_IDENTIFIER_LEN 12

typedef struct m3g_error {
    const char *message;
    size_t offset;
} m3g_error;

#ifdef __cplusplus
}

#include "m3g.hpp"
#endif

#endif /* m3g_h */
