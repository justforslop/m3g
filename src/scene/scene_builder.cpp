#include "scene/scene_builder.hpp"

#include "scene/matrix_util.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <map>
#include <set>
#include <stdexcept>
#include <tuple>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace slop {
namespace scene {
namespace {

struct SamplerKey {
    std::optional<int> mag_filter;
    std::optional<int> min_filter;
    int wrap_s = 0;
    int wrap_t = 0;
    bool operator<(const SamplerKey &o) const {
        return std::tie(mag_filter, min_filter, wrap_s, wrap_t) <
               std::tie(o.mag_filter, o.min_filter, o.wrap_s, o.wrap_t);
    }
};

int map_wrap(int value) { return value == 241 ? 10497 : 33071; }

int map_mag_filter(int value) {
    if (value == 210) {
        return 9728;
    }
    return 9729;
}

int map_min_filter(int level_filter, int image_filter) {
    if (image_filter == 210) {
        return 9728;
    }
    (void)level_filter;
    return 9729;
}

} // namespace

class BuilderImpl {
public:
    BuilderImpl(const m3g::File &file, std::string input_path)
        : file_(file), input_path_(std::move(input_path)) {}

    SceneIr build() {
        record_global_warnings();
        std::vector<int> root_node_indices;
        for (int id : determine_root_object_ids()) {
            if (auto idx = build_node(id)) {
                if (std::find(root_node_indices.begin(), root_node_indices.end(), *idx) ==
                    root_node_indices.end()) {
                    root_node_indices.push_back(*idx);
                }
            }
        }
        if (root_node_indices.empty()) {
            throw std::runtime_error("No convertible node objects found in parsed M3G file");
        }
        SceneIr scene;
        scene.nodes = nodes_;
        scene.root_node_indices = root_node_indices;
        scene.meshes = meshes_;
        scene.materials = materials_;
        scene.textures = textures_;
        scene.images = images_;
        scene.samplers = samplers_;
        scene.cameras = cameras_;
        for (const auto &kv : warnings_) {
            scene.warnings.push_back(kv.second);
        }
        return scene;
    }

private:
    const m3g::File &file_;
    std::string input_path_;

    std::map<std::string, ConversionWarning> warnings_;
    std::vector<SceneNodeIr> nodes_;
    std::vector<SceneMeshIr> meshes_;
    std::vector<SceneMaterialIr> materials_;
    std::vector<SceneTextureIr> textures_;
    std::vector<SceneImageIr> images_;
    std::vector<SceneSamplerIr> samplers_;
    std::vector<SceneCameraIr> cameras_;

    std::map<int, int> node_index_by_object_id_;
    std::map<int, int> mesh_index_by_object_id_;
    std::map<int, std::optional<int>> material_index_by_appearance_id_;
    std::map<int, std::optional<int>> texture_index_by_object_id_;
    std::map<int, std::optional<int>> image_index_by_object_id_;
    std::map<int, std::optional<int>> camera_index_by_object_id_;
    std::map<SamplerKey, int> sampler_index_by_key_;

    void warn(const std::string &code, const std::string &message) {
        warnings_.emplace(code + ":" + message, ConversionWarning{code, message});
    }

    void record_global_warnings() {
        bool has_anim = false, has_morph = false, has_light = false, has_fog = false;
        bool has_bg_img = false, has_skin = false, has_unknown = false;
        std::string unknown_types;
        for (const auto &obj : file_.objects_in_order()) {
            if (std::dynamic_pointer_cast<m3g::AnimationControllerObject>(obj) ||
                std::dynamic_pointer_cast<m3g::AnimationTrackObject>(obj) ||
                std::dynamic_pointer_cast<m3g::KeyframeSequenceObject>(obj)) {
                has_anim = true;
            }
            if (auto u = std::dynamic_pointer_cast<m3g::UnknownObject>(obj)) {
                has_unknown = true;
                if (u->object_type == m3g::ObjectTypes::MORPHING_MESH) {
                    has_morph = true;
                }
                if (!unknown_types.empty()) {
                    unknown_types += ", ";
                }
                unknown_types += std::to_string(u->object_type) + " (" + u->type_name() + ")";
            }
            if (std::dynamic_pointer_cast<m3g::LightObject>(obj)) {
                has_light = true;
            }
            if (std::dynamic_pointer_cast<m3g::FogObject>(obj)) {
                has_fog = true;
            }
            if (auto bg = std::dynamic_pointer_cast<m3g::BackgroundObject>(obj)) {
                if (bg->background_image_id) {
                    has_bg_img = true;
                }
            }
            if (auto sk = std::dynamic_pointer_cast<m3g::SkinnedMeshObject>(obj)) {
                if (!sk->bone_transforms.empty()) {
                    has_skin = true;
                }
            }
        }
        if (has_anim) {
            warn("animation", "Animation data is present but v1 exports static transforms and meshes only.");
        }
        if (has_morph) {
            warn("morphing", "MorphingMesh objects are not supported in v1 and will be skipped.");
        }
        if (has_light) {
            warn("lights", "Lights are not exported in v1; light nodes are emitted without glTF light payloads.");
        }
        if (has_fog) {
            warn("fog", "Fog objects are not exported in v1.");
        }
        if (has_bg_img) {
            warn("background-image", "Background images are not exported in v1.");
        }
        if (has_skin) {
            warn("skinning", "Skinned meshes are exported as static meshes in v1.");
        }
        if (has_unknown) {
            warn("unknown-objects", "Unknown M3G object types were parsed as opaque payloads: " + unknown_types);
        }
    }

    std::vector<int> find_top_level_node_object_ids() {
        std::vector<int> node_ids;
        for (const auto &obj : file_.objects_in_order()) {
            if (std::dynamic_pointer_cast<m3g::NodeObject>(obj)) {
                node_ids.push_back(obj->object_id);
            }
        }
        std::sort(node_ids.begin(), node_ids.end());
        if (node_ids.empty()) {
            return {};
        }

        std::set<int> child_reference_ids;
        for (const auto &obj : file_.objects_in_order()) {
            if (auto group = std::dynamic_pointer_cast<m3g::GroupLikeObject>(obj)) {
                for (int id : group->child_ids) {
                    child_reference_ids.insert(id);
                }
                if (auto world = std::dynamic_pointer_cast<m3g::WorldObject>(obj)) {
                    if (world->active_camera_id) {
                        child_reference_ids.insert(*world->active_camera_id);
                    }
                }
            }
        }

        std::vector<int> top;
        for (int id : node_ids) {
            if (!child_reference_ids.count(id)) {
                top.push_back(id);
            }
        }
        if (!top.empty()) {
            return top;
        }
        if (auto world = file_.world_or_null()) {
            return {world->object_id};
        }
        return {node_ids.front()};
    }

    std::vector<int> determine_root_object_ids() {
        auto top_level = find_top_level_node_object_ids();
        auto world = file_.world_or_null();
        if (world) {
            std::vector<int> roots{world->object_id};
            std::vector<int> extra;
            for (int id : top_level) {
                if (id != world->object_id) {
                    extra.push_back(id);
                }
            }
            if (!extra.empty()) {
                warn("top-level-nodes",
                     "Node roots exist outside the World object; exporting them as additional scene roots.");
                roots.insert(roots.end(), extra.begin(), extra.end());
            }
            return roots;
        }
        warn("no-world", "No World object found; exporting top-level node hierarchy as the scene.");
        return top_level;
    }

    std::string synthetic_name(const m3g::Object &obj) {
        int suffix = 0;
        if (auto n = dynamic_cast<const m3g::NodeObject *>(&obj)) {
            suffix = n->node_meta.transformable.object3d.user_id;
        } else if (auto v = dynamic_cast<const m3g::VertexArrayObject *>(&obj)) {
            suffix = v->object3d.user_id;
        } else if (auto vb = dynamic_cast<const m3g::VertexBufferObject *>(&obj)) {
            suffix = vb->object3d.user_id;
        } else if (auto a = dynamic_cast<const m3g::AppearanceObject *>(&obj)) {
            suffix = a->object3d.user_id;
        } else if (auto m = dynamic_cast<const m3g::MaterialObject *>(&obj)) {
            suffix = m->object3d.user_id;
        } else if (auto t = dynamic_cast<const m3g::Texture2DObject *>(&obj)) {
            suffix = t->transformable.object3d.user_id;
        } else if (auto im = dynamic_cast<const m3g::Image2DObject *>(&obj)) {
            suffix = im->object3d.user_id;
        } else if (auto bg = dynamic_cast<const m3g::BackgroundObject *>(&obj)) {
            suffix = bg->object3d.user_id;
        }
        if (suffix != 0) {
            return obj.type_name() + "_" + std::to_string(obj.object_id) + "_u" + std::to_string(suffix);
        }
        return obj.type_name() + "_" + std::to_string(obj.object_id);
    }

    std::optional<int> build_node(int object_id) {
        auto it = node_index_by_object_id_.find(object_id);
        if (it != node_index_by_object_id_.end()) {
            return it->second;
        }
        auto found = file_.objects_by_id.find(object_id);
        if (found == file_.objects_by_id.end()) {
            throw std::runtime_error("Referenced object " + std::to_string(object_id) + " is missing");
        }
        const auto &obj = found->second;
        if (auto world = std::dynamic_pointer_cast<m3g::WorldObject>(obj)) {
            return build_group_node(*world, world->active_camera_id);
        }
        if (auto group = std::dynamic_pointer_cast<m3g::GroupLikeObject>(obj)) {
            return build_group_node(*group, std::nullopt);
        }
        if (auto mesh = std::dynamic_pointer_cast<m3g::MeshLikeObject>(obj)) {
            return build_mesh_node(*mesh);
        }
        if (auto cam = std::dynamic_pointer_cast<m3g::CameraObject>(obj)) {
            return build_camera_node(*cam);
        }
        if (auto light = std::dynamic_pointer_cast<m3g::LightObject>(obj)) {
            return build_light_node(*light);
        }
        warn("unsupported-node-" + std::to_string(obj->object_id),
             "Unsupported node object " + obj->type_name() + " (" + std::to_string(obj->object_id) +
                 ") was skipped.");
        return std::nullopt;
    }

    int build_group_node(m3g::GroupLikeObject &group, std::optional<int> additional_child_id) {
        const int object_id = group.object_id;
        std::vector<int> child_ids = group.child_ids;
        if (additional_child_id) {
            if (std::find(child_ids.begin(), child_ids.end(), *additional_child_id) == child_ids.end()) {
                child_ids.push_back(*additional_child_id);
            }
        }
        const int node_index = register_node(object_id, group, std::nullopt, std::nullopt);
        for (int child_id : child_ids) {
            if (auto child = build_node(child_id)) {
                nodes_[static_cast<std::size_t>(node_index)].children.push_back(*child);
            }
        }
        return node_index;
    }

    int build_mesh_node(m3g::MeshLikeObject &mesh_object) {
        const int mesh_index = build_mesh(mesh_object.object_id, mesh_object);
        return register_node(mesh_object.object_id, mesh_object, mesh_index, std::nullopt);
    }

    int build_camera_node(m3g::CameraObject &camera_object) {
        auto camera_index = build_camera(camera_object);
        return register_node(camera_object.object_id, camera_object, std::nullopt, camera_index);
    }

    int build_light_node(m3g::LightObject &light_object) {
        return register_node(light_object.object_id, light_object, std::nullopt, std::nullopt);
    }

    int register_node(int object_id, m3g::NodeObject &obj, std::optional<int> mesh_index,
                      std::optional<int> camera_index) {
        auto it = node_index_by_object_id_.find(object_id);
        if (it != node_index_by_object_id_.end()) {
            return it->second;
        }
        if (obj.node_meta.alignment) {
            warn("alignment", "Node alignment is present but not exported in v1.");
        }
        auto matrix = node_matrix_row_major(obj.node_meta);
        SceneNodeIr scene_node;
        scene_node.name = synthetic_name(obj);
        if (!m3g::is_identity_row_major(matrix)) {
            scene_node.matrix = matrix;
        }
        scene_node.mesh_index = mesh_index;
        scene_node.camera_index = camera_index;
        const int index = static_cast<int>(nodes_.size());
        nodes_.push_back(std::move(scene_node));
        node_index_by_object_id_[object_id] = index;
        return index;
    }

    std::optional<int> build_camera(m3g::CameraObject &camera_object) {
        auto it = camera_index_by_object_id_.find(camera_object.object_id);
        if (it != camera_index_by_object_id_.end()) {
            return it->second;
        }
        if (!camera_object.perspective) {
            warn("camera-" + std::to_string(camera_object.object_id),
                 "Camera " + std::to_string(camera_object.object_id) + " uses unsupported projection type " +
                     std::to_string(camera_object.projection_type) + "; exporting only its transform node.");
            camera_index_by_object_id_[camera_object.object_id] = std::nullopt;
            return std::nullopt;
        }
        const auto &perspective = *camera_object.perspective;
        SceneCameraIr camera;
        camera.name = synthetic_name(camera_object);
        ScenePerspectiveCameraIr p;
        p.yfov_radians = perspective.field_of_view_degrees / 180.f * static_cast<float>(M_PI);
        if (perspective.aspect_ratio > 0.f) {
            p.aspect_ratio = perspective.aspect_ratio;
        }
        p.znear = std::max(perspective.near_distance, 0.0001f);
        if (perspective.far_distance > 0.f) {
            p.zfar = perspective.far_distance;
        }
        camera.perspective = p;
        const int index = static_cast<int>(cameras_.size());
        cameras_.push_back(std::move(camera));
        camera_index_by_object_id_[camera_object.object_id] = index;
        return index;
    }

    int build_mesh(int object_id, m3g::MeshLikeObject &mesh_object) {
        auto it = mesh_index_by_object_id_.find(object_id);
        if (it != mesh_index_by_object_id_.end()) {
            return it->second;
        }
        auto vertex_buffer =
            std::dynamic_pointer_cast<m3g::VertexBufferObject>(file_.object_or_null(mesh_object.vertex_buffer_id));
        if (!vertex_buffer) {
            throw std::runtime_error("Mesh references missing vertex buffer");
        }
        auto positions_array =
            std::dynamic_pointer_cast<m3g::VertexArrayObject>(file_.object_or_null(vertex_buffer->positions_id));
        if (!positions_array) {
            throw std::runtime_error("Mesh references missing positions array");
        }
        auto positions =
            decode_scaled_array(*positions_array, vertex_buffer->position_scale, vertex_buffer->position_bias, 3);

        std::optional<std::vector<float>> normals;
        if (auto n = std::dynamic_pointer_cast<m3g::VertexArrayObject>(file_.object_or_null(vertex_buffer->normals_id))) {
            normals = decode_normals(*n);
        }
        std::optional<std::vector<float>> vertex_colors;
        if (auto c = std::dynamic_pointer_cast<m3g::VertexArrayObject>(file_.object_or_null(vertex_buffer->colors_id))) {
            vertex_colors = decode_vertex_colors(*c);
        }
        std::optional<std::vector<float>> tex_coords0;
        if (!vertex_buffer->tex_coord_bindings.empty()) {
            const auto &binding = vertex_buffer->tex_coord_bindings.front();
            auto vertex_array =
                std::dynamic_pointer_cast<m3g::VertexArrayObject>(file_.object_or_null(binding.vertex_array_id));
            if (!vertex_array) {
                warn("uv-array", "Texture coordinates reference missing vertex array.");
            } else {
                if (vertex_buffer->tex_coord_bindings.size() > 1) {
                    warn("multi-uv", "Only the first texture coordinate set is exported in v1.");
                }
                tex_coords0 = decode_tex_coords(*vertex_array, binding);
            }
        }

        std::vector<ScenePrimitiveIr> primitives;
        for (std::size_t submesh_index = 0; submesh_index < mesh_object.submeshes.size(); ++submesh_index) {
            const auto &submesh = mesh_object.submeshes[submesh_index];
            auto index_buffer = std::dynamic_pointer_cast<m3g::TriangleStripArrayObject>(
                file_.object_or_null(submesh.index_buffer_id));
            if (!index_buffer) {
                warn("index-buffer-" + std::to_string(object_id) + "-" + std::to_string(submesh_index),
                     "Submesh uses an unsupported index buffer.");
                continue;
            }
            ScenePrimitiveIr prim;
            prim.name = synthetic_name(mesh_object) + "_Primitive_" + std::to_string(submesh_index);
            prim.positions = positions;
            prim.normals = normals;
            prim.tex_coords0 = tex_coords0;
            prim.vertex_colors = vertex_colors;
            prim.indices = expand_triangle_strips(*index_buffer);
            prim.material_index = build_material(submesh.appearance_id, vertex_colors.has_value());
            primitives.push_back(std::move(prim));
        }

        SceneMeshIr mesh;
        mesh.name = synthetic_name(mesh_object);
        mesh.primitives = std::move(primitives);
        const int mesh_index = static_cast<int>(meshes_.size());
        meshes_.push_back(std::move(mesh));
        mesh_index_by_object_id_[object_id] = mesh_index;
        return mesh_index;
    }

    std::optional<int> build_material(std::optional<int> appearance_id, bool vertex_colors_present) {
        if (!appearance_id) {
            return std::nullopt;
        }
        auto it = material_index_by_appearance_id_.find(*appearance_id);
        if (it != material_index_by_appearance_id_.end()) {
            return it->second;
        }
        auto appearance =
            std::dynamic_pointer_cast<m3g::AppearanceObject>(file_.object_or_null(appearance_id));
        if (!appearance) {
            warn("appearance-" + std::to_string(*appearance_id), "Referenced appearance is missing or invalid.");
            material_index_by_appearance_id_[*appearance_id] = std::nullopt;
            return std::nullopt;
        }
        if (appearance->compositing_mode_id) {
            warn("compositing-mode", "CompositingMode is not exported in v1.");
        }
        if (appearance->fog_id) {
            warn("appearance-fog", "Appearance fog references are not exported in v1.");
        }
        if (appearance->texture_ids.size() > 1) {
            warn("multi-texture", "Only texture unit 0 is exported in v1.");
        }

        auto material_object =
            std::dynamic_pointer_cast<m3g::MaterialObject>(file_.object_or_null(appearance->material_id));
        auto polygon_mode =
            std::dynamic_pointer_cast<m3g::PolygonModeObject>(file_.object_or_null(appearance->polygon_mode_id));
        std::optional<int> texture_index;
        if (!appearance->texture_ids.empty()) {
            texture_index = build_texture(appearance->texture_ids.front());
        }

        std::vector<float> base_color_factor =
            material_object ? material_object->diffuse_color.to_float_array() : std::vector<float>{1, 1, 1, 1};
        if (vertex_colors_present && material_object && material_object->vertex_color_tracking_enabled) {
            base_color_factor = {1.f, 1.f, 1.f, base_color_factor[3]};
        }
        std::vector<float> emissive =
            material_object ? material_object->emissive_color.to_float_array() : std::vector<float>{0, 0, 0};
        float roughness = 1.f;
        if (material_object) {
            roughness = std::clamp(1.f - (material_object->shininess / 128.f), 0.f, 1.f);
        }
        std::optional<std::string> alpha_mode;
        if (base_color_factor[3] < 0.999f) {
            alpha_mode = "BLEND";
        }

        SceneMaterialIr material;
        if (appearance->object3d.user_id != 0) {
            material.name = "u" + std::to_string(appearance->object3d.user_id);
        } else {
            material.name = synthetic_name(*appearance);
        }
        material.base_color_factor = std::move(base_color_factor);
        material.base_color_texture_index = texture_index;
        material.emissive_factor = std::move(emissive);
        material.roughness_factor = roughness;
        material.metallic_factor = 0.f;
        material.double_sided = polygon_mode && polygon_mode->culling == 162;
        material.alpha_mode = alpha_mode;

        const int index = static_cast<int>(materials_.size());
        materials_.push_back(std::move(material));
        material_index_by_appearance_id_[*appearance_id] = index;
        return index;
    }

    std::optional<int> build_texture(int texture_id) {
        auto it = texture_index_by_object_id_.find(texture_id);
        if (it != texture_index_by_object_id_.end()) {
            return it->second;
        }
        auto texture = std::dynamic_pointer_cast<m3g::Texture2DObject>(file_.object_or_null(texture_id));
        if (!texture) {
            warn("texture-" + std::to_string(texture_id), "Referenced texture is missing or invalid.");
            texture_index_by_object_id_[texture_id] = std::nullopt;
            return std::nullopt;
        }
        auto image_index = build_image(texture->image_id);
        if (!image_index) {
            texture_index_by_object_id_[texture_id] = std::nullopt;
            return std::nullopt;
        }

        SamplerKey sampler_key;
        sampler_key.mag_filter = map_mag_filter(texture->image_filter);
        sampler_key.min_filter = map_min_filter(texture->level_filter, texture->image_filter);
        sampler_key.wrap_s = map_wrap(texture->wrapping_s);
        sampler_key.wrap_t = map_wrap(texture->wrapping_t);

        int sampler_index;
        auto sit = sampler_index_by_key_.find(sampler_key);
        if (sit != sampler_index_by_key_.end()) {
            sampler_index = sit->second;
        } else {
            sampler_index = static_cast<int>(samplers_.size());
            SceneSamplerIr sampler;
            sampler.mag_filter = sampler_key.mag_filter;
            sampler.min_filter = sampler_key.min_filter;
            sampler.wrap_s = sampler_key.wrap_s;
            sampler.wrap_t = sampler_key.wrap_t;
            samplers_.push_back(sampler);
            sampler_index_by_key_[sampler_key] = sampler_index;
        }

        SceneTextureIr tex;
        tex.name = synthetic_name(*texture);
        tex.image_index = *image_index;
        tex.sampler_index = sampler_index;
        const int index = static_cast<int>(textures_.size());
        textures_.push_back(std::move(tex));
        texture_index_by_object_id_[texture_id] = index;
        return index;
    }

    std::optional<int> build_image(std::optional<int> image_id) {
        if (!image_id) {
            return std::nullopt;
        }
        auto it = image_index_by_object_id_.find(*image_id);
        if (it != image_index_by_object_id_.end()) {
            return it->second;
        }
        auto image_object = file_.object_or_null(image_id);
        SceneImageIr scene_image;
        bool ok = false;
        if (auto image = std::dynamic_pointer_cast<m3g::Image2DObject>(image_object)) {
            auto rgba = decode_embedded_image_to_rgba(*image);
            if (!rgba) {
                warn("image-format", "Embedded image uses unsupported format.");
            } else {
                scene_image.name = synthetic_name(*image);
                EmbeddedRgbaImageSource src;
                src.object_id = image->object_id;
                src.width = image->width;
                src.height = image->height;
                src.pixels = std::move(*rgba);
                scene_image.embedded = std::move(src);
                ok = true;
            }
        } else if (auto ext = std::dynamic_pointer_cast<m3g::ExternalReferenceObject>(image_object)) {
            scene_image.name = synthetic_name(*ext);
            ExternalFileImageSource src;
            src.object_id = ext->object_id;
            namespace fs = std::filesystem;
            src.source_path = (fs::path(input_path_).parent_path() / ext->uri).lexically_normal().string();
            scene_image.external = std::move(src);
            ok = true;
        } else {
            warn("image-ref", "Texture image reference points to unsupported object.");
        }
        if (!ok) {
            image_index_by_object_id_[*image_id] = std::nullopt;
            return std::nullopt;
        }
        const int index = static_cast<int>(images_.size());
        images_.push_back(std::move(scene_image));
        image_index_by_object_id_[*image_id] = index;
        return index;
    }

    static std::optional<int> component_count_for_image_format(int format) {
        switch (format) {
        case 96:
        case 97:
            return 1;
        case 98:
            return 2;
        case 99:
            return 3;
        case 100:
            return 4;
        default:
            return std::nullopt;
        }
    }

    static void write_pixel_to_rgba(const std::uint8_t *source, int source_offset, std::uint8_t *target,
                                    int target_offset, int format) {
        switch (format) {
        case 96:
            target[target_offset] = 0xFF;
            target[target_offset + 1] = 0xFF;
            target[target_offset + 2] = 0xFF;
            target[target_offset + 3] = source[source_offset];
            break;
        case 97: {
            const auto l = source[source_offset];
            target[target_offset] = l;
            target[target_offset + 1] = l;
            target[target_offset + 2] = l;
            target[target_offset + 3] = 0xFF;
            break;
        }
        case 98: {
            const auto l = source[source_offset];
            target[target_offset] = l;
            target[target_offset + 1] = l;
            target[target_offset + 2] = l;
            target[target_offset + 3] = source[source_offset + 1];
            break;
        }
        case 99:
            target[target_offset] = source[source_offset];
            target[target_offset + 1] = source[source_offset + 1];
            target[target_offset + 2] = source[source_offset + 2];
            target[target_offset + 3] = 0xFF;
            break;
        case 100:
            target[target_offset] = source[source_offset];
            target[target_offset + 1] = source[source_offset + 1];
            target[target_offset + 2] = source[source_offset + 2];
            target[target_offset + 3] = source[source_offset + 3];
            break;
        default:
            throw std::runtime_error("Unsupported Image2D format");
        }
    }

    std::optional<std::vector<std::uint8_t>> decode_embedded_image_to_rgba(const m3g::Image2DObject &image) {
        if (!image.pixels) {
            return std::nullopt;
        }
        auto entry_size = component_count_for_image_format(image.format);
        if (!entry_size) {
            return std::nullopt;
        }
        const int pixel_count = image.width * image.height;
        std::vector<std::uint8_t> rgba(static_cast<std::size_t>(pixel_count) * 4u);
        const auto &pixels = *image.pixels;
        if (!image.palette.empty()) {
            if (static_cast<int>(pixels.size()) != pixel_count) {
                throw std::runtime_error("Palettized image size mismatch");
            }
            for (int pixel_index = 0; pixel_index < pixel_count; ++pixel_index) {
                const int palette_index = pixels[static_cast<std::size_t>(pixel_index)];
                const int palette_offset = palette_index * *entry_size;
                if (palette_offset + *entry_size > static_cast<int>(image.palette.size())) {
                    throw std::runtime_error("Palette index out of range");
                }
                write_pixel_to_rgba(image.palette.data(), palette_offset, rgba.data(), pixel_index * 4, image.format);
            }
        } else {
            if (static_cast<int>(pixels.size()) != pixel_count * *entry_size) {
                throw std::runtime_error("Image pixel buffer size mismatch");
            }
            for (int pixel_index = 0; pixel_index < pixel_count; ++pixel_index) {
                write_pixel_to_rgba(pixels.data(), pixel_index * *entry_size, rgba.data(), pixel_index * 4,
                                   image.format);
            }
        }
        return rgba;
    }

    static std::vector<float> decode_vertex_colors(const m3g::VertexArrayObject &array) {
        if (array.component_count != 3 && array.component_count != 4) {
            throw std::runtime_error("Vertex color array component count invalid");
        }
        std::vector<float> values(static_cast<std::size_t>(array.vertex_count * array.component_count));
        for (std::size_t i = 0; i < values.size(); ++i) {
            values[i] = (array.components[i] & 0xFF) / 255.f;
        }
        return values;
    }

    static std::vector<float> decode_scaled_array(const m3g::VertexArrayObject &array, float scale,
                                                  const std::vector<float> &bias, int expected_components) {
        if (array.component_count < expected_components) {
            throw std::runtime_error("VertexArray component count too small");
        }
        std::vector<float> values(static_cast<std::size_t>(array.vertex_count * expected_components));
        for (int vertex_index = 0; vertex_index < array.vertex_count; ++vertex_index) {
            for (int component_index = 0; component_index < expected_components; ++component_index) {
                const int source_index = vertex_index * array.component_count + component_index;
                const float bias_value =
                    component_index < static_cast<int>(bias.size()) ? bias[static_cast<std::size_t>(component_index)]
                                                                    : 0.f;
                values[static_cast<std::size_t>(vertex_index * expected_components + component_index)] =
                    array.components[static_cast<std::size_t>(source_index)] * scale + bias_value;
            }
        }
        return values;
    }

    static std::vector<float> decode_normals(const m3g::VertexArrayObject &array) {
        if (array.component_count < 3) {
            throw std::runtime_error("Normal array component count too small");
        }
        float divisor = 0.f;
        if (array.component_size == 1) {
            divisor = 127.f;
        } else if (array.component_size == 2) {
            divisor = 32767.f;
        } else {
            throw std::runtime_error("Unsupported normal component size");
        }
        std::vector<float> values(static_cast<std::size_t>(array.vertex_count * 3));
        for (int vertex_index = 0; vertex_index < array.vertex_count; ++vertex_index) {
            for (int component_index = 0; component_index < 3; ++component_index) {
                const int source_index = vertex_index * array.component_count + component_index;
                float v = array.components[static_cast<std::size_t>(source_index)] / divisor;
                values[static_cast<std::size_t>(vertex_index * 3 + component_index)] = std::clamp(v, -1.f, 1.f);
            }
        }
        return values;
    }

    static std::vector<float> decode_tex_coords(const m3g::VertexArrayObject &array,
                                                const m3g::TexCoordBinding &binding) {
        if (array.component_count < 2) {
            throw std::runtime_error("UV array component count too small");
        }
        std::vector<float> values(static_cast<std::size_t>(array.vertex_count * 2));
        for (int vertex_index = 0; vertex_index < array.vertex_count; ++vertex_index) {
            const int u_index = vertex_index * array.component_count;
            const int v_index = u_index + 1;
            const float u = array.components[static_cast<std::size_t>(u_index)] * binding.scale + binding.bias[0];
            const float v = array.components[static_cast<std::size_t>(v_index)] * binding.scale + binding.bias[1];
            values[static_cast<std::size_t>(vertex_index * 2)] = u;
            values[static_cast<std::size_t>(vertex_index * 2 + 1)] = 1.f - v;
        }
        return values;
    }

    static std::vector<int> expand_triangle_strips(const m3g::TriangleStripArrayObject &index_buffer) {
        std::vector<int> triangles;
        int cursor = 0;
        for (int strip_length : index_buffer.strip_lengths) {
            if (cursor + strip_length > static_cast<int>(index_buffer.indices.size())) {
                throw std::runtime_error("TriangleStripArray strip lengths exceed index array length");
            }
            for (int strip_index = 2; strip_index < strip_length; ++strip_index) {
                const int a = index_buffer.indices[static_cast<std::size_t>(cursor + strip_index - 2)];
                const int b = index_buffer.indices[static_cast<std::size_t>(cursor + strip_index - 1)];
                const int c = index_buffer.indices[static_cast<std::size_t>(cursor + strip_index)];
                if (a == b || b == c || a == c) {
                    continue;
                }
                if (strip_index % 2 == 0) {
                    triangles.push_back(a);
                    triangles.push_back(b);
                    triangles.push_back(c);
                } else {
                    triangles.push_back(b);
                    triangles.push_back(a);
                    triangles.push_back(c);
                }
            }
            cursor += strip_length;
        }
        return triangles;
    }
};

M3gSceneBuilder::M3gSceneBuilder(const m3g::File &file, std::string input_path)
    : file_(file), input_path_(std::move(input_path)) {}

SceneIr M3gSceneBuilder::build() {
    BuilderImpl impl(file_, input_path_);
    return impl.build();
}

} // namespace scene
} // namespace slop
