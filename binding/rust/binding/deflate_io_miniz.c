/* Optional miniz backend for m3g_deflate_io (C). */
#include "m3g.h"

#include "miniz.h"

#include <stddef.h>
#include <stdint.h>

uint32_t m3g_miniz_adler32(uint32_t adler, unsigned char const *ptr, size_t buf_len, void *user) {
    (void)user;
    return (uint32_t)mz_adler32(adler, ptr, buf_len);
}

int m3g_miniz_uncompress(unsigned char *dest, size_t *dest_len, unsigned char const *source, size_t source_len,
                         void *user) {
    mz_ulong dlen;
    int rc;
    (void)user;
    dlen = (mz_ulong)(*dest_len);
    rc = mz_uncompress(dest, &dlen, source, (mz_ulong)source_len);
    *dest_len = (size_t)dlen;
    return rc; /* MZ_OK == 0 == M3G_DEFLATE_OK */
}

void *m3g_miniz_uncompress_to_heap(unsigned char const *source, size_t source_len, size_t *out_len, int zlib_header,
                                   void *user) {
    size_t len = 0;
    int flags = zlib_header ? TINFL_FLAG_PARSE_ZLIB_HEADER : 0;
    void *out;
    (void)user;
    out = tinfl_decompress_mem_to_heap(source, source_len, &len, flags);
    if (out_len) {
        *out_len = len;
    }
    return out;
}

void m3g_miniz_free_mem(void *p, void *user) {
    (void)user;
    mz_free(p);
}

void m3g_install_miniz_deflate_io(void) {
    m3g_deflate_io io;
    io.adler32 = m3g_miniz_adler32;
    io.uncompress = m3g_miniz_uncompress;
    io.uncompress_to_heap = m3g_miniz_uncompress_to_heap;
    io.free_mem = m3g_miniz_free_mem;
    io.user = NULL;
    m3g_set_deflate_io(&io);
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((constructor)) static void m3g__miniz_deflate_ctor(void) {
    m3g_install_miniz_deflate_io();
}
#endif
