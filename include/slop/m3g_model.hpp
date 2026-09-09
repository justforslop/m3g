#pragma once

#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace slop {
namespace m3g {

struct ObjectTypes {
    static constexpr int HEADER = 0;
    static constexpr int ANIMATION_CONTROLLER = 1;
    static constexpr int ANIMATION_TRACK = 2;
    static constexpr int APPEARANCE = 3;
    static constexpr int BACKGROUND = 4;
    static constexpr int CAMERA = 5;
    static constexpr int COMPOSITING_MODE = 6;
    static constexpr int FOG = 7;
    static constexpr int POLYGON_MODE = 8;
    static constexpr int GROUP = 9;
    static constexpr int IMAGE_2D = 10;
    static constexpr int TRIANGLE_STRIP_ARRAY = 11;
    static constexpr int LIGHT = 12;
    static constexpr int MATERIAL = 13;
    static constexpr int MESH = 14;
    static constexpr int MORPHING_MESH = 15;
    static constexpr int SKINNED_MESH = 16;
    static constexpr int TEXTURE_2D = 17;
    static constexpr int SPRITE_3D = 18;
    static constexpr int KEYFRAME_SEQUENCE = 19;
    static constexpr int VERTEX_ARRAY = 20;
    static constexpr int VERTEX_BUFFER = 21;
    static constexpr int WORLD = 22;
    static constexpr int EXTERNAL_REFERENCE = 0xFF;
};

inline std::string type_name_for_object_type(int object_type) {
    switch (object_type) {
    case ObjectTypes::HEADER:
        return "Header";
    case ObjectTypes::ANIMATION_CONTROLLER:
        return "AnimationController";
    case ObjectTypes::ANIMATION_TRACK:
        return "AnimationTrack";
    case ObjectTypes::APPEARANCE:
        return "Appearance";
    case ObjectTypes::BACKGROUND:
        return "Background";
    case ObjectTypes::CAMERA:
        return "Camera";
    case ObjectTypes::COMPOSITING_MODE:
        return "CompositingMode";
    case ObjectTypes::FOG:
        return "Fog";
    case ObjectTypes::POLYGON_MODE:
        return "PolygonMode";
    case ObjectTypes::GROUP:
        return "Group";
    case ObjectTypes::IMAGE_2D:
        return "Image2D";
    case ObjectTypes::TRIANGLE_STRIP_ARRAY:
        return "TriangleStripArray";
    case ObjectTypes::LIGHT:
        return "Light";
    case ObjectTypes::MATERIAL:
        return "Material";
    case ObjectTypes::MESH:
        return "Mesh";
    case ObjectTypes::MORPHING_MESH:
        return "MorphingMesh";
    case ObjectTypes::SKINNED_MESH:
        return "SkinnedMesh";
    case ObjectTypes::TEXTURE_2D:
        return "Texture2D";
    case ObjectTypes::SPRITE_3D:
        return "Sprite3D";
    case ObjectTypes::KEYFRAME_SEQUENCE:
        return "KeyframeSequence";
    case ObjectTypes::VERTEX_ARRAY:
        return "VertexArray";
    case ObjectTypes::VERTEX_BUFFER:
        return "VertexBuffer";
    case ObjectTypes::WORLD:
        return "World";
    case ObjectTypes::EXTERNAL_REFERENCE:
        return "ExternalReference";
    default:
        return "Type" + std::to_string(object_type);
    }
}

struct RgbColor {
    int red = 0;
    int green = 0;
    int blue = 0;
    std::vector<float> to_float_array() const {
        return {red / 255.f, green / 255.f, blue / 255.f};
    }
};

struct RgbaColor {
    int red = 0;
    int green = 0;
    int blue = 0;
    int alpha = 255;
    std::vector<float> to_float_array() const {
        return {red / 255.f, green / 255.f, blue / 255.f, alpha / 255.f};
    }
};

struct SectionInfo {
    int index = 0;
    int compression_scheme = 0;
    int total_section_length = 0;
    int uncompressed_length = 0;
};

struct Object {
    int object_id = 0;
    int object_type = 0;
    int raw_length = 0;
    virtual ~Object() = default;
    std::string type_name() const { return type_name_for_object_type(object_type); }
};

struct HeaderObject : Object {
    int version_major = 0;
    int version_minor = 0;
    bool has_external_references = false;
    std::uint32_t total_file_size = 0;
    std::uint32_t approximate_content_size = 0;
    std::string authoring_field;
};

struct ExternalReferenceObject : Object {
    std::string uri;
};

struct Object3DMeta {
    int user_id = 0;
    std::vector<int> animation_track_ids;
    int user_parameter_count = 0;
};

struct ComponentTransform {
    std::vector<float> translation{0, 0, 0};
    std::vector<float> scale{1, 1, 1};
    float orientation_angle = 0.f;
    std::vector<float> orientation_axis{0, 0, 1};
};

struct TransformableMeta {
    Object3DMeta object3d;
    std::optional<ComponentTransform> component_transform;
    std::optional<std::vector<float>> general_transform;
};

struct Alignment {
    int z_target = 0;
    int y_target = 0;
    std::optional<int> z_reference_id;
    std::optional<int> y_reference_id;
};

struct NodeMeta {
    TransformableMeta transformable;
    bool enable_rendering = true;
    bool enable_picking = true;
    int alpha_factor = 255;
    std::uint32_t scope = 0;
    std::optional<Alignment> alignment;
};

struct NodeObject : virtual Object {
    NodeMeta node_meta;
};

struct GroupLikeObject : NodeObject {
    std::vector<int> child_ids;
};

struct GroupObject : GroupLikeObject {
    GroupObject() { object_type = ObjectTypes::GROUP; }
};

struct WorldObject : GroupLikeObject {
    WorldObject() { object_type = ObjectTypes::WORLD; }
    std::optional<int> active_camera_id;
    std::optional<int> background_id;
};

struct PerspectiveProjection {
    float field_of_view_degrees = 45.f;
    float aspect_ratio = 1.f;
    float near_distance = 0.1f;
    float far_distance = 100.f;
};

struct GenericProjection {
    int projection_type = 0;
    std::vector<float> values;
};

struct CameraObject : NodeObject {
    CameraObject() { object_type = ObjectTypes::CAMERA; }
    int projection_type = 0;
    // If perspective is set, use it; else generic.
    std::optional<PerspectiveProjection> perspective;
    std::optional<GenericProjection> generic;
};

struct LightObject : NodeObject {
    LightObject() { object_type = ObjectTypes::LIGHT; }
    float attenuation_constant = 1.f;
    float attenuation_linear = 0.f;
    float attenuation_quadratic = 0.f;
    RgbColor color;
    int mode = 0;
    float intensity = 1.f;
    float spot_angle = 0.f;
    float spot_exponent = 0.f;
};

struct BackgroundObject : Object {
    BackgroundObject() { object_type = ObjectTypes::BACKGROUND; }
    Object3DMeta object3d;
    RgbaColor background_color;
    std::optional<int> background_image_id;
    int image_mode_x = 0;
    int image_mode_y = 0;
    int crop_x = 0;
    int crop_y = 0;
    int crop_width = 0;
    int crop_height = 0;
    bool depth_clear_enabled = true;
    bool color_clear_enabled = true;
};

struct FogObject : Object {
    FogObject() { object_type = ObjectTypes::FOG; }
    Object3DMeta object3d;
    RgbColor color;
    int mode = 0;
    float density = 0.f;
    float near_distance = 0.f;
    float far_distance = 0.f;
};

struct PolygonModeObject : Object {
    PolygonModeObject() { object_type = ObjectTypes::POLYGON_MODE; }
    Object3DMeta object3d;
    int culling = 0;
    int shading = 0;
    int winding = 0;
    bool two_sided_lighting_enabled = false;
    bool local_camera_lighting_enabled = false;
    bool perspective_correction_enabled = false;
};

struct MaterialObject : Object {
    MaterialObject() { object_type = ObjectTypes::MATERIAL; }
    Object3DMeta object3d;
    RgbColor ambient_color;
    RgbaColor diffuse_color;
    RgbColor emissive_color;
    RgbColor specular_color;
    float shininess = 0.f;
    bool vertex_color_tracking_enabled = false;
};

struct VertexArrayObject : Object {
    VertexArrayObject() { object_type = ObjectTypes::VERTEX_ARRAY; }
    Object3DMeta object3d;
    int component_size = 0;
    int component_count = 0;
    int encoding = 0;
    int vertex_count = 0;
    std::vector<int> components;
};

struct TexCoordBinding {
    std::optional<int> vertex_array_id;
    std::vector<float> bias{0, 0, 0};
    float scale = 1.f;
};

struct VertexBufferObject : Object {
    VertexBufferObject() { object_type = ObjectTypes::VERTEX_BUFFER; }
    Object3DMeta object3d;
    RgbaColor default_color;
    std::optional<int> positions_id;
    std::vector<float> position_bias{0, 0, 0};
    float position_scale = 1.f;
    std::optional<int> normals_id;
    std::optional<int> colors_id;
    std::vector<TexCoordBinding> tex_coord_bindings;
};

struct TriangleStripArrayObject : Object {
    TriangleStripArrayObject() { object_type = ObjectTypes::TRIANGLE_STRIP_ARRAY; }
    Object3DMeta object3d;
    int encoding = 0;
    std::vector<int> indices;
    std::vector<int> strip_lengths;
};

struct AppearanceObject : Object {
    AppearanceObject() { object_type = ObjectTypes::APPEARANCE; }
    Object3DMeta object3d;
    int layer = 0;
    std::optional<int> compositing_mode_id;
    std::optional<int> fog_id;
    std::optional<int> polygon_mode_id;
    std::optional<int> material_id;
    std::vector<int> texture_ids;
};

struct Texture2DObject : Object {
    Texture2DObject() { object_type = ObjectTypes::TEXTURE_2D; }
    TransformableMeta transformable;
    std::optional<int> image_id;
    RgbColor blend_color;
    int blending = 0;
    int wrapping_s = 0;
    int wrapping_t = 0;
    int level_filter = 0;
    int image_filter = 0;
};

struct Image2DObject : Object {
    Image2DObject() { object_type = ObjectTypes::IMAGE_2D; }
    Object3DMeta object3d;
    int format = 0;
    bool is_mutable = false;
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> palette;
    std::optional<std::vector<std::uint8_t>> pixels;
};

struct SubmeshRef {
    std::optional<int> index_buffer_id;
    std::optional<int> appearance_id;
};

struct MeshLikeObject : NodeObject {
    std::optional<int> vertex_buffer_id;
    std::vector<SubmeshRef> submeshes;
};

struct MeshObject : MeshLikeObject {
    MeshObject() { object_type = ObjectTypes::MESH; }
};

struct SkinnedMeshBoneTransform {
    std::optional<int> transform_node_id;
    int first_vertex = 0;
    int vertex_count = 0;
    int weight = 0;
};

struct SkinnedMeshObject : MeshLikeObject {
    SkinnedMeshObject() { object_type = ObjectTypes::SKINNED_MESH; }
    std::optional<int> skeleton_id;
    std::vector<SkinnedMeshBoneTransform> bone_transforms;
};

struct AnimationControllerObject : Object {
    AnimationControllerObject() { object_type = ObjectTypes::ANIMATION_CONTROLLER; }
    Object3DMeta object3d;
    float speed = 1.f;
    float weight = 1.f;
    int active_interval_start = 0;
    int active_interval_end = 0;
    float reference_sequence_time = 0.f;
    int reference_world_time = 0;
};

struct AnimationTrackObject : Object {
    AnimationTrackObject() { object_type = ObjectTypes::ANIMATION_TRACK; }
    Object3DMeta object3d;
    std::optional<int> keyframe_sequence_id;
    std::optional<int> animation_controller_id;
    int property_id = 0;
};

struct Keyframe {
    int time = 0;
    std::vector<float> values;
};

struct KeyframeSequenceObject : Object {
    KeyframeSequenceObject() { object_type = ObjectTypes::KEYFRAME_SEQUENCE; }
    Object3DMeta object3d;
    int interpolation = 0;
    int repeat_mode = 0;
    int encoding = 0;
    int duration = 0;
    int valid_range_first = 0;
    int valid_range_last = 0;
    int component_count = 0;
    std::vector<Keyframe> keyframes;
};

struct UnknownObject : Object {
    std::vector<std::uint8_t> raw_data;
};

struct File {
    std::shared_ptr<HeaderObject> header;
    std::vector<SectionInfo> sections;
    // Insertion-ordered map by sequential object ids.
    std::map<int, std::shared_ptr<Object>> objects_by_id;

    std::vector<std::shared_ptr<Object>> objects_in_order() const {
        std::vector<std::shared_ptr<Object>> out;
        out.reserve(objects_by_id.size());
        for (const auto &kv : objects_by_id) {
            out.push_back(kv.second);
        }
        return out;
    }

    std::shared_ptr<WorldObject> world_or_null() const {
        for (const auto &obj : objects_in_order()) {
            if (auto w = std::dynamic_pointer_cast<WorldObject>(obj)) {
                return w;
            }
        }
        return nullptr;
    }

    std::shared_ptr<WorldObject> require_world() const {
        auto w = world_or_null();
        if (!w) {
            throw std::runtime_error("No World object found in parsed M3G file");
        }
        return w;
    }

    std::shared_ptr<Object> object_or_null(std::optional<int> object_id) const {
        if (!object_id || *object_id == 0) {
            return nullptr;
        }
        auto it = objects_by_id.find(*object_id);
        if (it == objects_by_id.end()) {
            return nullptr;
        }
        return it->second;
    }
};

inline bool is_identity_row_major(const std::vector<float> &m, float epsilon = 1e-5f) {
    static const float identity[16] = {
        1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1,
    };
    if (m.size() != 16) {
        return false;
    }
    for (int i = 0; i < 16; ++i) {
        if (std::fabs(m[static_cast<std::size_t>(i)] - identity[i]) > epsilon) {
            return false;
        }
    }
    return true;
}

} // namespace m3g
} // namespace slop
