#include "scene/pattern_texture_attacher.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <limits>
#include <set>

namespace slop {
namespace scene {
namespace {

constexpr float UV_EPSILON = 1e-6f;
constexpr int WRAP_REPEAT = 10497;

std::string to_lower(std::string s) {
    for (char &c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

std::vector<float> generate_planar_uv_xz(const std::vector<float> &positions) {
    const int vertex_count = static_cast<int>(positions.size() / 3);
    std::vector<float> uv(static_cast<std::size_t>(vertex_count) * 2u);
    if (vertex_count == 0) {
        return uv;
    }
    float min_x = std::numeric_limits<float>::infinity();
    float max_x = -std::numeric_limits<float>::infinity();
    float min_z = std::numeric_limits<float>::infinity();
    float max_z = -std::numeric_limits<float>::infinity();
    for (int vertex = 0; vertex < vertex_count; ++vertex) {
        const float x = positions[static_cast<std::size_t>(vertex * 3)];
        const float z = positions[static_cast<std::size_t>(vertex * 3 + 2)];
        min_x = std::min(min_x, x);
        max_x = std::max(max_x, x);
        min_z = std::min(min_z, z);
        max_z = std::max(max_z, z);
    }
    const float span_x = max_x - min_x;
    const float span_z = max_z - min_z;
    const bool use_flat_u = span_x <= UV_EPSILON;
    const bool use_flat_v = span_z <= UV_EPSILON;
    for (int vertex = 0; vertex < vertex_count; ++vertex) {
        const float x = positions[static_cast<std::size_t>(vertex * 3)];
        const float z = positions[static_cast<std::size_t>(vertex * 3 + 2)];
        uv[static_cast<std::size_t>(vertex * 2)] = use_flat_u ? 0.5f : (x - min_x) / span_x;
        uv[static_cast<std::size_t>(vertex * 2 + 1)] = use_flat_v ? 0.5f : (z - min_z) / span_z;
    }
    return uv;
}

SceneIr with_warning(SceneIr scene, const std::string &code, const std::string &message) {
    ConversionWarning warning{code, message};
    if (std::find(scene.warnings.begin(), scene.warnings.end(), warning) == scene.warnings.end()) {
        scene.warnings.push_back(std::move(warning));
    }
    return scene;
}

} // namespace

SceneIr PatternTextureAttacher::auto_attach(const SceneIr &scene, const std::optional<std::string> &pattern_path) {
    if (!pattern_path) {
        return scene;
    }
    namespace fs = std::filesystem;
    const fs::path normalized = fs::absolute(*pattern_path).lexically_normal();
    if (!fs::exists(normalized)) {
        return with_warning(scene, "pattern-missing",
                            "Pattern image not found at " + normalized.string() + "; skipping auto-attach.");
    }
    const std::string extension = to_lower(normalized.extension().string());
    std::string ext = extension;
    if (!ext.empty() && ext[0] == '.') {
        ext.erase(ext.begin());
    }
    if (ext != "png" && ext != "jpg" && ext != "jpeg") {
        return with_warning(scene, "pattern-format",
                            "Pattern image " + normalized.string() + " has unsupported extension '" + ext +
                                "'; expected png, jpg, or jpeg.");
    }

    std::set<int> target_material_indices;
    for (std::size_t i = 0; i < scene.materials.size(); ++i) {
        if (!scene.materials[i].base_color_texture_index) {
            target_material_indices.insert(static_cast<int>(i));
        }
    }
    if (target_material_indices.empty()) {
        return with_warning(scene, "pattern-no-target",
                            "Pattern image was provided, but no materials without base color textures were found.");
    }

    SceneIr out = scene;
    const int pattern_image_index = static_cast<int>(out.images.size());
    SceneImageIr image;
    image.name = "AutoPattern_" + normalized.filename().string();
    ExternalFileImageSource src;
    src.object_id = -1;
    src.source_path = normalized.string();
    image.external = std::move(src);
    out.images.push_back(std::move(image));

    int pattern_sampler_index = -1;
    for (std::size_t i = 0; i < out.samplers.size(); ++i) {
        const auto &sampler = out.samplers[i];
        if (sampler.wrap_s == WRAP_REPEAT && sampler.wrap_t == WRAP_REPEAT && !sampler.mag_filter &&
            !sampler.min_filter) {
            pattern_sampler_index = static_cast<int>(i);
            break;
        }
    }
    if (pattern_sampler_index < 0) {
        SceneSamplerIr sampler;
        sampler.wrap_s = WRAP_REPEAT;
        sampler.wrap_t = WRAP_REPEAT;
        pattern_sampler_index = static_cast<int>(out.samplers.size());
        out.samplers.push_back(sampler);
    }

    const int pattern_texture_index = static_cast<int>(out.textures.size());
    SceneTextureIr texture;
    texture.name = "AutoPatternTexture";
    texture.image_index = pattern_image_index;
    texture.sampler_index = pattern_sampler_index;
    out.textures.push_back(std::move(texture));

    for (std::size_t material_index = 0; material_index < out.materials.size(); ++material_index) {
        if (target_material_indices.count(static_cast<int>(material_index))) {
            out.materials[material_index].base_color_texture_index = pattern_texture_index;
        }
    }

    for (auto &mesh : out.meshes) {
        for (auto &primitive : mesh.primitives) {
            const bool is_target = primitive.material_index &&
                                   target_material_indices.count(*primitive.material_index) > 0;
            if (!is_target || primitive.tex_coords0) {
                continue;
            }
            primitive.tex_coords0 = generate_planar_uv_xz(primitive.positions);
        }
    }
    return out;
}

} // namespace scene
} // namespace slop
