#include "gltf/gltf_writer.hpp"

#include "util/png_writer.hpp"

#include "cgltf/cgltf.h"
#include "stb/stb_image_write.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace m3g {
namespace gltf {
namespace {

namespace fs = std::filesystem;

struct BufferSlice {
    cgltf_size offset = 0;
    cgltf_size length = 0;
};

class BinaryBufferBuilder {
public:
    cgltf_size size() const { return out_.size(); }

    BufferSlice append_float_array(const std::vector<float> &values) {
        align(4);
        const cgltf_size offset = size();
        for (float v : values) {
            std::uint32_t bits = 0;
            std::memcpy(&bits, &v, sizeof(bits));
            append_u32(bits);
        }
        return {offset, values.size() * 4u};
    }

    BufferSlice append_u16_array(const std::vector<int> &values) {
        align(4);
        const cgltf_size offset = size();
        for (int v : values) {
            const auto s = static_cast<std::uint16_t>(v);
            out_.push_back(static_cast<std::uint8_t>(s & 0xFF));
            out_.push_back(static_cast<std::uint8_t>((s >> 8) & 0xFF));
        }
        return {offset, values.size() * 2u};
    }

    BufferSlice append_u32_array(const std::vector<int> &values) {
        align(4);
        const cgltf_size offset = size();
        for (int v : values) {
            append_u32(static_cast<std::uint32_t>(v));
        }
        return {offset, values.size() * 4u};
    }

    BufferSlice append_bytes(const std::vector<std::uint8_t> &data) {
        align(4);
        const cgltf_size offset = size();
        out_.insert(out_.end(), data.begin(), data.end());
        return {offset, data.size()};
    }

    void align_public(int alignment) { align(alignment); }

    const std::vector<std::uint8_t> &bytes() const { return out_; }
    std::vector<std::uint8_t> take_bytes() { return std::move(out_); }

private:
    void append_u32(std::uint32_t v) {
        out_.push_back(static_cast<std::uint8_t>(v & 0xFF));
        out_.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
        out_.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
        out_.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
    }

    void align(int alignment) {
        while (static_cast<int>(size() % static_cast<cgltf_size>(alignment)) != 0) {
            out_.push_back(0);
        }
    }

    std::vector<std::uint8_t> out_;
};

/* Owns all heap blocks pointed into by cgltf_data (except bin blob, owned separately). */
struct CgltfArena {
    std::vector<std::unique_ptr<char[]>> strings;
    std::vector<std::unique_ptr<cgltf_attribute[]>> attribute_blocks;
    std::vector<std::unique_ptr<cgltf_primitive[]>> primitive_blocks;
    std::vector<std::unique_ptr<cgltf_node *[]>> node_ptr_blocks;
    std::vector<std::unique_ptr<cgltf_animation_sampler[]>> anim_sampler_blocks;
    std::vector<std::unique_ptr<cgltf_animation_channel[]>> anim_channel_blocks;

    char *dup(const std::string &s) {
        auto buf = std::make_unique<char[]>(s.size() + 1);
        std::memcpy(buf.get(), s.c_str(), s.size() + 1);
        char *p = buf.get();
        strings.push_back(std::move(buf));
        return p;
    }

    template <typename T>
    T *alloc_array(std::size_t n, std::vector<std::unique_ptr<T[]>> &store) {
        if (n == 0) {
            return nullptr;
        }
        auto block = std::make_unique<T[]>(n);
        std::memset(block.get(), 0, sizeof(T) * n);
        T *p = block.get();
        store.push_back(std::move(block));
        return p;
    }
};

std::string to_lower_ascii(std::string s) {
    for (char &c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

std::string extension_lower(const fs::path &path) { return to_lower_ascii(path.extension().string()); }

std::string sanitize_file_name(const std::string &name) {
    std::string out;
    for (unsigned char c : name) {
        if (std::isalnum(c) || c == '.' || c == '-' || c == '_') {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('_');
        }
    }
    if (out.empty()) {
        out = "file";
    }
    return out;
}

std::string unique_file_name(const std::string &desired, std::set<std::string> &used) {
    std::string base = sanitize_file_name(desired);
    std::string candidate = base;
    int n = 1;
    while (used.count(candidate)) {
        candidate = base + "_" + std::to_string(n++);
    }
    used.insert(candidate);
    return candidate;
}

std::vector<std::uint8_t> read_file_bytes(const fs::path &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Failed to read file: " + path.string());
    }
    in.seekg(0, std::ios::end);
    const auto len = static_cast<std::streamoff>(in.tellg());
    in.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(len));
    if (len > 0) {
        in.read(reinterpret_cast<char *>(bytes.data()), len);
        if (!in) {
            throw std::runtime_error("Failed to read file contents: " + path.string());
        }
    }
    return bytes;
}

void stbi_write_vec_callback(void *context, void *data, int size) {
    auto *out = static_cast<std::vector<std::uint8_t> *>(context);
    const auto *bytes = static_cast<const std::uint8_t *>(data);
    out->insert(out->end(), bytes, bytes + size);
}

std::vector<std::uint8_t> encode_rgba_png(int width, int height, const std::vector<std::uint8_t> &pixels,
                                         int compression_level) {
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("PNG dimensions must be positive");
    }
    const std::size_t expected = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u;
    if (pixels.size() != expected) {
        throw std::invalid_argument("Unexpected RGBA pixel buffer size for PNG");
    }
    compression_level = std::clamp(compression_level, 0, 9);
    stbi_write_png_compression_level = compression_level;
    std::vector<std::uint8_t> bytes;
    if (stbi_write_png_to_func(stbi_write_vec_callback, &bytes, width, height, 4, pixels.data(), width * 4) == 0 ||
        bytes.empty()) {
        throw std::runtime_error("stbi_write_png_to_func failed");
    }
    return bytes;
}

std::optional<std::vector<float>> list_of_component_extremes(const std::vector<float> &values, int components,
                                                             bool want_min) {
    if (values.empty() || components <= 0 || static_cast<int>(values.size()) % components != 0) {
        return std::nullopt;
    }
    std::vector<float> out(static_cast<std::size_t>(components));
    for (int c = 0; c < components; ++c) {
        float extreme = values[static_cast<std::size_t>(c)];
        for (std::size_t i = static_cast<std::size_t>(c); i < values.size(); i += static_cast<std::size_t>(components)) {
            extreme = want_min ? std::min(extreme, values[i]) : std::max(extreme, values[i]);
        }
        out[static_cast<std::size_t>(c)] = extreme;
    }
    return out;
}

int vertex_color_component_count(const std::vector<float> &colors, int vertex_count) {
    if (vertex_count <= 0) {
        return 4;
    }
    if (static_cast<int>(colors.size()) == vertex_count * 4) {
        return 4;
    }
    if (static_cast<int>(colors.size()) == vertex_count * 3) {
        return 3;
    }
    return 4;
}

void validate_written_gltf(const std::string &gltf_path) {
    void *data = nullptr;
    try {
        gltf_parse_file(gltf_path.c_str(), &data);
        gltf_validate(data);
    } catch (...) {
        gltf_free_data(data);
        throw;
    }
    gltf_free_data(data);
}

cgltf_buffer_view *add_view(std::vector<cgltf_buffer_view> &views, cgltf_buffer *buffer, const BufferSlice &slice,
                            cgltf_buffer_view_type type) {
    cgltf_buffer_view view{};
    view.buffer = buffer;
    view.offset = slice.offset;
    view.size = slice.length;
    view.type = type;
    views.push_back(view);
    return &views.back();
}

cgltf_accessor *add_accessor(std::vector<cgltf_accessor> &accessors, cgltf_buffer_view *view,
                             cgltf_component_type component_type, cgltf_type type, cgltf_size count,
                             const std::optional<std::vector<float>> &min_v,
                             const std::optional<std::vector<float>> &max_v) {
    cgltf_accessor acc{};
    acc.component_type = component_type;
    acc.type = type;
    acc.count = count;
    acc.buffer_view = view;
    if (min_v) {
        acc.has_min = 1;
        for (std::size_t i = 0; i < min_v->size() && i < 16; ++i) {
            acc.min[i] = (*min_v)[i];
        }
    }
    if (max_v) {
        acc.has_max = 1;
        for (std::size_t i = 0; i < max_v->size() && i < 16; ++i) {
            acc.max[i] = (*max_v)[i];
        }
    }
    accessors.push_back(acc);
    return &accessors.back();
}

cgltf_interpolation_type map_interpolation(const std::string &s) {
    if (s == "STEP") {
        return cgltf_interpolation_type_step;
    }
    if (s == "CUBICSPLINE") {
        return cgltf_interpolation_type_cubic_spline;
    }
    return cgltf_interpolation_type_linear;
}

cgltf_animation_path_type map_anim_path(const std::string &s) {
    if (s == "translation") {
        return cgltf_animation_path_type_translation;
    }
    if (s == "rotation") {
        return cgltf_animation_path_type_rotation;
    }
    if (s == "scale") {
        return cgltf_animation_path_type_scale;
    }
    if (s == "weights") {
        return cgltf_animation_path_type_weights;
    }
    return cgltf_animation_path_type_invalid;
}

cgltf_alpha_mode map_alpha_mode(const std::optional<std::string> &mode) {
    if (!mode) {
        return cgltf_alpha_mode_opaque;
    }
    if (*mode == "MASK") {
        return cgltf_alpha_mode_mask;
    }
    if (*mode == "BLEND") {
        return cgltf_alpha_mode_blend;
    }
    return cgltf_alpha_mode_opaque;
}

cgltf_filter_type map_filter(std::optional<int> f) {
    if (!f) {
        return cgltf_filter_type_undefined;
    }
    return static_cast<cgltf_filter_type>(*f);
}

cgltf_wrap_mode map_wrap(int w) {
    if (w == 33071) {
        return cgltf_wrap_mode_clamp_to_edge;
    }
    if (w == 33648) {
        return cgltf_wrap_mode_mirrored_repeat;
    }
    return cgltf_wrap_mode_repeat;
}

} // namespace

scene::GltfWriteResult GltfWriter::write(const scene::SceneIr &scene, const std::string &output_path, bool overwrite,
                                         int png_compression_level) {
    const fs::path normalized_output = fs::absolute(output_path).lexically_normal();
    const std::string ext = extension_lower(normalized_output);
    const bool write_glb = ext == ".glb";
    if (!write_glb && ext != ".gltf") {
        throw std::invalid_argument("Output path must end with .gltf or .glb: " + normalized_output.string());
    }
    const fs::path output_dir = normalized_output.parent_path();
    if (output_dir.empty()) {
        throw std::runtime_error("Output path must have a parent directory");
    }
    const std::string stem = normalized_output.stem().string();
    const fs::path bin_path = output_dir / (stem + ".bin");
    const fs::path images_dir = output_dir / (stem + "_images");

    if (!overwrite) {
        if (fs::exists(normalized_output)) {
            throw std::runtime_error("Output file already exists: " + normalized_output.string());
        }
        if (!write_glb) {
            if (fs::exists(bin_path)) {
                throw std::runtime_error("Output binary already exists: " + bin_path.string());
            }
            if (!scene.images.empty() && fs::exists(images_dir)) {
                throw std::runtime_error("Output image directory already exists: " + images_dir.string());
            }
        }
    }

    fs::create_directories(output_dir);

    CgltfArena arena;
    BinaryBufferBuilder buffer_builder;

    std::vector<cgltf_buffer_view> buffer_views;
    std::vector<cgltf_accessor> accessors;
    // Reserve generously so pointers into these vectors stay stable while building.
    buffer_views.reserve(4096);
    accessors.reserve(4096);

    cgltf_buffer buffer0{};
    // Filled later once size is known.

    // ---- meshes / primitives / accessors ----
    std::vector<cgltf_mesh> meshes(scene.meshes.size());
    for (std::size_t mi = 0; mi < scene.meshes.size(); ++mi) {
        const auto &mesh = scene.meshes[mi];
        meshes[mi].name = arena.dup(mesh.name);
        meshes[mi].primitives_count = mesh.primitives.size();
        meshes[mi].primitives =
            arena.alloc_array(mesh.primitives.size(), arena.primitive_blocks);

        for (std::size_t pi = 0; pi < mesh.primitives.size(); ++pi) {
            const auto &prim = mesh.primitives[pi];
            cgltf_primitive &out_prim = meshes[mi].primitives[pi];
            out_prim.type = cgltf_primitive_type_triangles;

            std::vector<cgltf_attribute> attrs;
            attrs.reserve(4);

            {
                const auto slice = buffer_builder.append_float_array(prim.positions);
                cgltf_buffer_view *view =
                    add_view(buffer_views, &buffer0, slice, cgltf_buffer_view_type_vertices);
                const cgltf_size count = prim.positions.size() / 3;
                auto mn = list_of_component_extremes(prim.positions, 3, true);
                auto mx = list_of_component_extremes(prim.positions, 3, false);
                cgltf_accessor *acc =
                    add_accessor(accessors, view, cgltf_component_type_r_32f, cgltf_type_vec3, count, mn, mx);
                cgltf_attribute a{};
                a.name = arena.dup("POSITION");
                a.type = cgltf_attribute_type_position;
                a.index = 0;
                a.data = acc;
                attrs.push_back(a);
            }
            if (prim.normals) {
                const auto slice = buffer_builder.append_float_array(*prim.normals);
                cgltf_buffer_view *view =
                    add_view(buffer_views, &buffer0, slice, cgltf_buffer_view_type_vertices);
                cgltf_accessor *acc =
                    add_accessor(accessors, view, cgltf_component_type_r_32f, cgltf_type_vec3,
                                 prim.normals->size() / 3, std::nullopt, std::nullopt);
                cgltf_attribute a{};
                a.name = arena.dup("NORMAL");
                a.type = cgltf_attribute_type_normal;
                a.index = 0;
                a.data = acc;
                attrs.push_back(a);
            }
            if (prim.tex_coords0) {
                const auto slice = buffer_builder.append_float_array(*prim.tex_coords0);
                cgltf_buffer_view *view =
                    add_view(buffer_views, &buffer0, slice, cgltf_buffer_view_type_vertices);
                cgltf_accessor *acc =
                    add_accessor(accessors, view, cgltf_component_type_r_32f, cgltf_type_vec2,
                                 prim.tex_coords0->size() / 2, std::nullopt, std::nullopt);
                cgltf_attribute a{};
                a.name = arena.dup("TEXCOORD_0");
                a.type = cgltf_attribute_type_texcoord;
                a.index = 0;
                a.data = acc;
                attrs.push_back(a);
            }
            if (prim.vertex_colors) {
                const int position_count = static_cast<int>(prim.positions.size() / 3);
                const int cc = vertex_color_component_count(*prim.vertex_colors, position_count);
                const auto slice = buffer_builder.append_float_array(*prim.vertex_colors);
                cgltf_buffer_view *view =
                    add_view(buffer_views, &buffer0, slice, cgltf_buffer_view_type_vertices);
                cgltf_accessor *acc = add_accessor(
                    accessors, view, cgltf_component_type_r_32f, cc == 4 ? cgltf_type_vec4 : cgltf_type_vec3,
                    prim.vertex_colors->size() / static_cast<std::size_t>(cc), std::nullopt, std::nullopt);
                cgltf_attribute a{};
                a.name = arena.dup("COLOR_0");
                a.type = cgltf_attribute_type_color;
                a.index = 0;
                a.data = acc;
                attrs.push_back(a);
            }

            out_prim.attributes_count = attrs.size();
            out_prim.attributes = arena.alloc_array(attrs.size(), arena.attribute_blocks);
            for (std::size_t ai = 0; ai < attrs.size(); ++ai) {
                out_prim.attributes[ai] = attrs[ai];
            }

            int max_index = 0;
            for (int idx : prim.indices) {
                max_index = std::max(max_index, idx);
            }
            const bool use_u16 = max_index <= 0xFFFF;
            const BufferSlice indices_slice =
                use_u16 ? buffer_builder.append_u16_array(prim.indices) : buffer_builder.append_u32_array(prim.indices);
            cgltf_buffer_view *indices_view =
                add_view(buffer_views, &buffer0, indices_slice, cgltf_buffer_view_type_indices);
            out_prim.indices =
                add_accessor(accessors, indices_view,
                             use_u16 ? cgltf_component_type_r_16u : cgltf_component_type_r_32u, cgltf_type_scalar,
                             prim.indices.size(), std::nullopt, std::nullopt);

            if (prim.material_index) {
                // material pointer patched after materials array is allocated
                out_prim.material = reinterpret_cast<cgltf_material *>(
                    static_cast<std::uintptr_t>(*prim.material_index) + 1); // temp tag
            }
        }
    }

    // ---- images ----
    std::vector<std::string> image_paths;
    std::vector<cgltf_image> images(scene.images.size());
    std::set<std::string> used_names;

    if (!scene.images.empty() && !write_glb) {
        fs::create_directories(images_dir);
    }

    for (std::size_t i = 0; i < scene.images.size(); ++i) {
        const auto &image = scene.images[i];
        images[i].name = arena.dup(image.name);

        if (write_glb) {
            std::vector<std::uint8_t> encoded;
            std::string mime = "image/png";
            if (image.embedded) {
                encoded = encode_rgba_png(image.embedded->width, image.embedded->height, image.embedded->pixels,
                                          png_compression_level);
            } else if (image.external) {
                const fs::path source_path = image.external->source_path;
                if (!fs::exists(source_path)) {
                    throw std::runtime_error("Missing external image: " + source_path.string());
                }
                const std::string extension = extension_lower(source_path);
                if (extension == ".png") {
                    int w = 0, h = 0, n = 0;
                    unsigned char *data = image_load_file(source_path.string().c_str(), &w, &h, &n, 4);
                    if (!data || w <= 0 || h <= 0) {
                        if (data) {
                            image_free_pixels(data);
                        }
                        throw std::runtime_error("Failed to decode pattern PNG: " + source_path.string());
                    }
                    std::vector<std::uint8_t> rgba(data, data + static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4u);
                    image_free_pixels(data);
                    encoded = encode_rgba_png(w, h, rgba, png_compression_level);
                } else if (extension == ".jpg" || extension == ".jpeg") {
                    mime = "image/jpeg";
                    encoded = read_file_bytes(source_path);
                } else {
                    throw std::runtime_error("Unsupported external image format");
                }
            } else {
                throw std::runtime_error("Image has no source");
            }
            const BufferSlice slice = buffer_builder.append_bytes(encoded);
            images[i].buffer_view = add_view(buffer_views, &buffer0, slice, cgltf_buffer_view_type_invalid);
            images[i].mime_type = arena.dup(mime);
        } else {
            std::string file_name;
            if (image.embedded) {
                file_name = unique_file_name("image_" + std::to_string(image.embedded->object_id) + ".png", used_names);
            } else if (image.external) {
                file_name = unique_file_name(fs::path(image.external->source_path).filename().string(), used_names);
            } else {
                throw std::runtime_error("Image has no source");
            }
            const fs::path target = images_dir / file_name;
            if (image.embedded) {
                util::PngWriter::write_rgba(target.string(), image.embedded->width, image.embedded->height,
                                            image.embedded->pixels, png_compression_level);
            } else {
                const fs::path source_path = image.external->source_path;
                if (!fs::exists(source_path)) {
                    throw std::runtime_error("Missing external image: " + source_path.string());
                }
                const std::string extension = extension_lower(source_path);
                if (extension == ".png") {
                    int w = 0, h = 0, n = 0;
                    unsigned char *data = image_load_file(source_path.string().c_str(), &w, &h, &n, 4);
                    if (!data || w <= 0 || h <= 0) {
                        if (data) {
                            image_free_pixels(data);
                        }
                        throw std::runtime_error("Failed to decode pattern PNG: " + source_path.string());
                    }
                    std::vector<std::uint8_t> rgba(
                        data, data + static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4u);
                    image_free_pixels(data);
                    util::PngWriter::write_rgba(target.string(), w, h, rgba, png_compression_level);
                } else if (extension == ".jpg" || extension == ".jpeg") {
                    fs::copy_file(source_path, target,
                                  overwrite ? fs::copy_options::overwrite_existing : fs::copy_options::none);
                } else {
                    throw std::runtime_error("Unsupported external image format");
                }
            }
            const std::string uri = images_dir.filename().string() + "/" + file_name;
            images[i].uri = arena.dup(uri);
            image_paths.push_back(target.string());
        }
    }

    // ---- samplers / textures / materials ----
    std::vector<cgltf_sampler> samplers(scene.samplers.size());
    for (std::size_t i = 0; i < scene.samplers.size(); ++i) {
        samplers[i].mag_filter = map_filter(scene.samplers[i].mag_filter);
        samplers[i].min_filter = map_filter(scene.samplers[i].min_filter);
        samplers[i].wrap_s = map_wrap(scene.samplers[i].wrap_s);
        samplers[i].wrap_t = map_wrap(scene.samplers[i].wrap_t);
    }

    std::vector<cgltf_texture> textures(scene.textures.size());
    for (std::size_t i = 0; i < scene.textures.size(); ++i) {
        textures[i].name = arena.dup(scene.textures[i].name);
        if (scene.textures[i].image_index >= 0 &&
            static_cast<std::size_t>(scene.textures[i].image_index) < images.size()) {
            textures[i].image = &images[static_cast<std::size_t>(scene.textures[i].image_index)];
        }
        if (scene.textures[i].sampler_index &&
            static_cast<std::size_t>(*scene.textures[i].sampler_index) < samplers.size()) {
            textures[i].sampler = &samplers[static_cast<std::size_t>(*scene.textures[i].sampler_index)];
        }
    }

    std::vector<cgltf_material> materials(scene.materials.size());
    for (std::size_t i = 0; i < scene.materials.size(); ++i) {
        const auto &mat = scene.materials[i];
        materials[i].name = arena.dup(mat.name);
        materials[i].has_pbr_metallic_roughness = 1;
        materials[i].pbr_metallic_roughness.base_color_factor[0] = mat.base_color_factor.size() > 0 ? mat.base_color_factor[0] : 1.f;
        materials[i].pbr_metallic_roughness.base_color_factor[1] = mat.base_color_factor.size() > 1 ? mat.base_color_factor[1] : 1.f;
        materials[i].pbr_metallic_roughness.base_color_factor[2] = mat.base_color_factor.size() > 2 ? mat.base_color_factor[2] : 1.f;
        materials[i].pbr_metallic_roughness.base_color_factor[3] = mat.base_color_factor.size() > 3 ? mat.base_color_factor[3] : 1.f;
        materials[i].pbr_metallic_roughness.metallic_factor = mat.metallic_factor;
        materials[i].pbr_metallic_roughness.roughness_factor = mat.roughness_factor;
        if (mat.base_color_texture_index &&
            static_cast<std::size_t>(*mat.base_color_texture_index) < textures.size()) {
            materials[i].pbr_metallic_roughness.base_color_texture.texture =
                &textures[static_cast<std::size_t>(*mat.base_color_texture_index)];
        }
        materials[i].emissive_factor[0] = mat.emissive_factor.size() > 0 ? mat.emissive_factor[0] : 0.f;
        materials[i].emissive_factor[1] = mat.emissive_factor.size() > 1 ? mat.emissive_factor[1] : 0.f;
        materials[i].emissive_factor[2] = mat.emissive_factor.size() > 2 ? mat.emissive_factor[2] : 0.f;
        materials[i].double_sided = mat.double_sided ? 1 : 0;
        materials[i].alpha_mode = map_alpha_mode(mat.alpha_mode);
    }

    // Patch material pointers on primitives
    for (std::size_t mi = 0; mi < meshes.size(); ++mi) {
        for (cgltf_size pi = 0; pi < meshes[mi].primitives_count; ++pi) {
            cgltf_material *tagged = meshes[mi].primitives[pi].material;
            if (!tagged) {
                continue;
            }
            const std::size_t idx = static_cast<std::size_t>(reinterpret_cast<std::uintptr_t>(tagged) - 1);
            if (idx >= materials.size()) {
                meshes[mi].primitives[pi].material = nullptr;
            } else {
                meshes[mi].primitives[pi].material = &materials[idx];
            }
        }
    }

    // ---- cameras ----
    std::vector<cgltf_camera> cameras;
    cameras.reserve(scene.cameras.size());
    std::vector<int> camera_ir_to_out(scene.cameras.size(), -1);
    for (std::size_t i = 0; i < scene.cameras.size(); ++i) {
        if (!scene.cameras[i].perspective) {
            continue;
        }
        cgltf_camera cam{};
        cam.name = arena.dup(scene.cameras[i].name);
        cam.type = cgltf_camera_type_perspective;
        cam.data.perspective.yfov = scene.cameras[i].perspective->yfov_radians;
        cam.data.perspective.znear = scene.cameras[i].perspective->znear;
        if (scene.cameras[i].perspective->aspect_ratio) {
            cam.data.perspective.has_aspect_ratio = 1;
            cam.data.perspective.aspect_ratio = *scene.cameras[i].perspective->aspect_ratio;
        }
        if (scene.cameras[i].perspective->zfar) {
            cam.data.perspective.has_zfar = 1;
            cam.data.perspective.zfar = *scene.cameras[i].perspective->zfar;
        }
        camera_ir_to_out[i] = static_cast<int>(cameras.size());
        cameras.push_back(cam);
    }

    // ---- nodes ----
    std::vector<cgltf_node> nodes(scene.nodes.size());
    for (std::size_t i = 0; i < scene.nodes.size(); ++i) {
        const auto &node = scene.nodes[i];
        nodes[i].name = arena.dup(node.name);
        if (node.translation) {
            nodes[i].has_translation = 1;
            for (int k = 0; k < 3 && k < static_cast<int>(node.translation->size()); ++k) {
                nodes[i].translation[k] = (*node.translation)[static_cast<std::size_t>(k)];
            }
        }
        if (node.rotation) {
            nodes[i].has_rotation = 1;
            for (int k = 0; k < 4 && k < static_cast<int>(node.rotation->size()); ++k) {
                nodes[i].rotation[k] = (*node.rotation)[static_cast<std::size_t>(k)];
            }
        }
        if (node.scale) {
            nodes[i].has_scale = 1;
            for (int k = 0; k < 3 && k < static_cast<int>(node.scale->size()); ++k) {
                nodes[i].scale[k] = (*node.scale)[static_cast<std::size_t>(k)];
            }
        }
        if (node.matrix) {
            nodes[i].has_matrix = 1;
            // scene IR is row-major; glTF matrix is column-major
            const auto cm = scene::row_major_to_column_major_list(*node.matrix);
            for (int k = 0; k < 16; ++k) {
                nodes[i].matrix[k] = static_cast<cgltf_float>(cm[static_cast<std::size_t>(k)]);
            }
        }
        if (node.mesh_index && static_cast<std::size_t>(*node.mesh_index) < meshes.size()) {
            nodes[i].mesh = &meshes[static_cast<std::size_t>(*node.mesh_index)];
        }
        if (node.camera_index && static_cast<std::size_t>(*node.camera_index) < camera_ir_to_out.size()) {
            const int out_ci = camera_ir_to_out[static_cast<std::size_t>(*node.camera_index)];
            if (out_ci >= 0) {
                nodes[i].camera = &cameras[static_cast<std::size_t>(out_ci)];
            }
        }
        if (!node.children.empty()) {
            nodes[i].children_count = node.children.size();
            nodes[i].children = arena.alloc_array(node.children.size(), arena.node_ptr_blocks);
            for (std::size_t c = 0; c < node.children.size(); ++c) {
                const int ci = node.children[c];
                nodes[i].children[c] = (ci >= 0 && static_cast<std::size_t>(ci) < nodes.size())
                                           ? &nodes[static_cast<std::size_t>(ci)]
                                           : nullptr;
            }
        }
    }
    // parents
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        for (cgltf_size c = 0; c < nodes[i].children_count; ++c) {
            if (nodes[i].children[c]) {
                nodes[i].children[c]->parent = &nodes[i];
            }
        }
    }

    // ---- animations ----
    std::vector<cgltf_animation> animations(scene.animations.size());
    for (std::size_t ai = 0; ai < scene.animations.size(); ++ai) {
        const auto &anim = scene.animations[ai];
        animations[ai].name = arena.dup(anim.name);
        animations[ai].samplers_count = anim.samplers.size();
        animations[ai].samplers = arena.alloc_array(anim.samplers.size(), arena.anim_sampler_blocks);
        for (std::size_t si = 0; si < anim.samplers.size(); ++si) {
            const auto &samp = anim.samplers[si];
            const auto time_slice = buffer_builder.append_float_array(samp.times);
            cgltf_buffer_view *time_view =
                add_view(buffer_views, &buffer0, time_slice, cgltf_buffer_view_type_invalid);
            auto tmin = list_of_component_extremes(samp.times, 1, true);
            auto tmax = list_of_component_extremes(samp.times, 1, false);
            cgltf_accessor *time_acc =
                add_accessor(accessors, time_view, cgltf_component_type_r_32f, cgltf_type_scalar, samp.times.size(),
                             tmin, tmax);

            const auto val_slice = buffer_builder.append_float_array(samp.values);
            cgltf_buffer_view *val_view =
                add_view(buffer_views, &buffer0, val_slice, cgltf_buffer_view_type_invalid);
            cgltf_type vtype = cgltf_type_vec3;
            if (samp.component_count == 4) {
                vtype = cgltf_type_vec4;
            } else if (samp.component_count == 1) {
                vtype = cgltf_type_scalar;
            } else if (samp.component_count == 2) {
                vtype = cgltf_type_vec2;
            }
            const cgltf_size vcount =
                samp.component_count > 0 ? samp.values.size() / static_cast<std::size_t>(samp.component_count) : 0;
            cgltf_accessor *val_acc =
                add_accessor(accessors, val_view, cgltf_component_type_r_32f, vtype, vcount, std::nullopt, std::nullopt);

            animations[ai].samplers[si].input = time_acc;
            animations[ai].samplers[si].output = val_acc;
            animations[ai].samplers[si].interpolation = map_interpolation(samp.interpolation);
        }

        animations[ai].channels_count = anim.channels.size();
        animations[ai].channels = arena.alloc_array(anim.channels.size(), arena.anim_channel_blocks);
        for (std::size_t ci = 0; ci < anim.channels.size(); ++ci) {
            const auto &ch = anim.channels[ci];
            if (ch.sampler_index >= 0 && static_cast<std::size_t>(ch.sampler_index) < animations[ai].samplers_count) {
                animations[ai].channels[ci].sampler = &animations[ai].samplers[static_cast<std::size_t>(ch.sampler_index)];
            }
            if (ch.node_index >= 0 && static_cast<std::size_t>(ch.node_index) < nodes.size()) {
                animations[ai].channels[ci].target_node = &nodes[static_cast<std::size_t>(ch.node_index)];
            }
            animations[ai].channels[ci].target_path = map_anim_path(ch.path);
        }
    }

    buffer_builder.align_public(4);
    std::vector<std::uint8_t> bin_bytes = buffer_builder.take_bytes();
    buffer0.size = bin_bytes.size();

    std::string bin_uri_storage;
    if (!write_glb) {
        {
            std::ofstream out(bin_path, std::ios::binary);
            if (!out) {
                throw std::runtime_error("Failed to write bin: " + bin_path.string());
            }
            if (!bin_bytes.empty()) {
                out.write(reinterpret_cast<const char *>(bin_bytes.data()),
                          static_cast<std::streamsize>(bin_bytes.size()));
            }
        }
        bin_uri_storage = bin_path.filename().string();
        buffer0.uri = bin_uri_storage.data();
    }

    // Fix buffer pointers after possible reallocation of buffer_views/accessors - they point to buffer0 which is stable.
    // But buffer_views/accessors vectors may have reallocated - all internal pointers between them are by address into
    // those vectors. We reserved 4096; assert we didn't exceed.
    if (buffer_views.size() > 4096 || accessors.size() > 4096) {
        throw std::runtime_error("Internal glTF builder exceeded reserved buffer view/accessor capacity");
    }

    cgltf_data data{};
    data.asset.generator = arena.dup("m3g");
    data.asset.version = arena.dup("2.0");

    data.buffers = &buffer0;
    data.buffers_count = 1;
    data.buffer_views = buffer_views.data();
    data.buffer_views_count = buffer_views.size();
    data.accessors = accessors.data();
    data.accessors_count = accessors.size();
    data.meshes = meshes.data();
    data.meshes_count = meshes.size();
    data.materials = materials.empty() ? nullptr : materials.data();
    data.materials_count = materials.size();
    data.textures = textures.empty() ? nullptr : textures.data();
    data.textures_count = textures.size();
    data.images = images.empty() ? nullptr : images.data();
    data.images_count = images.size();
    data.samplers = samplers.empty() ? nullptr : samplers.data();
    data.samplers_count = samplers.size();
    data.cameras = cameras.empty() ? nullptr : cameras.data();
    data.cameras_count = cameras.size();
    data.nodes = nodes.data();
    data.nodes_count = nodes.size();
    data.animations = animations.empty() ? nullptr : animations.data();
    data.animations_count = animations.size();

    cgltf_scene root_scene{};
    root_scene.nodes_count = scene.root_node_indices.size();
    root_scene.nodes = arena.alloc_array(scene.root_node_indices.size(), arena.node_ptr_blocks);
    for (std::size_t i = 0; i < scene.root_node_indices.size(); ++i) {
        const int ni = scene.root_node_indices[i];
        root_scene.nodes[i] =
            (ni >= 0 && static_cast<std::size_t>(ni) < nodes.size()) ? &nodes[static_cast<std::size_t>(ni)] : nullptr;
    }
    data.scenes = &root_scene;
    data.scenes_count = 1;
    data.scene = &root_scene;

    if (write_glb) {
        data.bin = bin_bytes.data();
        data.bin_size = bin_bytes.size();
        data.file_type = cgltf_file_type_glb;
    } else {
        data.file_type = cgltf_file_type_gltf;
    }

    gltf_write_file(normalized_output.string().c_str(), &data,
                    write_glb ? GLTF_FILE_GLB : GLTF_FILE_JSON);
    validate_written_gltf(normalized_output.string());

    scene::GltfWriteResult result;
    result.gltf_path = normalized_output.string();
    if (!write_glb) {
        result.bin_path = bin_path.string();
    }
    result.image_paths = std::move(image_paths);
    return result;
}

} // namespace gltf
} // namespace m3g
