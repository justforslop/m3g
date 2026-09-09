#pragma once

#include "slop/decoded.hpp"
#include "slop/scene_model.hpp"

#include <string>
#include <vector>

namespace slop {
namespace exp {

struct GltfPaths {
    std::string gltf_path;
    std::string bin_path;
    std::vector<std::string> image_paths;
};

// Result of decode + glTF write (decoded module + on-disk paths).
struct ExportReport {
    decode::Decoded decoded;
    GltfPaths paths;
};

} // namespace exp
} // namespace slop
