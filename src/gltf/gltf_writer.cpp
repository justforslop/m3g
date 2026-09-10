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

struct BufferSlice {
    int offset = 0;
    int length = 0;
};

class BinaryBufferBuilder {
public:
    int size() const { return static_cast<int>(out_.size()); }

    BufferSlice append_float_array(const std::vector<float> &values) {
        align(4);
        const int offset = size();
        for (float v : values) {
            std::uint32_t bits = 0;
            std::memcpy(&bits, &v, sizeof(bits));
            append_u32(bits);
        }
        return {offset, static_cast<int>(values.size()) * 4};
    }

    BufferSlice append_u16_array(const std::vector<int> &values) {
        align(4);
        const int offset = size();
        for (int v : values) {
            const auto s = static_cast<std::uint16_t>(v);
            out_.push_back(static_cast<std::uint8_t>(s & 0xFF));
            out_.push_back(static_cast<std::uint8_t>((s >> 8) & 0xFF));
        }
        return {offset, static_cast<int>(values.size()) * 2};
    }

    BufferSlice append_u32_array(const std::vector<int> &values) {
        align(4);
        const int offset = size();
        for (int v : values) {
            append_u32(static_cast<std::uint32_t>(v));
        }
        return {offset, static_cast<int>(values.size()) * 4};
    }

    BufferSlice append_bytes(const std::uint8_t *data, std::size_t length) {
        align(4);
        const int offset = size();
        out_.insert(out_.end(), data, data + length);
        return {offset, static_cast<int>(length)};
    }

    BufferSlice append_bytes(const std::vector<std::uint8_t> &data) {
        return append_bytes(data.data(), data.size());
    }

    void align_public(int alignment) { align(alignment); }

    const std::vector<std::uint8_t> &bytes() const { return out_; }

private:
    void append_u32(std::uint32_t v) {
        out_.push_back(static_cast<std::uint8_t>(v & 0xFF));
        out_.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
        out_.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
        out_.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
    }

    void align(int alignment) {
        while (size() % alignment != 0) {
            out_.push_back(0);
        }
    }

    std::vector<std::uint8_t> out_;
};

struct JsonNodeDeleter {
    void operator()(void *p) const { json_delete(p); }
};

using JsonPtr = std::unique_ptr<void, JsonNodeDeleter>;

struct JsonPrintDeleter {
    void operator()(char *p) const { json_free_print(p); }
};

void *require_json(void *item, const char *what) {
    if (!item) {
        throw std::runtime_error(std::string("JSON allocation failed: ") + what);
    }
    return item;
}

void *json_string(const std::string &s) { return require_json(json_create_string(s.c_str()), "string"); }

void *json_number(double v) { return require_json(json_create_number(v), "number"); }

void *json_bool(bool v) { return require_json(json_create_bool(v ? 1 : 0), "bool"); }

void *json_int_array(const std::vector<int> &values) {
    void *arr = require_json(json_create_array(), "int array");
    for (int v : values) {
        json_add_item_to_array(arr, json_number(v));
    }
    return arr;
}

void *json_double_array(const std::vector<double> &values) {
    void *arr = require_json(json_create_array(), "double array");
    for (double v : values) {
        json_add_item_to_array(arr, json_number(v));
    }
    return arr;
}

void *json_float_array_as_double(const std::vector<float> &values) {
    void *arr = require_json(json_create_array(), "float array");
    for (float v : values) {
        json_add_item_to_array(arr, json_number(static_cast<double>(v)));
    }
    return arr;
}

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
        out = "image";
    }
    return out;
}

std::string unique_file_name(const std::string &original, std::set<std::string> &used_names) {
    const std::string sanitized = sanitize_file_name(original);
    if (used_names.insert(sanitized).second) {
        return sanitized;
    }
    const auto dot = sanitized.find_last_of('.');
    const std::string stem = dot == std::string::npos ? sanitized : sanitized.substr(0, dot);
    const std::string ext = dot == std::string::npos ? std::string() : sanitized.substr(dot + 1);
    int counter = 1;
    while (true) {
        std::string candidate =
            ext.empty() ? stem + "_" + std::to_string(counter) : stem + "_" + std::to_string(counter) + "." + ext;
        if (used_names.insert(candidate).second) {
            return candidate;
        }
        ++counter;
    }
}

std::vector<double> list_of_component_extremes(const std::vector<float> &values, int component_count, bool find_min) {
    std::vector<double> result(static_cast<std::size_t>(component_count),
                               find_min ? std::numeric_limits<double>::infinity()
                                        : -std::numeric_limits<double>::infinity());
    for (std::size_t offset = 0; offset + static_cast<std::size_t>(component_count) <= values.size();
         offset += static_cast<std::size_t>(component_count)) {
        for (int component_index = 0; component_index < component_count; ++component_index) {
            const double value = values[offset + static_cast<std::size_t>(component_index)];
            if (find_min) {
                result[static_cast<std::size_t>(component_index)] =
                    std::min(result[static_cast<std::size_t>(component_index)], value);
            } else {
                result[static_cast<std::size_t>(component_index)] =
                    std::max(result[static_cast<std::size_t>(component_index)], value);
            }
        }
    }
    return result;
}

int vertex_color_component_count(const std::vector<float> &colors, int vertex_count) {
    if (static_cast<int>(colors.size()) == vertex_count * 3) {
        return 3;
    }
    if (static_cast<int>(colors.size()) == vertex_count * 4) {
        return 4;
    }
    throw std::runtime_error("Vertex color array size mismatch");
}

void *material_to_json(const scene::SceneMaterialIr &material) {
    void *root = require_json(json_create_object(), "material");
    json_add_item_to_object(root, "name", json_string(material.name));

    void *pbr = require_json(json_create_object(), "pbr");
    json_add_item_to_object(pbr, "baseColorFactor", json_float_array_as_double(material.base_color_factor));
    json_add_item_to_object(pbr, "metallicFactor", json_number(material.metallic_factor));
    json_add_item_to_object(pbr, "roughnessFactor", json_number(material.roughness_factor));
    if (material.base_color_texture_index) {
        void *tex = require_json(json_create_object(), "baseColorTexture");
        json_add_item_to_object(tex, "index", json_number(*material.base_color_texture_index));
        json_add_item_to_object(pbr, "baseColorTexture", tex);
    }
    json_add_item_to_object(root, "pbrMetallicRoughness", pbr);
    json_add_item_to_object(root, "emissiveFactor", json_float_array_as_double(material.emissive_factor));
    json_add_item_to_object(root, "doubleSided", json_bool(material.double_sided));
    if (material.alpha_mode) {
        json_add_item_to_object(root, "alphaMode", json_string(*material.alpha_mode));
    }
    return root;
}

void *primitive_to_json(const scene::ScenePrimitiveIr &primitive, BinaryBufferBuilder &buffer_builder, void *buffer_views,
                         void *accessors) {
    auto add_buffer_view = [&](const BufferSlice &slice, std::optional<int> target) -> int {
        const int index = json_get_array_size(buffer_views);
        void *json = require_json(json_create_object(), "bufferView");
        json_add_item_to_object(json, "buffer", json_number(0));
        json_add_item_to_object(json, "byteOffset", json_number(slice.offset));
        json_add_item_to_object(json, "byteLength", json_number(slice.length));
        if (target) {
            json_add_item_to_object(json, "target", json_number(*target));
        }
        json_add_item_to_array(buffer_views, json);
        return index;
    };

    auto add_accessor = [&](int buffer_view_index, int component_type, int count, const char *type,
                            const std::optional<std::vector<double>> &min_v,
                            const std::optional<std::vector<double>> &max_v) -> int {
        const int index = json_get_array_size(accessors);
        void *json = require_json(json_create_object(), "accessor");
        json_add_item_to_object(json, "bufferView", json_number(buffer_view_index));
        json_add_item_to_object(json, "componentType", json_number(component_type));
        json_add_item_to_object(json, "count", json_number(count));
        json_add_item_to_object(json, "type", json_string(type));
        if (min_v) {
            json_add_item_to_object(json, "min", json_double_array(*min_v));
        }
        if (max_v) {
            json_add_item_to_object(json, "max", json_double_array(*max_v));
        }
        json_add_item_to_array(accessors, json);
        return index;
    };

    void *attributes = require_json(json_create_object(), "attributes");
    const auto positions_slice = buffer_builder.append_float_array(primitive.positions);
    const int positions_view = add_buffer_view(positions_slice, 34962);
    const int position_count = static_cast<int>(primitive.positions.size() / 3);
    const auto position_min = list_of_component_extremes(primitive.positions, 3, true);
    const auto position_max = list_of_component_extremes(primitive.positions, 3, false);
    json_add_item_to_object(attributes, "POSITION",
                          json_number(add_accessor(positions_view, 5126, position_count, "VEC3", position_min, position_max)));

    if (primitive.normals) {
        const auto slice = buffer_builder.append_float_array(*primitive.normals);
        const int view = add_buffer_view(slice, 34962);
        json_add_item_to_object(attributes, "NORMAL",
                              json_number(add_accessor(view, 5126, static_cast<int>(primitive.normals->size() / 3), "VEC3",
                                                       std::nullopt, std::nullopt)));
    }
    if (primitive.tex_coords0) {
        const auto slice = buffer_builder.append_float_array(*primitive.tex_coords0);
        const int view = add_buffer_view(slice, 34962);
        json_add_item_to_object(attributes, "TEXCOORD_0",
                              json_number(add_accessor(view, 5126, static_cast<int>(primitive.tex_coords0->size() / 2),
                                                       "VEC2", std::nullopt, std::nullopt)));
    }
    if (primitive.vertex_colors) {
        const int component_count = vertex_color_component_count(*primitive.vertex_colors, position_count);
        const auto slice = buffer_builder.append_float_array(*primitive.vertex_colors);
        const int view = add_buffer_view(slice, 34962);
        const char *type = component_count == 4 ? "VEC4" : "VEC3";
        json_add_item_to_object(attributes, "COLOR_0",
                              json_number(add_accessor(view, 5126,
                                                       static_cast<int>(primitive.vertex_colors->size() / component_count),
                                                       type, std::nullopt, std::nullopt)));
    }

    int max_index = 0;
    for (int idx : primitive.indices) {
        max_index = std::max(max_index, idx);
    }
    const int index_component_type = max_index <= 0xFFFF ? 5123 : 5125;
    const BufferSlice indices_slice = index_component_type == 5123
                                          ? buffer_builder.append_u16_array(primitive.indices)
                                          : buffer_builder.append_u32_array(primitive.indices);
    const int indices_view = add_buffer_view(indices_slice, 34963);
    const int indices_accessor =
        add_accessor(indices_view, index_component_type, static_cast<int>(primitive.indices.size()), "SCALAR",
                     std::nullopt, std::nullopt);

    void *root = require_json(json_create_object(), "primitive");
    json_add_item_to_object(root, "attributes", attributes);
    json_add_item_to_object(root, "indices", json_number(indices_accessor));
    if (primitive.material_index) {
        json_add_item_to_object(root, "material", json_number(*primitive.material_index));
    }
    return root;
}

void validate_with_cgltf(const std::string &gltf_path) {
    cgltf_options options{};
    cgltf_data *data = nullptr;
    cgltf_result result = cgltf_parse_file(&options, gltf_path.c_str(), &data);
    if (result != cgltf_result_success) {
        throw std::runtime_error("cgltf failed to parse written glTF (code " + std::to_string(static_cast<int>(result)) +
                                 "): " + gltf_path);
    }
    result = cgltf_validate(data);
    cgltf_free(data);
    if (result != cgltf_result_success) {
        throw std::runtime_error("cgltf validation failed for written glTF (code " +
                                 std::to_string(static_cast<int>(result)) + "): " + gltf_path);
    }
}

std::string to_lower_ascii(std::string s) {
    for (char &c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

std::string extension_lower(const std::filesystem::path &path) {
    return to_lower_ascii(path.extension().string());
}

std::vector<std::uint8_t> read_file_bytes(const std::filesystem::path &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Failed to read file: " + path.string());
    }
    in.seekg(0, std::ios::end);
    const auto len = in.tellg();
    if (len < 0) {
        throw std::runtime_error("Failed to size file: " + path.string());
    }
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
    if (compression_level < 0) {
        compression_level = 0;
    }
    if (compression_level > 9) {
        compression_level = 9;
    }
    stbi_write_png_compression_level = compression_level;
    std::vector<std::uint8_t> bytes;
    if (stbi_write_png_to_func(stbi_write_vec_callback, &bytes, width, height, 4, pixels.data(), width * 4) == 0 ||
        bytes.empty()) {
        throw std::runtime_error("stbi_write_png_to_func failed");
    }
    return bytes;
}

void write_u32_le(std::ostream &out, std::uint32_t value) {
    const unsigned char bytes[4] = {
        static_cast<unsigned char>(value & 0xFFu),
        static_cast<unsigned char>((value >> 8) & 0xFFu),
        static_cast<unsigned char>((value >> 16) & 0xFFu),
        static_cast<unsigned char>((value >> 24) & 0xFFu),
    };
    out.write(reinterpret_cast<const char *>(bytes), 4);
}

void write_glb_file(const std::filesystem::path &path, const std::string &json,
                    const std::vector<std::uint8_t> &bin) {
    std::string json_chunk = json;
    while (json_chunk.size() % 4 != 0) {
        json_chunk.push_back(' ');
    }
    std::vector<std::uint8_t> bin_chunk = bin;
    while (bin_chunk.size() % 4 != 0) {
        bin_chunk.push_back(0);
    }

    const std::uint32_t json_len = static_cast<std::uint32_t>(json_chunk.size());
    const std::uint32_t bin_len = static_cast<std::uint32_t>(bin_chunk.size());
    const std::uint32_t total_len = 12u + 8u + json_len + 8u + bin_len;

    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Failed to write GLB: " + path.string());
    }
    write_u32_le(out, 0x46546C67u); // glTF
    write_u32_le(out, 2u);
    write_u32_le(out, total_len);
    write_u32_le(out, json_len);
    write_u32_le(out, 0x4E4F534Au); // JSON
    out.write(json_chunk.data(), static_cast<std::streamsize>(json_chunk.size()));
    write_u32_le(out, bin_len);
    write_u32_le(out, 0x004E4942u); // BIN
    if (!bin_chunk.empty()) {
        out.write(reinterpret_cast<const char *>(bin_chunk.data()),
                  static_cast<std::streamsize>(bin_chunk.size()));
    }
    if (!out) {
        throw std::runtime_error("Failed while writing GLB: " + path.string());
    }
}

} // namespace

scene::GltfWriteResult GltfWriter::write(const scene::SceneIr &scene, const std::string &output_path, bool overwrite,
                                         int png_compression_level) {
    namespace fs = std::filesystem;
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

    BinaryBufferBuilder buffer_builder;
    JsonPtr root(require_json(json_create_object(), "root"));
    void *buffer_views = require_json(json_create_array(), "bufferViews");
    void *accessors = require_json(json_create_array(), "accessors");
    void *meshes_json = require_json(json_create_array(), "meshes");

    for (const auto &mesh : scene.meshes) {
        void *mesh_json = require_json(json_create_object(), "mesh");
        json_add_item_to_object(mesh_json, "name", json_string(mesh.name));
        void *prims = require_json(json_create_array(), "primitives");
        for (const auto &primitive : mesh.primitives) {
            json_add_item_to_array(prims, primitive_to_json(primitive, buffer_builder, buffer_views, accessors));
        }
        json_add_item_to_object(mesh_json, "primitives", prims);
        json_add_item_to_array(meshes_json, mesh_json);
    }

    std::vector<std::string> image_uris;
    std::vector<std::string> image_paths;
    std::vector<int> image_buffer_views(scene.images.size(), -1);
    std::vector<std::string> image_mime_types(scene.images.size());

    if (!scene.images.empty()) {
        if (write_glb) {
            for (std::size_t i = 0; i < scene.images.size(); ++i) {
                const auto &image = scene.images[i];
                std::vector<std::uint8_t> encoded;
                std::string mime;
                if (image.embedded) {
                    encoded = encode_rgba_png(image.embedded->width, image.embedded->height, image.embedded->pixels,
                                              png_compression_level);
                    mime = "image/png";
                } else if (image.external) {
                    const fs::path source_path = image.external->source_path;
                    if (!fs::exists(source_path)) {
                        throw std::runtime_error("Missing external image: " + source_path.string());
                    }
                    const std::string extension = extension_lower(source_path);
                    if (extension == ".png") {
                        mime = "image/png";
                    } else if (extension == ".jpg" || extension == ".jpeg") {
                        mime = "image/jpeg";
                    } else {
                        throw std::runtime_error("Unsupported external image format");
                    }
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
                    } else {
                        encoded = read_file_bytes(source_path);
                    }
                } else {
                    throw std::runtime_error("Image has no source");
                }
                const BufferSlice slice = buffer_builder.append_bytes(encoded);
                const int view_index = json_get_array_size(buffer_views);
                void *view = require_json(json_create_object(), "image bufferView");
                json_add_item_to_object(view, "buffer", json_number(0));
                json_add_item_to_object(view, "byteOffset", json_number(slice.offset));
                json_add_item_to_object(view, "byteLength", json_number(slice.length));
                json_add_item_to_array(buffer_views, view);
                image_buffer_views[i] = view_index;
                image_mime_types[i] = std::move(mime);
            }
        } else {
            fs::create_directories(images_dir);
            std::set<std::string> used_names;
            for (const auto &image : scene.images) {
                std::string file_name;
                if (image.embedded) {
                    file_name =
                        unique_file_name("image_" + std::to_string(image.embedded->object_id) + ".png", used_names);
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
                    if (extension != ".png" && extension != ".jpg" && extension != ".jpeg") {
                        throw std::runtime_error("Unsupported external image format");
                    }
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
                    } else {
                        fs::copy_file(source_path, target,
                                      overwrite ? fs::copy_options::overwrite_existing : fs::copy_options::none);
                    }
                }
                image_uris.push_back(images_dir.filename().string() + "/" + file_name);
                image_paths.push_back(target.string());
            }
        }
    }

    // Final BIN chunk length must include padding used by GLB packing.
    buffer_builder.align_public(4);

    void *cameras_json = require_json(json_create_array(), "cameras");
    for (const auto &camera : scene.cameras) {
        if (!camera.perspective) {
            continue;
        }
        void *cam = require_json(json_create_object(), "camera");
        json_add_item_to_object(cam, "name", json_string(camera.name));
        json_add_item_to_object(cam, "type", json_string("perspective"));
        void *perspective = require_json(json_create_object(), "perspective");
        json_add_item_to_object(perspective, "yfov", json_number(camera.perspective->yfov_radians));
        json_add_item_to_object(perspective, "znear", json_number(camera.perspective->znear));
        if (camera.perspective->aspect_ratio) {
            json_add_item_to_object(perspective, "aspectRatio", json_number(*camera.perspective->aspect_ratio));
        }
        if (camera.perspective->zfar) {
            json_add_item_to_object(perspective, "zfar", json_number(*camera.perspective->zfar));
        }
        json_add_item_to_object(cam, "perspective", perspective);
        json_add_item_to_array(cameras_json, cam);
    }

    void *nodes_json = require_json(json_create_array(), "nodes");
    for (const auto &node : scene.nodes) {
        void *n = require_json(json_create_object(), "node");
        json_add_item_to_object(n, "name", json_string(node.name));
        if (node.translation) {
            json_add_item_to_object(n, "translation", json_float_array_as_double(*node.translation));
        }
        if (node.rotation) {
            json_add_item_to_object(n, "rotation", json_float_array_as_double(*node.rotation));
        }
        if (node.scale) {
            json_add_item_to_object(n, "scale", json_float_array_as_double(*node.scale));
        }
        if (node.matrix && !node.translation && !node.rotation && !node.scale) {
            json_add_item_to_object(n, "matrix", json_double_array(scene::row_major_to_column_major_list(*node.matrix)));
        }
        if (node.mesh_index) {
            json_add_item_to_object(n, "mesh", json_number(*node.mesh_index));
        }
        if (node.camera_index) {
            json_add_item_to_object(n, "camera", json_number(*node.camera_index));
        }
        if (!node.children.empty()) {
            json_add_item_to_object(n, "children", json_int_array(node.children));
        }
        json_add_item_to_array(nodes_json, n);
    }

    void *materials_json = require_json(json_create_array(), "materials");
    for (const auto &material : scene.materials) {
        json_add_item_to_array(materials_json, material_to_json(material));
    }

    void *textures_json = require_json(json_create_array(), "textures");
    for (const auto &texture : scene.textures) {
        void *t = require_json(json_create_object(), "texture");
        json_add_item_to_object(t, "name", json_string(texture.name));
        json_add_item_to_object(t, "source", json_number(texture.image_index));
        if (texture.sampler_index) {
            json_add_item_to_object(t, "sampler", json_number(*texture.sampler_index));
        }
        json_add_item_to_array(textures_json, t);
    }

    void *images_json = require_json(json_create_array(), "images");
    for (std::size_t i = 0; i < scene.images.size(); ++i) {
        void *im = require_json(json_create_object(), "image");
        json_add_item_to_object(im, "name", json_string(scene.images[i].name));
        if (write_glb) {
            json_add_item_to_object(im, "mimeType", json_string(image_mime_types[i]));
            json_add_item_to_object(im, "bufferView", json_number(image_buffer_views[i]));
        } else {
            json_add_item_to_object(im, "uri", json_string(image_uris[i]));
        }
        json_add_item_to_array(images_json, im);
    }

    void *samplers_json = require_json(json_create_array(), "samplers");
    for (const auto &sampler : scene.samplers) {
        void *s = require_json(json_create_object(), "sampler");
        json_add_item_to_object(s, "wrapS", json_number(sampler.wrap_s));
        json_add_item_to_object(s, "wrapT", json_number(sampler.wrap_t));
        if (sampler.mag_filter) {
            json_add_item_to_object(s, "magFilter", json_number(*sampler.mag_filter));
        }
        if (sampler.min_filter) {
            json_add_item_to_object(s, "minFilter", json_number(*sampler.min_filter));
        }
        json_add_item_to_array(samplers_json, s);
    }

    void *asset = require_json(json_create_object(), "asset");
    json_add_item_to_object(asset, "version", json_string("2.0"));
    json_add_item_to_object(asset, "generator", json_string("m3g"));
    json_add_item_to_object(root.get(), "asset", asset);
    json_add_item_to_object(root.get(), "scene", json_number(0));

    void *scenes = require_json(json_create_array(), "scenes");
    void *scene0 = require_json(json_create_object(), "scene0");
    json_add_item_to_object(scene0, "nodes", json_int_array(scene.root_node_indices));
    json_add_item_to_array(scenes, scene0);
    json_add_item_to_object(root.get(), "scenes", scenes);

    json_add_item_to_object(root.get(), "nodes", nodes_json);
    json_add_item_to_object(root.get(), "meshes", meshes_json);

    auto add_buffer_view = [&](const BufferSlice &slice) -> int {
        const int index = json_get_array_size(buffer_views);
        void *json = require_json(json_create_object(), "bufferView");
        json_add_item_to_object(json, "buffer", json_number(0));
        json_add_item_to_object(json, "byteOffset", json_number(slice.offset));
        json_add_item_to_object(json, "byteLength", json_number(slice.length));
        json_add_item_to_array(buffer_views, json);
        return index;
    };
    auto add_accessor = [&](int buffer_view_index, int component_type, int count, const char *type,
                            const std::optional<std::vector<double>> &min_v,
                            const std::optional<std::vector<double>> &max_v) -> int {
        const int index = json_get_array_size(accessors);
        void *json = require_json(json_create_object(), "accessor");
        json_add_item_to_object(json, "bufferView", json_number(buffer_view_index));
        json_add_item_to_object(json, "componentType", json_number(component_type));
        json_add_item_to_object(json, "count", json_number(count));
        json_add_item_to_object(json, "type", json_string(type));
        if (min_v) {
            json_add_item_to_object(json, "min", json_double_array(*min_v));
        }
        if (max_v) {
            json_add_item_to_object(json, "max", json_double_array(*max_v));
        }
        json_add_item_to_array(accessors, json);
        return index;
    };

    void *animations_json = require_json(json_create_array(), "animations");
    for (const auto &animation : scene.animations) {
        if (animation.channels.empty() || animation.samplers.empty()) {
            continue;
        }
        void *anim = require_json(json_create_object(), "animation");
        json_add_item_to_object(anim, "name", json_string(animation.name));
        void *samplers_json_anim = require_json(json_create_array(), "animation samplers");
        for (const auto &sampler : animation.samplers) {
            const auto time_slice = buffer_builder.append_float_array(sampler.times);
            const int time_view = add_buffer_view(time_slice);
            const auto time_min = list_of_component_extremes(sampler.times, 1, true);
            const auto time_max = list_of_component_extremes(sampler.times, 1, false);
            const int time_acc =
                add_accessor(time_view, 5126, static_cast<int>(sampler.times.size()), "SCALAR", time_min, time_max);

            const auto value_slice = buffer_builder.append_float_array(sampler.values);
            const int value_view = add_buffer_view(value_slice);
            const char *type = sampler.component_count == 4 ? "VEC4" : "VEC3";
            const int value_count =
                sampler.component_count > 0 ? static_cast<int>(sampler.values.size() / sampler.component_count) : 0;
            const int value_acc =
                add_accessor(value_view, 5126, value_count, type, std::nullopt, std::nullopt);

            void *s = require_json(json_create_object(), "animation sampler");
            json_add_item_to_object(s, "input", json_number(time_acc));
            json_add_item_to_object(s, "output", json_number(value_acc));
            json_add_item_to_object(s, "interpolation", json_string(sampler.interpolation));
            json_add_item_to_array(samplers_json_anim, s);
        }
        void *channels_json = require_json(json_create_array(), "animation channels");
        for (const auto &channel : animation.channels) {
            void *ch = require_json(json_create_object(), "animation channel");
            json_add_item_to_object(ch, "sampler", json_number(channel.sampler_index));
            void *target = require_json(json_create_object(), "animation target");
            json_add_item_to_object(target, "node", json_number(channel.node_index));
            json_add_item_to_object(target, "path", json_string(channel.path));
            json_add_item_to_object(ch, "target", target);
            json_add_item_to_array(channels_json, ch);
        }
        json_add_item_to_object(anim, "samplers", samplers_json_anim);
        json_add_item_to_object(anim, "channels", channels_json);
        json_add_item_to_array(animations_json, anim);
    }

    json_add_item_to_object(root.get(), "accessors", accessors);
    json_add_item_to_object(root.get(), "bufferViews", buffer_views);

    void *buffers = require_json(json_create_array(), "buffers");
    void *buffer0 = require_json(json_create_object(), "buffer0");
    if (!write_glb) {
        json_add_item_to_object(buffer0, "uri", json_string(bin_path.filename().string()));
    }
    json_add_item_to_object(buffer0, "byteLength", json_number(buffer_builder.size()));
    json_add_item_to_array(buffers, buffer0);
    json_add_item_to_object(root.get(), "buffers", buffers);

    if (json_get_array_size(materials_json) > 0) {
        json_add_item_to_object(root.get(), "materials", materials_json);
    } else {
        json_delete(materials_json);
    }
    if (json_get_array_size(textures_json) > 0) {
        json_add_item_to_object(root.get(), "textures", textures_json);
    } else {
        json_delete(textures_json);
    }
    if (json_get_array_size(images_json) > 0) {
        json_add_item_to_object(root.get(), "images", images_json);
    } else {
        json_delete(images_json);
    }
    if (json_get_array_size(samplers_json) > 0) {
        json_add_item_to_object(root.get(), "samplers", samplers_json);
    } else {
        json_delete(samplers_json);
    }
    if (json_get_array_size(cameras_json) > 0) {
        json_add_item_to_object(root.get(), "cameras", cameras_json);
    } else {
        json_delete(cameras_json);
    }
    if (json_get_array_size(animations_json) > 0) {
        json_add_item_to_object(root.get(), "animations", animations_json);
    } else {
        json_delete(animations_json);
    }

    std::unique_ptr<char, JsonPrintDeleter> printed(json_print_unformatted(root.get()));
    if (!printed) {
        throw std::runtime_error("json_print_unformatted failed");
    }

    if (write_glb) {
        write_glb_file(normalized_output, printed.get(), buffer_builder.bytes());
    } else {
        {
            std::ofstream out(normalized_output);
            if (!out) {
                throw std::runtime_error("Failed to write glTF: " + normalized_output.string());
            }
            // Pretty JSON for .gltf side files.
            std::unique_ptr<char, JsonPrintDeleter> pretty(json_print_formatted(root.get()));
            if (!pretty) {
                throw std::runtime_error("json_print_formatted failed");
            }
            out << pretty.get() << '\n';
        }
        {
            std::ofstream out(bin_path, std::ios::binary);
            if (!out) {
                throw std::runtime_error("Failed to write bin: " + bin_path.string());
            }
            out.write(reinterpret_cast<const char *>(buffer_builder.bytes().data()),
                      static_cast<std::streamsize>(buffer_builder.bytes().size()));
        }
    }

    // Round-trip through cgltf to ensure the document is valid glTF 2.0.
    validate_with_cgltf(normalized_output.string());

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
