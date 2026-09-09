#include "slop/decoder.hpp"

#include "m3g/m3g_parser.hpp"
#include "scene/pattern_texture_attacher.hpp"
#include "scene/scene_builder.hpp"

#include <filesystem>

namespace slop {
namespace decode {
namespace {

Decoded build_decoded(m3g::File file, std::string source_path, const DecodeOptions &options) {
    Decoded decoded;
    decoded.source_path = std::move(source_path);
    decoded.m3g = std::move(file);

    auto base_scene = scene::M3gSceneBuilder(decoded.m3g, decoded.source_path).build();
    decoded.scene_ir = scene::PatternTextureAttacher::auto_attach(base_scene, options.pattern_path);
    return decoded;
}

} // namespace

Decoded Decoder::decode_file(const std::string &input_path, const DecodeOptions &options) const {
    m3g::Parser parser;
    auto file = parser.parse_path(input_path);
    const std::string normalized = std::filesystem::absolute(input_path).lexically_normal().string();
    return build_decoded(std::move(file), normalized, options);
}

Decoded Decoder::decode_bytes(const std::vector<std::uint8_t> &bytes, const std::string &source_path,
                              const DecodeOptions &options) const {
    m3g::Parser parser;
    auto file = parser.parse(bytes);
    std::string normalized = source_path;
    if (!normalized.empty()) {
        normalized = std::filesystem::absolute(normalized).lexically_normal().string();
    }
    return build_decoded(std::move(file), std::move(normalized), options);
}

} // namespace decode
} // namespace slop
