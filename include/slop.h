#ifndef SLOP_H
#define SLOP_H

/*
 * slop — JSR-184 / M3G tools.
 *
 * C++ API (preferred):
 *   #include <slop.hpp>
 *   #include <slop/decode.hpp>
 *   #include <slop/convert.hpp>
 *
 * This header is a C-compatible version/metadata stub; under C++ it also
 * pulls the full public API.
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SLOP_VERSION_MAJOR 0
#define SLOP_VERSION_MINOR 1
#define SLOP_VERSION_PATCH 0

/* M3G file identifier length: "\xABJSRI184\xBB\r\n\x1A\n". */
#define SLOP_M3G_IDENTIFIER_LEN 12

typedef struct slop_error {
    const char *message;
    size_t offset;
} slop_error;

#ifdef __cplusplus
}

#include "slop/decode.hpp"
#include "slop/convert.hpp"
#endif

#endif /* SLOP_H */
