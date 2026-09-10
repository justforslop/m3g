#ifndef M3G_PNG_WRITER_HPP_INCLUDED
#define M3G_PNG_WRITER_HPP_INCLUDED

#include <cstdint>
#include <string>
#include <vector>

namespace m3g {
namespace util {

// RGBA8 image writer backed by stb_image_write (stbi_write_png).
struct PngWriter {
    static void write_rgba(const std::string &path, int width, int height, const std::vector<std::uint8_t> &pixels,
                           int compression_level = 8);
};

} // namespace util
} // namespace m3g

#endif /* M3G_PNG_WRITER_HPP_INCLUDED */
