/* Optional miniz backend for m3g::DeflateIo. */
#include "m3g.hpp"

#include "miniz.h"

namespace m3g {
namespace {

std::uint32_t miniz_adler32(std::uint32_t adler, unsigned char const *ptr, std::size_t buf_len, void * /*user*/) {
    return static_cast<std::uint32_t>(mz_adler32(adler, ptr, buf_len));
}

int miniz_uncompress(unsigned char *dest, std::size_t *dest_len, unsigned char const *source, std::size_t source_len,
                     void * /*user*/) {
    mz_ulong dlen = static_cast<mz_ulong>(*dest_len);
    const int rc = mz_uncompress(dest, &dlen, source, static_cast<mz_ulong>(source_len));
    *dest_len = static_cast<std::size_t>(dlen);
    return rc; /* MZ_OK == 0 == DEFLATE_OK */
}

void *miniz_uncompress_to_heap(unsigned char const *source, std::size_t source_len, std::size_t *out_len,
                               int zlib_header, void * /*user*/) {
    size_t len = 0;
    const int flags = zlib_header ? TINFL_FLAG_PARSE_ZLIB_HEADER : 0;
    void *out = tinfl_decompress_mem_to_heap(source, source_len, &len, flags);
    if (out_len) {
        *out_len = static_cast<std::size_t>(len);
    }
    return out;
}

void miniz_free_mem(void *p, void * /*user*/) { mz_free(p); }

} // namespace

void install_miniz_deflate_io(void) {
    DeflateIo io{};
    io.adler32 = miniz_adler32;
    io.uncompress = miniz_uncompress;
    io.uncompress_to_heap = miniz_uncompress_to_heap;
    io.free_mem = miniz_free_mem;
    io.user = nullptr;
    set_deflate_io(&io);
}

namespace {
struct MinizDeflateIoAutoInstall {
    MinizDeflateIoAutoInstall() { install_miniz_deflate_io(); }
};
static MinizDeflateIoAutoInstall g_m3g_miniz_deflate_io_auto_install;
} // namespace

} // namespace m3g
