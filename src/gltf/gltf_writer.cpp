#include "gltf/gltf_writer.hpp"

#include "scene/matrix_util.hpp"
#include "util/png_writer.hpp"

#include "cgltf/cgltf.h"
#include "cjson/cJSON.h"
#include "stb/stb_image.h"
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

namespace slop {
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

struct CJsonDeleter {
    void operator()(cJSON *p) const {
        if (p) {
            cJSON_Delete(p);
        }
    }
};

using CJsonPtr = std::unique_ptr<cJSON, CJsonDeleter>;

struct CJsonPrintDeleter {
    void operator()(char *p) const {
        if (p) {
            cJSON_free(p);
        }
    }
};

cJSON *require_json(cJSON *item, const char *what) {
    if (!item) {
        throw std::runtime_error(std::string("cJSON allocation failed: ") + what);
    }
    return item;
}

cJSON *json_string(const std::string &s) { return require_json(cJSON_CreateString(s.c_str()), "string"); }

cJSON *json_number(double v) { return require_json(cJSON_CreateNumber(v), "number"); }

cJSON *json_bool(bool v) { return require_json(cJSON_CreateBool(v ? 1 : 0), "bool"); }

cJSON *json_int_array(const std::vector<int> &values) {
    cJSON *arr = require_json(cJSON_CreateArray(), "int array");
    for (int v : values) {
        cJSON_AddItemToArray(arr, json_number(v));
    }
    return arr;
}

cJSON *json_double_array(const std::vector<double> &values) {
    cJSON *arr = require_json(cJSON_CreateArray(), "double array");
    for (double v : values) {
        cJSON_AddItemToArray(arr, json_number(v));
    }
    return arr;
}

cJSON *json_float_array_as_double(const std::vector<float> &values) {
    cJSON *arr = require_json(cJSON_CreateArray(), "float array");
    for (float v : values) {
        cJSON_AddItemToArray(arr, json_number(static_cast<double>(v)));
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

cJSON *material_to_json(const scene::SceneMaterialIr &material) {
    cJSON *root = require_json(cJSON_CreateObject(), "material");
    cJSON_AddItemToObject(root, "name", json_string(material.name));

    cJSON *pbr = require_json(cJSON_CreateObject(), "pbr");
    cJSON_AddItemToObject(pbr, "baseColorFactor", json_float_array_as_double(material.base_color_factor));
    cJSON_AddItemToObject(pbr, "metallicFactor", json_number(material.metallic_factor));
    cJSON_AddItemToObject(pbr, "roughnessFactor", json_number(material.roughness_factor));
    if (material.base_color_texture_index) {
        cJSON *tex = require_json(cJSON_CreateObject(), "baseColorTexture");
        cJSON_AddItemToObject(tex, "index", json_number(*material.base_color_texture_index));
        cJSON_AddItemToObject(pbr, "baseColorTexture", tex);
    }
    cJSON_AddItemToObject(root, "pbrMetallicRoughness", pbr);
    cJSON_AddItemToObject(root, "emissiveFactor", json_float_array_as_double(material.emissive_factor));
    cJSON_AddItemToObject(root, "doubleSided", json_bool(material.double_sided));
    if (material.alpha_mode) {
        cJSON_AddItemToObject(root, "alphaMode", json_string(*material.alpha_mode));
    }
    return root;
}

cJSON *primitive_to_json(const scene::ScenePrimitiveIr &primitive, BinaryBufferBuilder &buffer_builder, cJSON *buffer_views,
                         cJSON *accessors) {
    auto add_buffer_view = [&](const BufferSlice &slice, std::optional<int> target) -> int {
        const int index = cJSON_GetArraySize(buffer_views);
        cJSON *json = require_json(cJSON_CreateObject(), "bufferView");
        cJSON_AddItemToObject(json, "buffer", json_number(0));
        cJSON_AddItemToObject(json, "byteOffset", json_number(slice.offset));
        cJSON_AddItemToObject(json, "byteLength", json_number(slice.length));
        if (target) {
            cJSON_AddItemToObject(json, "target", json_number(*target));
        }
        cJSON_AddItemToArray(buffer_views, json);
        return index;
    };

    auto add_accessor = [&](int buffer_view_index, int component_type, int count, const char *type,
                            const std::optional<std::vector<double>> &min_v,
                            const std::optional<std::vector<double>> &max_v) -> int {
        const int index = cJSON_GetArraySize(accessors);
        cJSON *json = require_json(cJSON_CreateObject(), "accessor");
        cJSON_AddItemToObject(json, "bufferView", json_number(buffer_view_index));
        cJSON_AddItemToObject(json, "componentType", json_number(component_type));
        cJSON_AddItemToObject(json, "count", json_number(count));
        cJSON_AddItemToObject(json, "type", json_string(type));
        if (min_v) {
            cJSON_AddItemToObject(json, "min", json_double_array(*min_v));
        }
        if (max_v) {
            cJSON_AddItemToObject(json, "max", json_double_array(*max_v));
        }
        cJSON_AddItemToArray(accessors, json);
        return index;
    };

    cJSON *attributes = require_json(cJSON_CreateObject(), "attributes");
    const auto positions_slice = buffer_builder.append_float_array(primitive.positions);
    const int positions_view = add_buffer_view(positions_slice, 34962);
    const int position_count = static_cast<int>(primitive.positions.size() / 3);
    const auto position_min = list_of_component_extremes(primitive.positions, 3, true);
    const auto position_max = list_of_component_extremes(primitive.positions, 3, false);
    cJSON_AddItemToObject(attributes, "POSITION",
                          json_number(add_accessor(positions_view, 5126, position_count, "VEC3", position_min, position_max)));

    if (primitive.normals) {
        const auto slice = buffer_builder.append_float_array(*primitive.normals);
        const int view = add_buffer_view(slice, 34962);
        cJSON_AddItemToObject(attributes, "NORMAL",
                              json_number(add_accessor(view, 5126, static_cast<int>(primitive.normals->size() / 3), "VEC3",
                                                       std::nullopt, std::nullopt)));
    }
    if (primitive.tex_coords0) {
        const auto slice = buffer_builder.append_float_array(*primitive.tex_coords0);
        const int view = add_buffer_view(slice, 34962);
        cJSON_AddItemToObject(attributes, "TEXCOORD_0",
                              json_number(add_accessor(view, 5126, static_cast<int>(primitive.tex_coords0->size() / 2),
                                                       "VEC2", std::nullopt, std::nullopt)));
    }
    if (primitive.vertex_colors) {
        const int component_count = vertex_color_component_count(*primitive.vertex_colors, position_count);
        const auto slice = buffer_builder.append_float_array(*primitive.vertex_colors);
        const int view = add_buffer_view(slice, 34962);
        const char *type = component_count == 4 ? "VEC4" : "VEC3";
        cJSON_AddItemToObject(attributes, "COLOR_0",
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

    cJSON *root = require_json(cJSON_CreateObject(), "primitive");
    cJSON_AddItemToObject(root, "attributes", attributes);
    cJSON_AddItemToObject(root, "indices", json_number(indices_accessor));
    if (primitive.material_index) {
        cJSON_AddItemToObject(root, "material", json_number(*primitive.material_index));
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
    CJsonPtr root(require_json(cJSON_CreateObject(), "root"));
    cJSON *buffer_views = require_json(cJSON_CreateArray(), "bufferViews");
    cJSON *accessors = require_json(cJSON_CreateArray(), "accessors");
    cJSON *meshes_json = require_json(cJSON_CreateArray(), "meshes");

    for (const auto &mesh : scene.meshes) {
        cJSON *mesh_json = require_json(cJSON_CreateObject(), "mesh");
        cJSON_AddItemToObject(mesh_json, "name", json_string(mesh.name));
        cJSON *prims = require_json(cJSON_CreateArray(), "primitives");
        for (const auto &primitive : mesh.primitives) {
            cJSON_AddItemToArray(prims, primitive_to_json(primitive, buffer_builder, buffer_views, accessors));
        }
        cJSON_AddItemToObject(mesh_json, "primitives", prims);
        cJSON_AddItemToArray(meshes_json, mesh_json);
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
                        unsigned char *data = stbi_load(source_path.string().c_str(), &w, &h, &n, 4);
                        if (!data || w <= 0 || h <= 0) {
                            if (data) {
                                stbi_image_free(data);
                            }
                            throw std::runtime_error("Failed to decode pattern PNG: " + source_path.string());
                        }
                        std::vector<std::uint8_t> rgba(data, data + static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4u);
                        stbi_image_free(data);
                        encoded = encode_rgba_png(w, h, rgba, png_compression_level);
                    } else {
                        encoded = read_file_bytes(source_path);
                    }
                } else {
                    throw std::runtime_error("Image has no source");
                }
                const BufferSlice slice = buffer_builder.append_bytes(encoded);
                const int view_index = cJSON_GetArraySize(buffer_views);
                cJSON *view = require_json(cJSON_CreateObject(), "image bufferView");
                cJSON_AddItemToObject(view, "buffer", json_number(0));
                cJSON_AddItemToObject(view, "byteOffset", json_number(slice.offset));
                cJSON_AddItemToObject(view, "byteLength", json_number(slice.length));
                cJSON_AddItemToArray(buffer_views, view);
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
                        unsigned char *data = stbi_load(source_path.string().c_str(), &w, &h, &n, 4);
                        if (!data || w <= 0 || h <= 0) {
                            if (data) {
                                stbi_image_free(data);
                            }
                            throw std::runtime_error("Failed to decode pattern PNG: " + source_path.string());
                        }
                        std::vector<std::uint8_t> rgba(
                            data, data + static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4u);
                        stbi_image_free(data);
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

    cJSON *cameras_json = require_json(cJSON_CreateArray(), "cameras");
    for (const auto &camera : scene.cameras) {
        if (!camera.perspective) {
            continue;
        }
        cJSON *cam = require_json(cJSON_CreateObject(), "camera");
        cJSON_AddItemToObject(cam, "name", json_string(camera.name));
        cJSON_AddItemToObject(cam, "type", json_string("perspective"));
        cJSON *perspective = require_json(cJSON_CreateObject(), "perspective");
        cJSON_AddItemToObject(perspective, "yfov", json_number(camera.perspective->yfov_radians));
        cJSON_AddItemToObject(perspective, "znear", json_number(camera.perspective->znear));
        if (camera.perspective->aspect_ratio) {
            cJSON_AddItemToObject(perspective, "aspectRatio", json_number(*camera.perspective->aspect_ratio));
        }
        if (camera.perspective->zfar) {
            cJSON_AddItemToObject(perspective, "zfar", json_number(*camera.perspective->zfar));
        }
        cJSON_AddItemToObject(cam, "perspective", perspective);
        cJSON_AddItemToArray(cameras_json, cam);
    }

    cJSON *nodes_json = require_json(cJSON_CreateArray(), "nodes");
    for (const auto &node : scene.nodes) {
        cJSON *n = require_json(cJSON_CreateObject(), "node");
        cJSON_AddItemToObject(n, "name", json_string(node.name));
        if (node.translation) {
            cJSON_AddItemToObject(n, "translation", json_float_array_as_double(*node.translation));
        }
        if (node.rotation) {
            cJSON_AddItemToObject(n, "rotation", json_float_array_as_double(*node.rotation));
        }
        if (node.scale) {
            cJSON_AddItemToObject(n, "scale", json_float_array_as_double(*node.scale));
        }
        if (node.matrix && !node.translation && !node.rotation && !node.scale) {
            cJSON_AddItemToObject(n, "matrix", json_double_array(scene::row_major_to_column_major_list(*node.matrix)));
        }
        if (node.mesh_index) {
            cJSON_AddItemToObject(n, "mesh", json_number(*node.mesh_index));
        }
        if (node.camera_index) {
            cJSON_AddItemToObject(n, "camera", json_number(*node.camera_index));
        }
        if (!node.children.empty()) {
            cJSON_AddItemToObject(n, "children", json_int_array(node.children));
        }
        cJSON_AddItemToArray(nodes_json, n);
    }

    cJSON *materials_json = require_json(cJSON_CreateArray(), "materials");
    for (const auto &material : scene.materials) {
        cJSON_AddItemToArray(materials_json, material_to_json(material));
    }

    cJSON *textures_json = require_json(cJSON_CreateArray(), "textures");
    for (const auto &texture : scene.textures) {
        cJSON *t = require_json(cJSON_CreateObject(), "texture");
        cJSON_AddItemToObject(t, "name", json_string(texture.name));
        cJSON_AddItemToObject(t, "source", json_number(texture.image_index));
        if (texture.sampler_index) {
            cJSON_AddItemToObject(t, "sampler", json_number(*texture.sampler_index));
        }
        cJSON_AddItemToArray(textures_json, t);
    }

    cJSON *images_json = require_json(cJSON_CreateArray(), "images");
    for (std::size_t i = 0; i < scene.images.size(); ++i) {
        cJSON *im = require_json(cJSON_CreateObject(), "image");
        cJSON_AddItemToObject(im, "name", json_string(scene.images[i].name));
        if (write_glb) {
            cJSON_AddItemToObject(im, "mimeType", json_string(image_mime_types[i]));
            cJSON_AddItemToObject(im, "bufferView", json_number(image_buffer_views[i]));
        } else {
            cJSON_AddItemToObject(im, "uri", json_string(image_uris[i]));
        }
        cJSON_AddItemToArray(images_json, im);
    }

    cJSON *samplers_json = require_json(cJSON_CreateArray(), "samplers");
    for (const auto &sampler : scene.samplers) {
        cJSON *s = require_json(cJSON_CreateObject(), "sampler");
        cJSON_AddItemToObject(s, "wrapS", json_number(sampler.wrap_s));
        cJSON_AddItemToObject(s, "wrapT", json_number(sampler.wrap_t));
        if (sampler.mag_filter) {
            cJSON_AddItemToObject(s, "magFilter", json_number(*sampler.mag_filter));
        }
        if (sampler.min_filter) {
            cJSON_AddItemToObject(s, "minFilter", json_number(*sampler.min_filter));
        }
        cJSON_AddItemToArray(samplers_json, s);
    }

    cJSON *asset = require_json(cJSON_CreateObject(), "asset");
    cJSON_AddItemToObject(asset, "version", json_string("2.0"));
    cJSON_AddItemToObject(asset, "generator", json_string("slop"));
    cJSON_AddItemToObject(root.get(), "asset", asset);
    cJSON_AddItemToObject(root.get(), "scene", json_number(0));

    cJSON *scenes = require_json(cJSON_CreateArray(), "scenes");
    cJSON *scene0 = require_json(cJSON_CreateObject(), "scene0");
    cJSON_AddItemToObject(scene0, "nodes", json_int_array(scene.root_node_indices));
    cJSON_AddItemToArray(scenes, scene0);
    cJSON_AddItemToObject(root.get(), "scenes", scenes);

    cJSON_AddItemToObject(root.get(), "nodes", nodes_json);
    cJSON_AddItemToObject(root.get(), "meshes", meshes_json);

    auto add_buffer_view = [&](const BufferSlice &slice) -> int {
        const int index = cJSON_GetArraySize(buffer_views);
        cJSON *json = require_json(cJSON_CreateObject(), "bufferView");
        cJSON_AddItemToObject(json, "buffer", json_number(0));
        cJSON_AddItemToObject(json, "byteOffset", json_number(slice.offset));
        cJSON_AddItemToObject(json, "byteLength", json_number(slice.length));
        cJSON_AddItemToArray(buffer_views, json);
        return index;
    };
    auto add_accessor = [&](int buffer_view_index, int component_type, int count, const char *type,
                            const std::optional<std::vector<double>> &min_v,
                            const std::optional<std::vector<double>> &max_v) -> int {
        const int index = cJSON_GetArraySize(accessors);
        cJSON *json = require_json(cJSON_CreateObject(), "accessor");
        cJSON_AddItemToObject(json, "bufferView", json_number(buffer_view_index));
        cJSON_AddItemToObject(json, "componentType", json_number(component_type));
        cJSON_AddItemToObject(json, "count", json_number(count));
        cJSON_AddItemToObject(json, "type", json_string(type));
        if (min_v) {
            cJSON_AddItemToObject(json, "min", json_double_array(*min_v));
        }
        if (max_v) {
            cJSON_AddItemToObject(json, "max", json_double_array(*max_v));
        }
        cJSON_AddItemToArray(accessors, json);
        return index;
    };

    cJSON *animations_json = require_json(cJSON_CreateArray(), "animations");
    for (const auto &animation : scene.animations) {
        if (animation.channels.empty() || animation.samplers.empty()) {
            continue;
        }
        cJSON *anim = require_json(cJSON_CreateObject(), "animation");
        cJSON_AddItemToObject(anim, "name", json_string(animation.name));
        cJSON *samplers_json_anim = require_json(cJSON_CreateArray(), "animation samplers");
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

            cJSON *s = require_json(cJSON_CreateObject(), "animation sampler");
            cJSON_AddItemToObject(s, "input", json_number(time_acc));
            cJSON_AddItemToObject(s, "output", json_number(value_acc));
            cJSON_AddItemToObject(s, "interpolation", json_string(sampler.interpolation));
            cJSON_AddItemToArray(samplers_json_anim, s);
        }
        cJSON *channels_json = require_json(cJSON_CreateArray(), "animation channels");
        for (const auto &channel : animation.channels) {
            cJSON *ch = require_json(cJSON_CreateObject(), "animation channel");
            cJSON_AddItemToObject(ch, "sampler", json_number(channel.sampler_index));
            cJSON *target = require_json(cJSON_CreateObject(), "animation target");
            cJSON_AddItemToObject(target, "node", json_number(channel.node_index));
            cJSON_AddItemToObject(target, "path", json_string(channel.path));
            cJSON_AddItemToObject(ch, "target", target);
            cJSON_AddItemToArray(channels_json, ch);
        }
        cJSON_AddItemToObject(anim, "samplers", samplers_json_anim);
        cJSON_AddItemToObject(anim, "channels", channels_json);
        cJSON_AddItemToArray(animations_json, anim);
    }

    cJSON_AddItemToObject(root.get(), "accessors", accessors);
    cJSON_AddItemToObject(root.get(), "bufferViews", buffer_views);

    cJSON *buffers = require_json(cJSON_CreateArray(), "buffers");
    cJSON *buffer0 = require_json(cJSON_CreateObject(), "buffer0");
    if (!write_glb) {
        cJSON_AddItemToObject(buffer0, "uri", json_string(bin_path.filename().string()));
    }
    cJSON_AddItemToObject(buffer0, "byteLength", json_number(buffer_builder.size()));
    cJSON_AddItemToArray(buffers, buffer0);
    cJSON_AddItemToObject(root.get(), "buffers", buffers);

    if (cJSON_GetArraySize(materials_json) > 0) {
        cJSON_AddItemToObject(root.get(), "materials", materials_json);
    } else {
        cJSON_Delete(materials_json);
    }
    if (cJSON_GetArraySize(textures_json) > 0) {
        cJSON_AddItemToObject(root.get(), "textures", textures_json);
    } else {
        cJSON_Delete(textures_json);
    }
    if (cJSON_GetArraySize(images_json) > 0) {
        cJSON_AddItemToObject(root.get(), "images", images_json);
    } else {
        cJSON_Delete(images_json);
    }
    if (cJSON_GetArraySize(samplers_json) > 0) {
        cJSON_AddItemToObject(root.get(), "samplers", samplers_json);
    } else {
        cJSON_Delete(samplers_json);
    }
    if (cJSON_GetArraySize(cameras_json) > 0) {
        cJSON_AddItemToObject(root.get(), "cameras", cameras_json);
    } else {
        cJSON_Delete(cameras_json);
    }
    if (cJSON_GetArraySize(animations_json) > 0) {
        cJSON_AddItemToObject(root.get(), "animations", animations_json);
    } else {
        cJSON_Delete(animations_json);
    }

    std::unique_ptr<char, CJsonPrintDeleter> printed(cJSON_PrintUnformatted(root.get()));
    if (!printed) {
        throw std::runtime_error("cJSON_PrintUnformatted failed");
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
            std::unique_ptr<char, CJsonPrintDeleter> pretty(cJSON_Print(root.get()));
            if (!pretty) {
                throw std::runtime_error("cJSON_Print failed");
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
} // namespace slop
