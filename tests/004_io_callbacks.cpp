/* Unit tests: DeflateIo / ImageIo / GltfIo install and helpers. */
#include "testfw.h"

#include <m3g.hpp>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

std::uint32_t mock_adler32(std::uint32_t adler, unsigned char const *ptr, std::size_t buf_len, void *user)
{
    auto *hits = static_cast<int *>(user);
    if (hits) {
        ++(*hits);
    }
    if (!ptr) {
        return static_cast<std::uint32_t>(m3g::ADLER32_INIT);
    }
    /* trivial non-crypto mix for testing */
    std::uint32_t s = adler ? adler : 1u;
    for (std::size_t i = 0; i < buf_len; ++i) {
        s = s * 131u + ptr[i];
    }
    return s;
}

int mock_uncompress(unsigned char *dest, std::size_t *dest_len, unsigned char const *source, std::size_t source_len,
                    void * /*user*/)
{
    if (!dest || !dest_len || !source) {
        return -1;
    }
    if (*dest_len < source_len) {
        return -2;
    }
    std::memcpy(dest, source, source_len);
    *dest_len = source_len;
    return m3g::DEFLATE_OK;
}

unsigned char *mock_load_memory(unsigned char const *buffer, int len, int *width, int *height, int *channels_in_file,
                                int req_comp, void * /*user*/)
{
    if (!buffer || len < 4 || !width || !height) {
        return nullptr;
    }
    /* fake 1x1 RGB(A) */
    *width = 1;
    *height = 1;
    if (channels_in_file) {
        *channels_in_file = 3;
    }
    const int comp = req_comp > 0 ? req_comp : 3;
    auto *px = static_cast<unsigned char *>(std::malloc(static_cast<std::size_t>(comp)));
    if (!px) {
        return nullptr;
    }
    for (int i = 0; i < comp; ++i) {
        px[i] = buffer[i % len];
    }
    return px;
}

void mock_free_pixels(void *pixels, void * /*user*/) { std::free(pixels); }

int mock_write_file(char const *path, void const *data, int kind, void *user)
{
    auto *log = static_cast<std::string *>(user);
    if (log) {
        *log = std::string(path ? path : "") + ":" + std::to_string(kind) + ":" + (data ? "data" : "null");
    }
    return m3g::GLTF_IO_OK;
}

} // namespace

int main(void)
{
    TESTFW_INIT();

    TESTFW_TEST_BEGIN("deflate_io unset then install mock");
    {
        m3g::set_deflate_io(nullptr);
        TESTFW_EXPECTED(m3g::deflate_io() == nullptr);

        int hits = 0;
        m3g::DeflateIo dio{};
        dio.adler32 = mock_adler32;
        dio.uncompress = mock_uncompress;
        dio.user = &hits;
        m3g::set_deflate_io(&dio);

        TESTFW_EXPECTED(m3g::deflate_io() != nullptr);
        const std::uint32_t seed = m3g::deflate_adler32(0, nullptr, 0);
        TESTFW_EXPECTED(seed == static_cast<std::uint32_t>(m3g::ADLER32_INIT));
        TESTFW_EXPECTED(hits >= 1);

        unsigned char src[] = {1, 2, 3, 4};
        unsigned char dst[4] = {};
        std::size_t n = 4;
        TESTFW_EXPECTED(m3g::deflate_uncompress(dst, &n, src, 4) == m3g::DEFLATE_OK);
        TESTFW_EXPECTED(n == 4u);
        TESTFW_EXPECTED(dst[0] == 1 && dst[3] == 4);

        m3g::set_deflate_io(nullptr);
        TESTFW_EXPECTED(m3g::deflate_io() == nullptr);
    }
    TESTFW_TEST_END();

    TESTFW_TEST_BEGIN("image_io mock load_memory + free");
    {
        m3g::set_image_io(nullptr);
        m3g::ImageIo iio{};
        iio.load_memory = mock_load_memory;
        iio.free_pixels = mock_free_pixels;
        m3g::set_image_io(&iio);

        unsigned char blob[] = {10, 20, 30, 40};
        int w = 0, h = 0, ch = 0;
        unsigned char *px = m3g::image_load_memory(blob, 4, &w, &h, &ch, 4);
        TESTFW_EXPECTED(px != nullptr);
        TESTFW_EXPECTED(w == 1 && h == 1);
        TESTFW_EXPECTED(px[0] == 10);
        m3g::image_free_pixels(px);
        m3g::set_image_io(nullptr);
    }
    TESTFW_TEST_END();

    TESTFW_TEST_BEGIN("gltf_io mock write_file");
    {
        m3g::set_gltf_io(nullptr);
        std::string log;
        m3g::GltfIo gio{};
        gio.write_file = mock_write_file;
        gio.user = &log;
        m3g::set_gltf_io(&gio);

        int dummy = 42;
        m3g::gltf_write_file("/tmp/out.glb", &dummy, m3g::GLTF_FILE_GLB);
        TESTFW_EXPECTED(log.find("/tmp/out.glb") != std::string::npos);
        TESTFW_EXPECTED(log.find(std::to_string(m3g::GLTF_FILE_GLB)) != std::string::npos);

        m3g::set_gltf_io(nullptr);
    }
    TESTFW_TEST_END();

    TESTFW_TEST_BEGIN("version macros");
    TESTFW_EXPECTED(M3G_VERSION_MAJOR == 0);
    TESTFW_EXPECTED(M3G_VERSION_MINOR == 1);
    TESTFW_EXPECTED(M3G_IDENTIFIER_LEN == 12);
    TESTFW_TEST_END();

    return TESTFW_SUMMARY();
}
