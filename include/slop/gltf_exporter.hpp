#pragma once

#include "slop/decoded.hpp"
#include "slop/export_result.hpp"
#include "slop/scene_model.hpp"

#include <string>

namespace slop {
namespace exp {

class GltfExporter {
public:
    GltfPaths write(const decode::Decoded &decoded, const std::string &output_path, bool overwrite) const;
    GltfPaths write(const scene::SceneIr &scene_ir, const std::string &output_path, bool overwrite) const;
};

} // namespace exp
} // namespace slop
