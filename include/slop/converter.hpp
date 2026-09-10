#pragma once

#include "slop/decoded.hpp"
#include "slop/export_result.hpp"

#include <optional>
#include <string>

namespace slop {

// Facade: decode M3G → scene IR → glTF.
// Prefer decode::Decoder / exp::GltfExporter for a single stage.
class M3gConverter {
public:
    decode::Decoded decode(const std::string &input_path,
                           const std::optional<std::string> &pattern_path = std::nullopt) const;

    exp::GltfPaths export_gltf(const decode::Decoded &decoded, const std::string &output_path, bool overwrite,
                               int png_compression_level = 8) const;

    exp::ExportReport convert(const std::string &input_path, const std::string &output_path, bool overwrite,
                              const std::optional<std::string> &pattern_path = std::nullopt,
                              int png_compression_level = 8) const;
};

} // namespace slop
