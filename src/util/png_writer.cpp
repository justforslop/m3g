#include "util/png_writer.hpp"

#include "stb/stb_image_write.h"

#include <filesystem>
#include <stdexcept>

namespace slop {
namespace util {

void PngWriter::write_rgba(const std::string &path, int width, int height, const std::vector<std::uint8_t> &pixels) {
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("PNG dimensions must be positive");
    }
    const std::size_t expected = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u;
    if (pixels.size() != expected) {
        throw std::invalid_argument("Unexpected RGBA pixel buffer size for PNG");
    }

    namespace fs = std::filesystem;
    const fs::path p(path);
    if (p.has_parent_path()) {
        fs::create_directories(p.parent_path());
    }

    // stbi_write_png stride = bytes per row
    const int stride = width * 4;
    if (stbi_write_png(path.c_str(), width, height, 4, pixels.data(), stride) == 0) {
        throw std::runtime_error("stbi_write_png failed: " + path);
    }
}

} // namespace util
} // namespace slop
