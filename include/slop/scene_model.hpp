#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace slop {
namespace scene {

struct ConversionWarning {
    std::string code;
    std::string message;
    bool operator==(const ConversionWarning &o) const {
        return code == o.code && message == o.message;
    }
};

struct EmbeddedRgbaImageSource {
    int object_id = 0;
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> pixels;
};

struct ExternalFileImageSource {
    int object_id = 0;
    std::string source_path;
};

struct SceneImageIr {
    std::string name;
    // Exactly one of embedded / external is set.
    std::optional<EmbeddedRgbaImageSource> embedded;
    std::optional<ExternalFileImageSource> external;
};

struct SceneSamplerIr {
    std::optional<int> mag_filter;
    std::optional<int> min_filter;
    int wrap_s = 0;
    int wrap_t = 0;
};

struct SceneTextureIr {
    std::string name;
    int image_index = 0;
    std::optional<int> sampler_index;
};

struct SceneMaterialIr {
    std::string name;
    std::vector<float> base_color_factor{1, 1, 1, 1};
    std::optional<int> base_color_texture_index;
    std::vector<float> emissive_factor{0, 0, 0};
    float roughness_factor = 1.f;
    float metallic_factor = 0.f;
    bool double_sided = false;
    std::optional<std::string> alpha_mode;
};

struct ScenePrimitiveIr {
    std::string name;
    std::vector<float> positions;
    std::optional<std::vector<float>> normals;
    std::optional<std::vector<float>> tex_coords0;
    std::optional<std::vector<float>> vertex_colors;
    std::vector<int> indices;
    std::optional<int> material_index;
};

struct SceneMeshIr {
    std::string name;
    std::vector<ScenePrimitiveIr> primitives;
};

struct ScenePerspectiveCameraIr {
    float yfov_radians = 0.f;
    std::optional<float> aspect_ratio;
    float znear = 0.1f;
    std::optional<float> zfar;
};

struct SceneCameraIr {
    std::string name;
    std::optional<ScenePerspectiveCameraIr> perspective;
};

struct SceneNodeIr {
    std::string name;
    std::optional<std::vector<float>> matrix;
    std::optional<std::vector<float>> translation;
    std::optional<std::vector<float>> rotation;
    std::optional<std::vector<float>> scale;
    std::optional<int> mesh_index;
    std::optional<int> camera_index;
    std::vector<int> children;
};

struct SceneAnimationSamplerIr {
    std::vector<float> times;
    std::vector<float> values;
    std::string interpolation = "LINEAR";
    int component_count = 3;
};

struct SceneAnimationChannelIr {
    int sampler_index = 0;
    int node_index = 0;
    std::string path;
};

struct SceneAnimationIr {
    std::string name;
    std::vector<SceneAnimationSamplerIr> samplers;
    std::vector<SceneAnimationChannelIr> channels;
};

struct SceneIr {
    std::vector<SceneNodeIr> nodes;
    std::vector<int> root_node_indices;
    std::vector<SceneMeshIr> meshes;
    std::vector<SceneMaterialIr> materials;
    std::vector<SceneTextureIr> textures;
    std::vector<SceneImageIr> images;
    std::vector<SceneSamplerIr> samplers;
    std::vector<SceneCameraIr> cameras;
    std::vector<SceneAnimationIr> animations;
    std::vector<ConversionWarning> warnings;
};

// Low-level glTF writer paths (also mirrored in exp::GltfPaths).
struct GltfWriteResult {
    std::string gltf_path;
    std::string bin_path;
    std::vector<std::string> image_paths;
};

} // namespace scene
} // namespace slop
