#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace slop {
namespace util {

// RGBA8 image writer backed by stb_image_write (stbi_write_png).
struct PngWriter {
    static void write_rgba(const std::string &path, int width, int height, const std::vector<std::uint8_t> &pixels);
};

} // namespace util
} // namespace slop
