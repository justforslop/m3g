#pragma once

#include "slop/m3g_model.hpp"
#include "slop/scene_model.hpp"

#include <optional>
#include <string>
#include <vector>

namespace slop {
namespace decode {

// Fully decoded M3G asset: parsed objects + intermediate scene graph ready for export.
struct Decoded {
    std::string source_path;
    m3g::File m3g;
    scene::SceneIr scene_ir;

    // Convenience mirrors of scene tallies (for logging / tooling).
    std::size_t node_count() const { return scene_ir.nodes.size(); }
    std::size_t mesh_count() const { return scene_ir.meshes.size(); }
    std::size_t material_count() const { return scene_ir.materials.size(); }
    std::size_t texture_count() const { return scene_ir.textures.size(); }
    std::size_t image_count() const { return scene_ir.images.size(); }
    std::size_t camera_count() const { return scene_ir.cameras.size(); }
    std::size_t animation_count() const { return scene_ir.animations.size(); }
    const std::vector<scene::ConversionWarning> &warnings() const { return scene_ir.warnings; }
};

struct DecodeOptions {
    // Optional external pattern image attached to untextured materials.
    std::optional<std::string> pattern_path;
};

} // namespace decode
} // namespace slop
