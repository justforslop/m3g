#include "m3g/m3g_parser.hpp"

#include "m3g/binary_reader.hpp"

#include <cstring>
#include <fstream>
#include <stdexcept>

#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include "miniz.h"

namespace slop {
namespace m3g {
namespace {

const std::uint8_t FILE_IDENTIFIER[12] = {
    0xAB, 0x4A, 0x53, 0x52, 0x31, 0x38, 0x34, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A,
};

int to_checked_int(std::uint32_t value, const char *label) {
    if (value > static_cast<std::uint32_t>(0x7FFFFFFF)) {
        throw std::runtime_error(std::string(label) + " out of Int range: " + std::to_string(value));
    }
    return static_cast<int>(value);
}

void write_u32_le(std::uint8_t *target, int offset, int value) {
    target[offset] = static_cast<std::uint8_t>(value & 0xFF);
    target[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
    target[offset + 2] = static_cast<std::uint8_t>((value >> 16) & 0xFF);
    target[offset + 3] = static_cast<std::uint8_t>((value >> 24) & 0xFF);
}

std::vector<float> read_float_array(BinaryReader &reader, int count) {
    std::vector<float> values(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        values[static_cast<std::size_t>(i)] = reader.read_f32_le();
    }
    return values;
}

RgbColor read_rgb(BinaryReader &reader) {
    return RgbColor{reader.read_u8(), reader.read_u8(), reader.read_u8()};
}

RgbaColor read_rgba(BinaryReader &reader) {
    return RgbaColor{reader.read_u8(), reader.read_u8(), reader.read_u8(), reader.read_u8()};
}

int read_object_ref(BinaryReader &reader) {
    return to_checked_int(reader.read_u32_le(), "object reference");
}

std::optional<int> read_object_ref_nullable(BinaryReader &reader) {
    const int object_id = read_object_ref(reader);
    if (object_id == 0) {
        return std::nullopt;
    }
    return object_id;
}

Object3DMeta read_object3d_meta(BinaryReader &reader) {
    Object3DMeta meta;
    meta.user_id = to_checked_int(reader.read_u32_le(), "user ID");
    const int animation_track_count = to_checked_int(reader.read_u32_le(), "animation track count");
    meta.animation_track_ids.reserve(static_cast<std::size_t>(animation_track_count));
    for (int i = 0; i < animation_track_count; ++i) {
        meta.animation_track_ids.push_back(read_object_ref(reader));
    }
    meta.user_parameter_count = to_checked_int(reader.read_u32_le(), "user parameter count");
    return meta;
}

TransformableMeta read_transformable_meta(BinaryReader &reader) {
    TransformableMeta meta;
    meta.object3d = read_object3d_meta(reader);
    if (reader.read_bool_byte()) {
        ComponentTransform ct;
        ct.translation = read_float_array(reader, 3);
        ct.scale = read_float_array(reader, 3);
        ct.orientation_angle = reader.read_f32_le();
        ct.orientation_axis = read_float_array(reader, 3);
        meta.component_transform = ct;
    }
    if (reader.read_bool_byte()) {
        meta.general_transform = read_float_array(reader, 16);
    }
    return meta;
}

NodeMeta read_node_meta(BinaryReader &reader) {
    NodeMeta meta;
    meta.transformable = read_transformable_meta(reader);
    meta.enable_rendering = reader.read_bool_byte();
    meta.enable_picking = reader.read_bool_byte();
    meta.alpha_factor = reader.read_u8();
    meta.scope = reader.read_u32_le();
    if (reader.read_bool_byte()) {
        Alignment alignment;
        alignment.z_target = reader.read_u8();
        alignment.y_target = reader.read_u8();
        alignment.z_reference_id = read_object_ref_nullable(reader);
        alignment.y_reference_id = read_object_ref_nullable(reader);
        meta.alignment = alignment;
    }
    return meta;
}

std::vector<int> build_implicit_triangle_strip_indices(int start_index, const std::vector<int> &strip_lengths) {
    int total = 0;
    for (int len : strip_lengths) {
        total += len;
    }
    std::vector<int> indices(static_cast<std::size_t>(total));
    int next = start_index;
    for (int &idx : indices) {
        idx = next++;
    }
    return indices;
}

std::shared_ptr<Object> parse_header(int object_id, int raw_length, BinaryReader &reader) {
    auto obj = std::make_shared<HeaderObject>();
    obj->object_id = object_id;
    obj->object_type = ObjectTypes::HEADER;
    obj->raw_length = raw_length;
    obj->version_major = reader.read_u8();
    obj->version_minor = reader.read_u8();
    obj->has_external_references = reader.read_bool_byte();
    obj->total_file_size = reader.read_u32_le();
    obj->approximate_content_size = reader.read_u32_le();
    obj->authoring_field = reader.read_cstring();
    return obj;
}

std::shared_ptr<Object> parse_group(int object_id, int raw_length, BinaryReader &reader) {
    auto obj = std::make_shared<GroupObject>();
    obj->object_id = object_id;
    obj->raw_length = raw_length;
    obj->node_meta = read_node_meta(reader);
    const int child_count = to_checked_int(reader.read_u32_le(), "group child count");
    for (int i = 0; i < child_count; ++i) {
        obj->child_ids.push_back(read_object_ref(reader));
    }
    return obj;
}

std::shared_ptr<Object> parse_world(int object_id, int raw_length, BinaryReader &reader) {
    auto obj = std::make_shared<WorldObject>();
    obj->object_id = object_id;
    obj->raw_length = raw_length;
    obj->node_meta = read_node_meta(reader);
    const int child_count = to_checked_int(reader.read_u32_le(), "world child count");
    for (int i = 0; i < child_count; ++i) {
        obj->child_ids.push_back(read_object_ref(reader));
    }
    obj->active_camera_id = read_object_ref_nullable(reader);
    obj->background_id = read_object_ref_nullable(reader);
    return obj;
}

std::shared_ptr<Object> parse_camera(int object_id, int raw_length, BinaryReader &reader) {
    auto obj = std::make_shared<CameraObject>();
    obj->object_id = object_id;
    obj->raw_length = raw_length;
    obj->node_meta = read_node_meta(reader);
    obj->projection_type = reader.read_u8();
    if (obj->projection_type == 48) {
        GenericProjection g;
        g.projection_type = obj->projection_type;
        g.values = read_float_array(reader, 16);
        obj->generic = g;
    } else {
        auto values = read_float_array(reader, 4);
        if (obj->projection_type == 50) {
            PerspectiveProjection p;
            p.field_of_view_degrees = values[0];
            p.aspect_ratio = values[1];
            p.near_distance = values[2];
            p.far_distance = values[3];
            obj->perspective = p;
        } else {
            GenericProjection g;
            g.projection_type = obj->projection_type;
            g.values = std::move(values);
            obj->generic = g;
        }
    }
    return obj;
}

std::shared_ptr<Object> parse_light(int object_id, int raw_length, BinaryReader &reader) {
    auto obj = std::make_shared<LightObject>();
    obj->object_id = object_id;
    obj->raw_length = raw_length;
    obj->node_meta = read_node_meta(reader);
    obj->attenuation_constant = reader.read_f32_le();
    obj->attenuation_linear = reader.read_f32_le();
    obj->attenuation_quadratic = reader.read_f32_le();
    obj->color = read_rgb(reader);
    obj->mode = reader.read_u8();
    obj->intensity = reader.read_f32_le();
    obj->spot_angle = reader.read_f32_le();
    obj->spot_exponent = reader.read_f32_le();
    return obj;
}

std::shared_ptr<Object> parse_background(int object_id, int raw_length, BinaryReader &reader) {
    auto obj = std::make_shared<BackgroundObject>();
    obj->object_id = object_id;
    obj->raw_length = raw_length;
    obj->object3d = read_object3d_meta(reader);
    obj->background_color = read_rgba(reader);
    obj->background_image_id = read_object_ref_nullable(reader);
    obj->image_mode_x = reader.read_u8();
    obj->image_mode_y = reader.read_u8();
    obj->crop_x = reader.read_i32_le();
    obj->crop_y = reader.read_i32_le();
    obj->crop_width = reader.read_i32_le();
    obj->crop_height = reader.read_i32_le();
    obj->depth_clear_enabled = reader.read_bool_byte();
    obj->color_clear_enabled = reader.read_bool_byte();
    return obj;
}

std::shared_ptr<Object> parse_fog(int object_id, int raw_length, BinaryReader &reader) {
    auto obj = std::make_shared<FogObject>();
    obj->object_id = object_id;
    obj->raw_length = raw_length;
    obj->object3d = read_object3d_meta(reader);
    obj->color = read_rgb(reader);
    obj->mode = reader.read_u8();
    obj->density = reader.read_f32_le();
    obj->near_distance = reader.read_f32_le();
    obj->far_distance = reader.read_f32_le();
    return obj;
}

std::shared_ptr<Object> parse_polygon_mode(int object_id, int raw_length, BinaryReader &reader) {
    auto obj = std::make_shared<PolygonModeObject>();
    obj->object_id = object_id;
    obj->raw_length = raw_length;
    obj->object3d = read_object3d_meta(reader);
    obj->culling = reader.read_u8();
    obj->shading = reader.read_u8();
    obj->winding = reader.read_u8();
    obj->two_sided_lighting_enabled = reader.read_bool_byte();
    obj->local_camera_lighting_enabled = reader.read_bool_byte();
    obj->perspective_correction_enabled = reader.read_bool_byte();
    return obj;
}

std::shared_ptr<Object> parse_material(int object_id, int raw_length, BinaryReader &reader) {
    auto obj = std::make_shared<MaterialObject>();
    obj->object_id = object_id;
    obj->raw_length = raw_length;
    obj->object3d = read_object3d_meta(reader);
    obj->ambient_color = read_rgb(reader);
    obj->diffuse_color = read_rgba(reader);
    obj->emissive_color = read_rgb(reader);
    obj->specular_color = read_rgb(reader);
    obj->shininess = reader.read_f32_le();
    obj->vertex_color_tracking_enabled = reader.read_bool_byte();
    return obj;
}

std::shared_ptr<Object> parse_vertex_array(int object_id, int raw_length, BinaryReader &reader) {
    auto obj = std::make_shared<VertexArrayObject>();
    obj->object_id = object_id;
    obj->raw_length = raw_length;
    obj->object3d = read_object3d_meta(reader);
    obj->component_size = reader.read_u8();
    obj->component_count = reader.read_u8();
    obj->encoding = reader.read_u8();
    obj->vertex_count = reader.read_u16_le();
    const int total = obj->vertex_count * obj->component_count;
    obj->components.resize(static_cast<std::size_t>(total));
    for (int i = 0; i < total; ++i) {
        if (obj->component_size == 1) {
            obj->components[static_cast<std::size_t>(i)] = reader.read_i8();
        } else if (obj->component_size == 2) {
            obj->components[static_cast<std::size_t>(i)] = reader.read_i16_le();
        } else {
            throw std::runtime_error("Unsupported VertexArray component size " + std::to_string(obj->component_size));
        }
    }
    return obj;
}

std::shared_ptr<Object> parse_vertex_buffer(int object_id, int raw_length, BinaryReader &reader) {
    auto obj = std::make_shared<VertexBufferObject>();
    obj->object_id = object_id;
    obj->raw_length = raw_length;
    obj->object3d = read_object3d_meta(reader);
    obj->default_color = read_rgba(reader);
    obj->positions_id = read_object_ref_nullable(reader);
    obj->position_bias = read_float_array(reader, 3);
    obj->position_scale = reader.read_f32_le();
    obj->normals_id = read_object_ref_nullable(reader);
    obj->colors_id = read_object_ref_nullable(reader);
    const int tex_coord_count = to_checked_int(reader.read_u32_le(), "tex coord array count");
    for (int i = 0; i < tex_coord_count; ++i) {
        TexCoordBinding binding;
        binding.vertex_array_id = read_object_ref_nullable(reader);
        binding.bias = read_float_array(reader, 3);
        binding.scale = reader.read_f32_le();
        obj->tex_coord_bindings.push_back(std::move(binding));
    }
    return obj;
}

std::shared_ptr<Object> parse_triangle_strip_array(int object_id, int raw_length, BinaryReader &reader) {
    auto obj = std::make_shared<TriangleStripArrayObject>();
    obj->object_id = object_id;
    obj->raw_length = raw_length;
    obj->object3d = read_object3d_meta(reader);
    obj->encoding = reader.read_u8();

    auto finish_implicit = [&](int start_index) {
        const int strip_count = to_checked_int(reader.read_u32_le(), "strip count");
        obj->strip_lengths.resize(static_cast<std::size_t>(strip_count));
        for (int i = 0; i < strip_count; ++i) {
            obj->strip_lengths[static_cast<std::size_t>(i)] =
                to_checked_int(reader.read_u32_le(), "strip length");
        }
        obj->indices = build_implicit_triangle_strip_indices(start_index, obj->strip_lengths);
    };

    switch (obj->encoding) {
    case 0:
        finish_implicit(to_checked_int(reader.read_u32_le(), "strip startIndex"));
        break;
    case 1:
        finish_implicit(reader.read_u8());
        break;
    case 2:
        finish_implicit(reader.read_u16_le());
        break;
    case 128: {
        const int index_count = to_checked_int(reader.read_u32_le(), "index count");
        obj->indices.resize(static_cast<std::size_t>(index_count));
        for (int i = 0; i < index_count; ++i) {
            obj->indices[static_cast<std::size_t>(i)] = to_checked_int(reader.read_u32_le(), "strip index");
        }
        break;
    }
    case 129: {
        const int index_count = to_checked_int(reader.read_u32_le(), "index count");
        obj->indices.resize(static_cast<std::size_t>(index_count));
        for (int i = 0; i < index_count; ++i) {
            obj->indices[static_cast<std::size_t>(i)] = reader.read_u8();
        }
        break;
    }
    case 130: {
        const int index_count = to_checked_int(reader.read_u32_le(), "index count");
        obj->indices.resize(static_cast<std::size_t>(index_count));
        for (int i = 0; i < index_count; ++i) {
            obj->indices[static_cast<std::size_t>(i)] = reader.read_u16_le();
        }
        break;
    }
    default:
        throw std::runtime_error("Unsupported TriangleStripArray encoding " + std::to_string(obj->encoding));
    }

    if (obj->encoding >= 128) {
        const int strip_count = to_checked_int(reader.read_u32_le(), "strip count");
        obj->strip_lengths.resize(static_cast<std::size_t>(strip_count));
        for (int i = 0; i < strip_count; ++i) {
            obj->strip_lengths[static_cast<std::size_t>(i)] =
                to_checked_int(reader.read_u32_le(), "strip length");
        }
    }
    return obj;
}

std::shared_ptr<Object> parse_appearance(int object_id, int raw_length, BinaryReader &reader) {
    auto obj = std::make_shared<AppearanceObject>();
    obj->object_id = object_id;
    obj->raw_length = raw_length;
    obj->object3d = read_object3d_meta(reader);
    obj->layer = reader.read_u8();
    obj->compositing_mode_id = read_object_ref_nullable(reader);
    obj->fog_id = read_object_ref_nullable(reader);
    obj->polygon_mode_id = read_object_ref_nullable(reader);
    obj->material_id = read_object_ref_nullable(reader);
    const int texture_count = to_checked_int(reader.read_u32_le(), "appearance texture count");
    for (int i = 0; i < texture_count; ++i) {
        obj->texture_ids.push_back(read_object_ref(reader));
    }
    return obj;
}

std::shared_ptr<Object> parse_texture2d(int object_id, int raw_length, BinaryReader &reader) {
    auto obj = std::make_shared<Texture2DObject>();
    obj->object_id = object_id;
    obj->raw_length = raw_length;
    obj->transformable = read_transformable_meta(reader);
    obj->image_id = read_object_ref_nullable(reader);
    obj->blend_color = read_rgb(reader);
    obj->blending = reader.read_u8();
    obj->wrapping_s = reader.read_u8();
    obj->wrapping_t = reader.read_u8();
    obj->level_filter = reader.read_u8();
    obj->image_filter = reader.read_u8();
    return obj;
}

std::shared_ptr<Object> parse_image2d(int object_id, int raw_length, BinaryReader &reader) {
    auto obj = std::make_shared<Image2DObject>();
    obj->object_id = object_id;
    obj->raw_length = raw_length;
    obj->object3d = read_object3d_meta(reader);
    obj->format = reader.read_u8();
    obj->is_mutable = reader.read_bool_byte();
    obj->width = to_checked_int(reader.read_u32_le(), "image width");
    obj->height = to_checked_int(reader.read_u32_le(), "image height");
    if (!obj->is_mutable) {
        const int palette_length = to_checked_int(reader.read_u32_le(), "image palette length");
        if (palette_length > 0) {
            obj->palette = reader.read_bytes(static_cast<std::size_t>(palette_length));
        }
        if (reader.remaining() >= 4) {
            const int pixel_length = to_checked_int(reader.read_u32_le(), "pixel length");
            obj->pixels = reader.read_bytes(static_cast<std::size_t>(pixel_length));
        } else if (reader.remaining() > 0) {
            obj->pixels = reader.read_remaining_bytes();
        }
    }
    return obj;
}

std::shared_ptr<Object> parse_mesh(int object_id, int raw_length, BinaryReader &reader) {
    auto obj = std::make_shared<MeshObject>();
    obj->object_id = object_id;
    obj->raw_length = raw_length;
    obj->node_meta = read_node_meta(reader);
    obj->vertex_buffer_id = read_object_ref_nullable(reader);
    const int submesh_count = to_checked_int(reader.read_u32_le(), "submesh count");
    for (int i = 0; i < submesh_count; ++i) {
        SubmeshRef ref;
        ref.index_buffer_id = read_object_ref_nullable(reader);
        ref.appearance_id = read_object_ref_nullable(reader);
        obj->submeshes.push_back(ref);
    }
    return obj;
}

std::shared_ptr<Object> parse_skinned_mesh(int object_id, int raw_length, BinaryReader &reader) {
    auto obj = std::make_shared<SkinnedMeshObject>();
    obj->object_id = object_id;
    obj->raw_length = raw_length;
    obj->node_meta = read_node_meta(reader);
    obj->vertex_buffer_id = read_object_ref_nullable(reader);
    const int submesh_count = to_checked_int(reader.read_u32_le(), "submesh count");
    for (int i = 0; i < submesh_count; ++i) {
        SubmeshRef ref;
        ref.index_buffer_id = read_object_ref_nullable(reader);
        ref.appearance_id = read_object_ref_nullable(reader);
        obj->submeshes.push_back(ref);
    }
    obj->skeleton_id = read_object_ref_nullable(reader);
    const int transform_count = to_checked_int(reader.read_u32_le(), "bone transform count");
    for (int i = 0; i < transform_count; ++i) {
        SkinnedMeshBoneTransform bt;
        bt.transform_node_id = read_object_ref_nullable(reader);
        bt.first_vertex = to_checked_int(reader.read_u32_le(), "bone first vertex");
        bt.vertex_count = to_checked_int(reader.read_u32_le(), "bone vertex count");
        bt.weight = reader.read_i32_le();
        obj->bone_transforms.push_back(bt);
    }
    return obj;
}

std::shared_ptr<Object> parse_animation_controller(int object_id, int raw_length, BinaryReader &reader) {
    auto obj = std::make_shared<AnimationControllerObject>();
    obj->object_id = object_id;
    obj->raw_length = raw_length;
    obj->object3d = read_object3d_meta(reader);
    obj->speed = reader.read_f32_le();
    obj->weight = reader.read_f32_le();
    obj->active_interval_start = reader.read_i32_le();
    obj->active_interval_end = reader.read_i32_le();
    obj->reference_sequence_time = reader.read_f32_le();
    obj->reference_world_time = reader.read_i32_le();
    return obj;
}

std::shared_ptr<Object> parse_animation_track(int object_id, int raw_length, BinaryReader &reader) {
    auto obj = std::make_shared<AnimationTrackObject>();
    obj->object_id = object_id;
    obj->raw_length = raw_length;
    obj->object3d = read_object3d_meta(reader);
    obj->keyframe_sequence_id = read_object_ref_nullable(reader);
    obj->animation_controller_id = read_object_ref_nullable(reader);
    obj->property_id = to_checked_int(reader.read_u32_le(), "animation property id");
    return obj;
}

std::shared_ptr<Object> parse_keyframe_sequence(int object_id, int raw_length, BinaryReader &reader) {
    auto obj = std::make_shared<KeyframeSequenceObject>();
    obj->object_id = object_id;
    obj->raw_length = raw_length;
    obj->object3d = read_object3d_meta(reader);
    obj->interpolation = reader.read_u8();
    obj->repeat_mode = reader.read_u8();
    obj->encoding = reader.read_u8();
    obj->duration = to_checked_int(reader.read_u32_le(), "sequence duration");
    obj->valid_range_first = to_checked_int(reader.read_u32_le(), "sequence validRangeFirst");
    obj->valid_range_last = to_checked_int(reader.read_u32_le(), "sequence validRangeLast");
    obj->component_count = to_checked_int(reader.read_u32_le(), "sequence componentCount");
    const int keyframe_count = to_checked_int(reader.read_u32_le(), "sequence keyframeCount");
    if (obj->encoding == 0) {
        for (int i = 0; i < keyframe_count; ++i) {
            Keyframe kf;
            kf.time = reader.read_i32_le();
            kf.values = read_float_array(reader, obj->component_count);
            obj->keyframes.push_back(std::move(kf));
        }
    } else if (obj->encoding == 1 || obj->encoding == 2) {
        const auto bias = read_float_array(reader, obj->component_count);
        const float scale = reader.read_f32_le();
        for (int i = 0; i < keyframe_count; ++i) {
            Keyframe kf;
            kf.time = reader.read_i32_le();
            kf.values.resize(static_cast<std::size_t>(obj->component_count));
            for (int c = 0; c < obj->component_count; ++c) {
                const float raw = obj->encoding == 1 ? static_cast<float>(reader.read_i16_le())
                                                     : static_cast<float>(reader.read_i8());
                kf.values[static_cast<std::size_t>(c)] =
                    raw * scale + bias[static_cast<std::size_t>(c)];
            }
            obj->keyframes.push_back(std::move(kf));
        }
    } else {
        throw std::runtime_error("Unsupported KeyframeSequence encoding " + std::to_string(obj->encoding));
    }
    return obj;
}

std::shared_ptr<Object> parse_object(int object_id, int object_type, int raw_length, BinaryReader &reader,
                                    const std::vector<std::uint8_t> &raw_payload) {
    switch (object_type) {
    case ObjectTypes::HEADER:
        return parse_header(object_id, raw_length, reader);
    case ObjectTypes::EXTERNAL_REFERENCE: {
        auto obj = std::make_shared<ExternalReferenceObject>();
        obj->object_id = object_id;
        obj->object_type = ObjectTypes::EXTERNAL_REFERENCE;
        obj->raw_length = raw_length;
        obj->uri = reader.read_cstring();
        return obj;
    }
    case ObjectTypes::WORLD:
        return parse_world(object_id, raw_length, reader);
    case ObjectTypes::GROUP:
        return parse_group(object_id, raw_length, reader);
    case ObjectTypes::CAMERA:
        return parse_camera(object_id, raw_length, reader);
    case ObjectTypes::LIGHT:
        return parse_light(object_id, raw_length, reader);
    case ObjectTypes::BACKGROUND:
        return parse_background(object_id, raw_length, reader);
    case ObjectTypes::FOG:
        return parse_fog(object_id, raw_length, reader);
    case ObjectTypes::POLYGON_MODE:
        return parse_polygon_mode(object_id, raw_length, reader);
    case ObjectTypes::MATERIAL:
        return parse_material(object_id, raw_length, reader);
    case ObjectTypes::VERTEX_ARRAY:
        return parse_vertex_array(object_id, raw_length, reader);
    case ObjectTypes::VERTEX_BUFFER:
        return parse_vertex_buffer(object_id, raw_length, reader);
    case ObjectTypes::TRIANGLE_STRIP_ARRAY:
        return parse_triangle_strip_array(object_id, raw_length, reader);
    case ObjectTypes::APPEARANCE:
        return parse_appearance(object_id, raw_length, reader);
    case ObjectTypes::TEXTURE_2D:
        return parse_texture2d(object_id, raw_length, reader);
    case ObjectTypes::IMAGE_2D:
        return parse_image2d(object_id, raw_length, reader);
    case ObjectTypes::MESH:
        return parse_mesh(object_id, raw_length, reader);
    case ObjectTypes::SKINNED_MESH:
        return parse_skinned_mesh(object_id, raw_length, reader);
    case ObjectTypes::ANIMATION_CONTROLLER:
        return parse_animation_controller(object_id, raw_length, reader);
    case ObjectTypes::ANIMATION_TRACK:
        return parse_animation_track(object_id, raw_length, reader);
    case ObjectTypes::KEYFRAME_SEQUENCE:
        return parse_keyframe_sequence(object_id, raw_length, reader);
    default: {
        auto obj = std::make_shared<UnknownObject>();
        obj->object_id = object_id;
        obj->object_type = object_type;
        obj->raw_length = raw_length;
        obj->raw_data = raw_payload;
        return obj;
    }
    }
}

} // namespace

File Parser::parse_path(const std::string &path) const {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Failed to open M3G file: " + path);
    }
    in.seekg(0, std::ios::end);
    const auto len = in.tellg();
    in.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(len));
    if (len > 0) {
        in.read(reinterpret_cast<char *>(bytes.data()), len);
    }
    return parse(bytes);
}

File Parser::parse(const std::vector<std::uint8_t> &bytes) const {
    BinaryReader reader(bytes, "m3g-file");
    auto identifier = reader.read_bytes(sizeof(FILE_IDENTIFIER));
    if (identifier.size() != sizeof(FILE_IDENTIFIER) ||
        std::memcmp(identifier.data(), FILE_IDENTIFIER, sizeof(FILE_IDENTIFIER)) != 0) {
        throw std::runtime_error("Invalid M3G file identifier");
    }

    File file;
    int next_object_id = 1;

    while (!reader.is_eof()) {
        const int section_index = static_cast<int>(file.sections.size());
        const int compression_scheme = reader.read_u8();
        const int total_section_length = to_checked_int(reader.read_u32_le(), "section total length");
        const int uncompressed_length = to_checked_int(reader.read_u32_le(), "section uncompressed length");
        if (total_section_length < 13) {
            throw std::runtime_error("Section has invalid total length");
        }
        const int payload_length = total_section_length - 13;
        if (payload_length < 0) {
            throw std::runtime_error("Section payload length is negative");
        }
        if (compression_scheme == 0 && payload_length != uncompressed_length) {
            throw std::runtime_error("Section length mismatch");
        }
        if (compression_scheme != 0 && compression_scheme != 1) {
            throw std::runtime_error("Unsupported section compression scheme " +
                                     std::to_string(compression_scheme));
        }

        auto payload_bytes = reader.read_bytes(static_cast<std::size_t>(payload_length));
        const std::uint32_t expected_checksum = reader.read_u32_le();

        std::vector<std::uint8_t> checksum_input(1 + 4 + 4 + payload_bytes.size());
        checksum_input[0] = static_cast<std::uint8_t>(compression_scheme);
        write_u32_le(checksum_input.data(), 1, total_section_length);
        write_u32_le(checksum_input.data(), 5, uncompressed_length);
        if (!payload_bytes.empty()) {
            std::memcpy(checksum_input.data() + 9, payload_bytes.data(), payload_bytes.size());
        }
        const std::uint32_t checksum = static_cast<std::uint32_t>(
            mz_adler32(MZ_ADLER32_INIT, checksum_input.data(), checksum_input.size()));
        if (checksum != expected_checksum) {
            throw std::runtime_error("Section checksum mismatch");
        }

        std::vector<std::uint8_t> object_bytes;
        if (compression_scheme == 0) {
            object_bytes = std::move(payload_bytes);
        } else {
            object_bytes.resize(static_cast<std::size_t>(uncompressed_length));
            mz_ulong dest_len = static_cast<mz_ulong>(uncompressed_length);
            const int rc = mz_uncompress(object_bytes.data(), &dest_len, payload_bytes.data(),
                                         static_cast<mz_ulong>(payload_bytes.size()));
            if (rc != MZ_OK) {
                throw std::runtime_error("Failed to inflate compressed M3G section (miniz " +
                                         std::to_string(rc) + ")");
            }
            if (dest_len != static_cast<mz_ulong>(uncompressed_length)) {
                throw std::runtime_error("Inflated M3G section size mismatch");
            }
        }

        file.sections.push_back(SectionInfo{section_index, compression_scheme, total_section_length,
                                            uncompressed_length});

        BinaryReader section_reader(object_bytes, "section-" + std::to_string(section_index));
        while (!section_reader.is_eof()) {
            const int object_type = section_reader.read_u8();
            const int object_length = to_checked_int(section_reader.read_u32_le(), "object length");
            auto payload = section_reader.read_bytes(static_cast<std::size_t>(object_length));
            BinaryReader payload_reader(payload, "object-" + std::to_string(next_object_id) + "/" +
                                                     type_name_for_object_type(object_type));
            auto parsed = parse_object(next_object_id, object_type, object_length, payload_reader, payload);
            if (!std::dynamic_pointer_cast<UnknownObject>(parsed)) {
                payload_reader.ensure_fully_consumed("Object " + std::to_string(parsed->object_id) + " (" +
                                                     parsed->type_name() + ")");
            }
            file.objects_by_id[next_object_id] = parsed;
            ++next_object_id;
        }
    }

    for (const auto &kv : file.objects_by_id) {
        if (auto h = std::dynamic_pointer_cast<HeaderObject>(kv.second)) {
            file.header = h;
            break;
        }
    }
    return file;
}

} // namespace m3g
} // namespace slop
