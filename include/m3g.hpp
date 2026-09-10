/*
    m3g.hpp -- JSR-184 / M3G decode + glTF convert (C++17)

    Project: m3g

    Do this:
        #define M3G_IMPL
    before you include this file in *one* C++ translation unit to create the
    implementation (stb style).

    Optionally, for a single subsystem only:
        #define M3G_DECODE_IMPL

    In every other translation unit:
        #include <m3g.hpp>

    Repository `include/` (or this file's directory) must be on the compiler
    include path (-Iinclude).

    Optional defines (before include):
        M3G_ASSERT(c)          - your assert macro (default: none / C++ throw paths)
        M3G_DECODE_API_DECL    - decl prefix for decode API (default: empty)
        M3G_API_DECL           - alias for M3G_DECODE_API_DECL
        M3G_API_IMPL           - impl prefix (default: empty)

    Dependencies for M3G_DECODE_IMPL: none beyond the standard library.

    Optional backends are installed via callbacks (no direct miniz/stb in this header):

        // zlib/deflate (section inflate, embedded image inflate)
        m3g::DeflateIo dio = {};
        dio.adler32 = ...;            // like mz_adler32
        dio.uncompress = ...;         // like mz_uncompress (0 = OK)
        dio.uncompress_to_heap = ...; // like tinfl_decompress_mem_to_heap
        dio.free_mem = ...;           // like mz_free
        m3g::set_deflate_io(&dio);

        // raster decode
        m3g::ImageIo iio = {};
        iio.load_file = ...;          // like stbi_load
        iio.load_memory = ...;        // like stbi_load_from_memory
        iio.free_pixels = ...;        // like stbi_image_free
        m3g::set_image_io(&iio);

        // JSON build (glTF export)
        m3g::JsonIo jio = {};
        jio.create_object = ...;      // like cJSON_CreateObject
        jio.create_array = ...;
        jio.create_string = ...;
        jio.create_number = ...;
        jio.create_bool = ...;
        jio.add_item_to_object = ...;
        jio.add_item_to_array = ...;
        jio.get_array_size = ...;
        jio.delete_node = ...;
        jio.print_unformatted = ...;
        jio.print_formatted = ...;
        jio.free_print = ...;
        m3g::set_json_io(&jio);

    Optional adapters in this tree (static-init when linked):
        src/deflate_io_miniz.cpp  -> install_miniz_deflate_io()
        src/image_io_stb.cpp      -> install_stb_image_io()
        src/json_io_cjson.cpp     -> install_cjson_json_io()

    Public decode API:
        m3g::decode::Decoder
        m3g::decode::DecodeOptions
        m3g::decode::Decoded      // .file (M3G object graph) + .scene_ir (export IR)

        Decoder::decode_file(path, options)
        Decoder::decode_bytes(bytes, source_path, options)

    Ownership:
        Decoded is value-semantic (RAII). No free() required.
        Throws std::runtime_error (and related) on parse / I/O failure.

    Example (one .cpp provides the impl):

        // m3g_impl.cpp
        #define M3G_IMPL
        #include <m3g.hpp>

        // main.cpp
        #include <m3g.hpp>
        int main() {
            m3g::decode::Decoder dec;
            auto d = dec.decode_file("model.m3g");
            // use d.scene_ir / d.file
        }

    C metadata stub: #include <m3g.h> (version macros; pulls this header in C++).

    Convert / export types (Converter, GltfExporter) are declared here; their
    implementations still live in src/ until M3G_CONVERT_IMPL is added.
*/

#ifndef M3G_HPP_INCLUDED
#define M3G_HPP_INCLUDED

#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

/* stb-style: one #define M3G_IMPL enables all module impls, then is cleared. */
#ifdef M3G_IMPL
#ifndef M3G_DECODE_IMPL
#define M3G_DECODE_IMPL
#endif
/* #ifndef M3G_CONVERT_IMPL
#define M3G_CONVERT_IMPL
#endif */
#undef M3G_IMPL
#endif

#if defined(M3G_API_DECL) && !defined(M3G_DECODE_API_DECL)
#define M3G_DECODE_API_DECL M3G_API_DECL
#endif
#ifndef M3G_DECODE_API_DECL
#define M3G_DECODE_API_DECL
#endif
#ifndef M3G_API_IMPL
#define M3G_API_IMPL
#endif

#define M3G_VERSION_MAJOR 0
#define M3G_VERSION_MINOR 1
#define M3G_VERSION_PATCH 0

#ifndef M3G_IDENTIFIER_LEN
#define M3G_IDENTIFIER_LEN 12
#endif

namespace m3g {

/*
    Deflate / zlib callbacks (miniz-shaped).

    adler32:
      - Running Adler-32. Pass ptr=NULL to get the initial seed (normally 1).
    uncompress:
      - Inflate zlib-wrapped deflate into *dest_len bytes at dest.
      - On entry *dest_len is capacity; on success *dest_len is output size.
      - Return 0 on success (DEFLATE_OK), non-zero on failure.
    uncompress_to_heap:
      - Inflate when output size is unknown. zlib_header != 0 => zlib wrap,
        0 => raw deflate. Returns malloc'd buffer (free with free_mem) or NULL.
    free_mem:
      - Release pointer from uncompress_to_heap (NULL-safe).

    Default: unset. Link deflate_io_miniz.cpp or call set_deflate_io() before
    decode that needs compression.
*/
enum { DEFLATE_OK = 0 };
enum { ADLER32_INIT = 1 };

struct DeflateIo {
    std::uint32_t (*adler32)(std::uint32_t adler, unsigned char const *ptr, std::size_t buf_len, void *user) = nullptr;
    int (*uncompress)(unsigned char *dest, std::size_t *dest_len, unsigned char const *source, std::size_t source_len,
                      void *user) = nullptr;
    void *(*uncompress_to_heap)(unsigned char const *source, std::size_t source_len, std::size_t *out_len,
                                int zlib_header, void *user) = nullptr;
    void (*free_mem)(void *p, void *user) = nullptr;
    void *user = nullptr;
};

void set_deflate_io(DeflateIo const *io);
DeflateIo const *deflate_io(void);
void install_miniz_deflate_io(void);

inline std::uint32_t deflate_adler32(std::uint32_t adler, unsigned char const *ptr, std::size_t buf_len) {
    DeflateIo const *io = deflate_io();
    if (!io || !io->adler32) {
        throw std::runtime_error("m3g deflate I/O: adler32 callback not set (call m3g::set_deflate_io or install_miniz_deflate_io)");
    }
    return io->adler32(adler, ptr, buf_len, io->user);
}

inline int deflate_uncompress(unsigned char *dest, std::size_t *dest_len, unsigned char const *source,
                              std::size_t source_len) {
    DeflateIo const *io = deflate_io();
    if (!io || !io->uncompress) {
        throw std::runtime_error("m3g deflate I/O: uncompress callback not set (call m3g::set_deflate_io or install_miniz_deflate_io)");
    }
    return io->uncompress(dest, dest_len, source, source_len, io->user);
}

inline void *deflate_uncompress_to_heap(unsigned char const *source, std::size_t source_len, std::size_t *out_len,
                                        int zlib_header) {
    DeflateIo const *io = deflate_io();
    if (!io || !io->uncompress_to_heap) {
        throw std::runtime_error("m3g deflate I/O: uncompress_to_heap callback not set (call m3g::set_deflate_io or install_miniz_deflate_io)");
    }
    return io->uncompress_to_heap(source, source_len, out_len, zlib_header, io->user);
}

inline void deflate_free(void *p) {
    if (!p) {
        return;
    }
    DeflateIo const *io = deflate_io();
    if (!io || !io->free_mem) {
        throw std::runtime_error("m3g deflate I/O: free_mem callback not set");
    }
    io->free_mem(p, io->user);
}

/*
    Image I/O callbacks (stb_image-shaped).

    load_file / load_memory:
      - path or memory buffer in
      - *width, *height, *channels_in_file out (channels before req_comp conversion)
      - req_comp: 0 = original, 4 = force RGBA8
      - return pixel pointer on success, NULL on failure
    free_pixels:
      - release pointer returned by load_* (may be NULL-safe)

    Default: unset (loads fail with a clear error). Link image_io_stb.cpp or
    call set_image_io() / install_stb_image_io() before decode/export that
    needs raster decode.
*/
struct ImageIo {
    unsigned char *(*load_file)(char const *filename, int *width, int *height, int *channels_in_file, int req_comp,
                                void *user) = nullptr;
    unsigned char *(*load_memory)(unsigned char const *buffer, int len, int *width, int *height, int *channels_in_file,
                                  int req_comp, void *user) = nullptr;
    void (*free_pixels)(void *pixels, void *user) = nullptr;
    void *user = nullptr;
};

/* Copy io into process-global state. Pass nullptr to clear. */
void set_image_io(ImageIo const *io);
ImageIo const *image_io(void);

/* Optional: wire stb_image (defined in src/image_io_stb.cpp when linked). */
void install_stb_image_io(void);

/* Helpers used by decode/export (throw on missing callbacks / load failure). */
inline unsigned char *image_load_file(char const *filename, int *width, int *height, int *channels_in_file,
                                     int req_comp) {
    ImageIo const *io = image_io();
    if (!io || !io->load_file) {
        throw std::runtime_error("m3g image I/O: load_file callback not set (call m3g::set_image_io or install_stb_image_io)");
    }
    return io->load_file(filename, width, height, channels_in_file, req_comp, io->user);
}

inline unsigned char *image_load_memory(unsigned char const *buffer, int len, int *width, int *height,
                                       int *channels_in_file, int req_comp) {
    ImageIo const *io = image_io();
    if (!io || !io->load_memory) {
        throw std::runtime_error("m3g image I/O: load_memory callback not set (call m3g::set_image_io or install_stb_image_io)");
    }
    return io->load_memory(buffer, len, width, height, channels_in_file, req_comp, io->user);
}

inline void image_free_pixels(void *pixels) {
    if (!pixels) {
        return;
    }
    ImageIo const *io = image_io();
    if (!io || !io->free_pixels) {
        throw std::runtime_error("m3g image I/O: free_pixels callback not set");
    }
    io->free_pixels(pixels, io->user);
}

/*
    JSON tree callbacks (cJSON-shaped). Nodes are opaque void*.

    create_* return a new node or NULL on OOM.
    add_item_to_object / add_item_to_array take ownership of item on success.
    get_array_size returns element count.
    delete_node frees a root (and children).
    print_unformatted / print_formatted return malloc'd C string; free with free_print.
*/
struct JsonIo {
    void *(*create_object)(void *user) = nullptr;
    void *(*create_array)(void *user) = nullptr;
    void *(*create_string)(char const *s, void *user) = nullptr;
    void *(*create_number)(double v, void *user) = nullptr;
    void *(*create_bool)(int v, void *user) = nullptr;
    void (*add_item_to_object)(void *object, char const *key, void *item, void *user) = nullptr;
    void (*add_item_to_array)(void *array, void *item, void *user) = nullptr;
    int (*get_array_size)(void const *array, void *user) = nullptr;
    void (*delete_node)(void *node, void *user) = nullptr;
    char *(*print_unformatted)(void *node, void *user) = nullptr;
    char *(*print_formatted)(void *node, void *user) = nullptr;
    void (*free_print)(char *printed, void *user) = nullptr;
    void *user = nullptr;
};

void set_json_io(JsonIo const *io);
JsonIo const *json_io(void);
void install_cjson_json_io(void);

inline JsonIo const *require_json_io(char const *what) {
    JsonIo const *io = json_io();
    if (!io) {
        throw std::runtime_error(std::string("m3g JSON I/O: not set (need ") + what +
                                 "; call m3g::set_json_io or install_cjson_json_io)");
    }
    return io;
}

inline void *json_create_object() {
    JsonIo const *io = require_json_io("create_object");
    if (!io->create_object) {
        throw std::runtime_error("m3g JSON I/O: create_object callback not set");
    }
    return io->create_object(io->user);
}
inline void *json_create_array() {
    JsonIo const *io = require_json_io("create_array");
    if (!io->create_array) {
        throw std::runtime_error("m3g JSON I/O: create_array callback not set");
    }
    return io->create_array(io->user);
}
inline void *json_create_string(char const *s) {
    JsonIo const *io = require_json_io("create_string");
    if (!io->create_string) {
        throw std::runtime_error("m3g JSON I/O: create_string callback not set");
    }
    return io->create_string(s, io->user);
}
inline void *json_create_number(double v) {
    JsonIo const *io = require_json_io("create_number");
    if (!io->create_number) {
        throw std::runtime_error("m3g JSON I/O: create_number callback not set");
    }
    return io->create_number(v, io->user);
}
inline void *json_create_bool(int v) {
    JsonIo const *io = require_json_io("create_bool");
    if (!io->create_bool) {
        throw std::runtime_error("m3g JSON I/O: create_bool callback not set");
    }
    return io->create_bool(v, io->user);
}
inline void json_add_item_to_object(void *object, char const *key, void *item) {
    JsonIo const *io = require_json_io("add_item_to_object");
    if (!io->add_item_to_object) {
        throw std::runtime_error("m3g JSON I/O: add_item_to_object callback not set");
    }
    io->add_item_to_object(object, key, item, io->user);
}
inline void json_add_item_to_array(void *array, void *item) {
    JsonIo const *io = require_json_io("add_item_to_array");
    if (!io->add_item_to_array) {
        throw std::runtime_error("m3g JSON I/O: add_item_to_array callback not set");
    }
    io->add_item_to_array(array, item, io->user);
}
inline int json_get_array_size(void const *array) {
    JsonIo const *io = require_json_io("get_array_size");
    if (!io->get_array_size) {
        throw std::runtime_error("m3g JSON I/O: get_array_size callback not set");
    }
    return io->get_array_size(array, io->user);
}
inline void json_delete(void *node) {
    if (!node) {
        return;
    }
    JsonIo const *io = require_json_io("delete_node");
    if (!io->delete_node) {
        throw std::runtime_error("m3g JSON I/O: delete_node callback not set");
    }
    io->delete_node(node, io->user);
}
inline char *json_print_unformatted(void *node) {
    JsonIo const *io = require_json_io("print_unformatted");
    if (!io->print_unformatted) {
        throw std::runtime_error("m3g JSON I/O: print_unformatted callback not set");
    }
    return io->print_unformatted(node, io->user);
}
inline char *json_print_formatted(void *node) {
    JsonIo const *io = require_json_io("print_formatted");
    if (!io->print_formatted) {
        throw std::runtime_error("m3g JSON I/O: print_formatted callback not set");
    }
    return io->print_formatted(node, io->user);
}
inline void json_free_print(char *printed) {
    if (!printed) {
        return;
    }
    JsonIo const *io = require_json_io("free_print");
    if (!io->free_print) {
        throw std::runtime_error("m3g JSON I/O: free_print callback not set");
    }
    io->free_print(printed, io->user);
}

namespace model {

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

struct AnimationProperty {
    static constexpr int ALPHA = 256;
    static constexpr int AMBIENT_COLOR = 257;
    static constexpr int COLOR = 258;
    static constexpr int CROP = 259;
    static constexpr int DENSITY = 260;
    static constexpr int DIFFUSE_COLOR = 261;
    static constexpr int EMISSIVE_COLOR = 262;
    static constexpr int FAR_DISTANCE = 263;
    static constexpr int FIELD_OF_VIEW = 264;
    static constexpr int INTENSITY = 265;
    static constexpr int MORPH_WEIGHTS = 266;
    static constexpr int NEAR_DISTANCE = 267;
    static constexpr int ORIENTATION = 268;
    static constexpr int PICKABILITY = 269;
    static constexpr int SCALE = 270;
    static constexpr int SHININESS = 271;
    static constexpr int SPECULAR_COLOR = 272;
    static constexpr int SPOT_ANGLE = 273;
    static constexpr int SPOT_EXPONENT = 274;
    static constexpr int TRANSLATION = 275;
    static constexpr int VISIBILITY = 276;
};

struct KeyframeInterpolation {
    static constexpr int LINEAR = 176;
    static constexpr int SLERP = 177;
    static constexpr int SPLINE = 178;
    static constexpr int SQUAD = 179;
    static constexpr int STEP = 180;
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

} // namespace model

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

struct GltfWriteResult {
    std::string gltf_path;
    std::string bin_path;
    std::vector<std::string> image_paths;
};

} // namespace scene

namespace decode {

struct Decoded {
    std::string source_path;
    model::File file;
    scene::SceneIr scene_ir;

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
    std::optional<std::string> pattern_path;
};

class Decoder {
public:
    Decoded decode_file(const std::string &input_path, const DecodeOptions &options = {}) const;
    Decoded decode_bytes(const std::vector<std::uint8_t> &bytes, const std::string &source_path,
                         const DecodeOptions &options = {}) const;
};

} // namespace decode

namespace exp {

struct GltfPaths {
    std::string gltf_path;
    std::string bin_path;
    std::vector<std::string> image_paths;
};

struct ExportReport {
    decode::Decoded decoded;
    GltfPaths paths;
};

class GltfExporter {
public:
    GltfPaths write(const decode::Decoded &decoded, const std::string &output_path, bool overwrite,
                    int png_compression_level = 8) const;
    GltfPaths write(const scene::SceneIr &scene_ir, const std::string &output_path, bool overwrite,
                    int png_compression_level = 8) const;
};

} // namespace exp

class Converter {
public:
    decode::Decoded decode(const std::string &input_path,
                           const std::optional<std::string> &pattern_path = std::nullopt) const;

    exp::GltfPaths export_gltf(const decode::Decoded &decoded, const std::string &output_path, bool overwrite,
                               int png_compression_level = 8) const;

    exp::ExportReport convert(const std::string &input_path, const std::string &output_path, bool overwrite,
                              const std::optional<std::string> &pattern_path = std::nullopt,
                              int png_compression_level = 8) const;
};

} // namespace m3g


/* ---- deflate + image + JSON I/O state ---- */
namespace m3g {

inline DeflateIo &deflate_io_storage() {
    static DeflateIo storage{};
    return storage;
}

inline void set_deflate_io(DeflateIo const *io) {
    if (!io) {
        deflate_io_storage() = DeflateIo{};
        return;
    }
    deflate_io_storage() = *io;
}

inline DeflateIo const *deflate_io(void) {
    DeflateIo const &s = deflate_io_storage();
    if (!s.adler32 && !s.uncompress && !s.uncompress_to_heap && !s.free_mem) {
        return nullptr;
    }
    return &s;
}

inline ImageIo &image_io_storage() {
    static ImageIo storage{};
    return storage;
}

inline void set_image_io(ImageIo const *io) {
    if (!io) {
        image_io_storage() = ImageIo{};
        return;
    }
    image_io_storage() = *io;
}

inline ImageIo const *image_io(void) {
    ImageIo const &s = image_io_storage();
    if (!s.load_file && !s.load_memory && !s.free_pixels) {
        return nullptr;
    }
    return &s;
}

inline JsonIo &json_io_storage() {
    static JsonIo storage{};
    return storage;
}

inline void set_json_io(JsonIo const *io) {
    if (!io) {
        json_io_storage() = JsonIo{};
        return;
    }
    json_io_storage() = *io;
}

inline JsonIo const *json_io(void) {
    JsonIo const &s = json_io_storage();
    if (!s.create_object && !s.create_array && !s.create_string && !s.create_number && !s.create_bool &&
        !s.add_item_to_object && !s.add_item_to_array && !s.get_array_size && !s.delete_node &&
        !s.print_unformatted && !s.print_formatted && !s.free_print) {
        return nullptr;
    }
    return &s;
}

} // namespace m3g

/* ---- always-available matrix helpers (used by decode impl + glTF export) ---- */
#ifndef M3G_MATRIX_UTIL_INCLUDED
#define M3G_MATRIX_UTIL_INCLUDED


#include <cmath>
#include <stdexcept>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace m3g {
namespace scene {

inline std::vector<float> identity_matrix_row_major() {
    return {
        1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f,
    };
}

inline std::vector<float> multiply_row_major(const std::vector<float> &left, const std::vector<float> &right) {
    if (left.size() != 16 || right.size() != 16) {
        throw std::invalid_argument("Matrix multiplication expects 4x4 matrices");
    }
    std::vector<float> result(16);
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            float sum = 0.f;
            for (int index = 0; index < 4; ++index) {
                sum += left[static_cast<std::size_t>(row * 4 + index)] *
                       right[static_cast<std::size_t>(index * 4 + col)];
            }
            result[static_cast<std::size_t>(row * 4 + col)] = sum;
        }
    }
    return result;
}

inline std::vector<float> translation_matrix_row_major(float x, float y, float z) {
    auto m = identity_matrix_row_major();
    m[3] = x;
    m[7] = y;
    m[11] = z;
    return m;
}

inline std::vector<float> scale_matrix_row_major(float x, float y, float z) {
    return {
        x, 0.f, 0.f, 0.f, 0.f, y, 0.f, 0.f, 0.f, 0.f, z, 0.f, 0.f, 0.f, 0.f, 1.f,
    };
}

inline std::vector<float> axis_angle_matrix_row_major(float angle_radians, float axis_x, float axis_y, float axis_z) {
    const float length = std::sqrt(axis_x * axis_x + axis_y * axis_y + axis_z * axis_z);
    if (length < 1e-6f || angle_radians == 0.f) {
        return identity_matrix_row_major();
    }
    const float x = axis_x / length;
    const float y = axis_y / length;
    const float z = axis_z / length;
    const float c = std::cos(angle_radians);
    const float s = std::sin(angle_radians);
    const float t = 1.f - c;
    return {
        t * x * x + c, t * x * y - s * z, t * x * z + s * y, 0.f, t * x * y + s * z, t * y * y + c,
        t * y * z - s * x, 0.f, t * x * z - s * y, t * y * z + s * x, t * z * z + c, 0.f, 0.f, 0.f, 0.f, 1.f,
    };
}

inline std::vector<float> component_transform_to_row_major(const model::ComponentTransform &component) {
    const auto translation =
        translation_matrix_row_major(component.translation[0], component.translation[1], component.translation[2]);
    const auto rotation = axis_angle_matrix_row_major(component.orientation_angle, component.orientation_axis[0],
                                                      component.orientation_axis[1], component.orientation_axis[2]);
    const auto scale = scale_matrix_row_major(component.scale[0], component.scale[1], component.scale[2]);
    return multiply_row_major(translation, multiply_row_major(rotation, scale));
}

inline std::vector<float> node_matrix_row_major(const model::NodeMeta &node_meta) {
    if (node_meta.transformable.general_transform) {
        return *node_meta.transformable.general_transform;
    }
    if (node_meta.transformable.component_transform) {
        return component_transform_to_row_major(*node_meta.transformable.component_transform);
    }
    return identity_matrix_row_major();
}

struct DecomposedTrs {
    std::vector<float> translation{0.f, 0.f, 0.f};
    std::vector<float> rotation{0.f, 0.f, 0.f, 1.f}; // xyzw
    std::vector<float> scale{1.f, 1.f, 1.f};
};

inline float vec3_length(float x, float y, float z) { return std::sqrt(x * x + y * y + z * z); }

inline std::vector<float> quaternion_from_axis_angle_degrees(float angle_degrees, float ax, float ay, float az) {
    const float len = vec3_length(ax, ay, az);
    if (len < 1e-8f || std::abs(angle_degrees) < 1e-8f) {
        return {0.f, 0.f, 0.f, 1.f};
    }
    const float half = (angle_degrees * static_cast<float>(M_PI) / 180.f) * 0.5f;
    const float s = std::sin(half);
    const float c = std::cos(half);
    return {ax / len * s, ay / len * s, az / len * s, c};
}

inline std::vector<float> quaternion_from_row_major_rotation(const float r[3][3]) {
    const float trace = r[0][0] + r[1][1] + r[2][2];
    float x, y, z, w;
    if (trace > 0.f) {
        const float s = std::sqrt(trace + 1.f) * 2.f;
        w = 0.25f * s;
        x = (r[2][1] - r[1][2]) / s;
        y = (r[0][2] - r[2][0]) / s;
        z = (r[1][0] - r[0][1]) / s;
    } else if (r[0][0] > r[1][1] && r[0][0] > r[2][2]) {
        const float s = std::sqrt(1.f + r[0][0] - r[1][1] - r[2][2]) * 2.f;
        w = (r[2][1] - r[1][2]) / s;
        x = 0.25f * s;
        y = (r[0][1] + r[1][0]) / s;
        z = (r[0][2] + r[2][0]) / s;
    } else if (r[1][1] > r[2][2]) {
        const float s = std::sqrt(1.f + r[1][1] - r[0][0] - r[2][2]) * 2.f;
        w = (r[0][2] - r[2][0]) / s;
        x = (r[0][1] + r[1][0]) / s;
        y = 0.25f * s;
        z = (r[1][2] + r[2][1]) / s;
    } else {
        const float s = std::sqrt(1.f + r[2][2] - r[0][0] - r[1][1]) * 2.f;
        w = (r[1][0] - r[0][1]) / s;
        x = (r[0][2] + r[2][0]) / s;
        y = (r[1][2] + r[2][1]) / s;
        z = 0.25f * s;
    }
    const float qlen = std::sqrt(x * x + y * y + z * z + w * w);
    if (qlen < 1e-8f) {
        return {0.f, 0.f, 0.f, 1.f};
    }
    return {x / qlen, y / qlen, z / qlen, w / qlen};
}

inline DecomposedTrs decompose_row_major_trs(const std::vector<float> &matrix) {
    DecomposedTrs out;
    if (matrix.size() != 16) {
        return out;
    }
    out.translation = {matrix[3], matrix[7], matrix[11]};
    float sx = vec3_length(matrix[0], matrix[4], matrix[8]);
    float sy = vec3_length(matrix[1], matrix[5], matrix[9]);
    float sz = vec3_length(matrix[2], matrix[6], matrix[10]);
    const float det = matrix[0] * (matrix[5] * matrix[10] - matrix[6] * matrix[9]) -
                      matrix[1] * (matrix[4] * matrix[10] - matrix[6] * matrix[8]) +
                      matrix[2] * (matrix[4] * matrix[9] - matrix[5] * matrix[8]);
    if (det < 0.f) {
        sx = -sx;
    }
    if (sx == 0.f) {
        sx = 1e-8f;
    }
    if (sy == 0.f) {
        sy = 1e-8f;
    }
    if (sz == 0.f) {
        sz = 1e-8f;
    }
    out.scale = {sx, sy, sz};
    float r[3][3] = {
        {matrix[0] / sx, matrix[1] / sy, matrix[2] / sz},
        {matrix[4] / sx, matrix[5] / sy, matrix[6] / sz},
        {matrix[8] / sx, matrix[9] / sy, matrix[10] / sz},
    };
    out.rotation = quaternion_from_row_major_rotation(r);
    return out;
}

inline std::vector<double> row_major_to_column_major_list(const std::vector<float> &matrix) {
    if (matrix.size() != 16) {
        throw std::invalid_argument("Expected a 4x4 matrix");
    }
    return {
        matrix[0],  matrix[4],  matrix[8],  matrix[12], matrix[1],  matrix[5],  matrix[9],  matrix[13],
        matrix[2],  matrix[6],  matrix[10], matrix[14], matrix[3],  matrix[7],  matrix[11], matrix[15],
    };
}

} // namespace scene
} // namespace m3g

#endif /* M3G_MATRIX_UTIL_INCLUDED */

#endif /* M3G_HPP_INCLUDED */

/* ============================ IMPLEMENTATION ============================ */
#ifdef M3G_DECODE_IMPL
#ifndef M3G_DECODE_IMPL_INCLUDED
#define M3G_DECODE_IMPL_INCLUDED

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <tuple>
#include <vector>
#include <string>
#include <optional>
#include <cstdint>

/* internal: BinaryReader */

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace m3g {
namespace model {

class BinaryReader {
public:
    BinaryReader(const std::uint8_t *data, std::size_t size, std::string label = "buffer")
        : data_(data), size_(size), label_(std::move(label)) {}

    explicit BinaryReader(const std::vector<std::uint8_t> &bytes, std::string label = "buffer")
        : BinaryReader(bytes.data(), bytes.size(), std::move(label)) {}

    std::size_t position() const { return position_; }
    std::size_t size() const { return size_; }
    std::size_t remaining() const { return size_ - position_; }
    bool is_eof() const { return position_ >= size_; }

    std::uint8_t read_u8() {
        ensure_available(1);
        return data_[position_++];
    }

    std::int8_t read_i8() { return static_cast<std::int8_t>(read_u8()); }

    bool read_bool_byte() { return read_u8() != 0; }

    std::int32_t read_i32_le() {
        ensure_available(4);
        std::int32_t result = static_cast<std::int32_t>(data_[position_]) |
                              (static_cast<std::int32_t>(data_[position_ + 1]) << 8) |
                              (static_cast<std::int32_t>(data_[position_ + 2]) << 16) |
                              (static_cast<std::int32_t>(data_[position_ + 3]) << 24);
        position_ += 4;
        return result;
    }

    std::uint32_t read_u32_le() { return static_cast<std::uint32_t>(read_i32_le()); }

    std::int16_t read_i16_le() {
        ensure_available(2);
        std::int16_t result = static_cast<std::int16_t>(
            data_[position_] | (static_cast<std::uint16_t>(data_[position_ + 1]) << 8));
        position_ += 2;
        return result;
    }

    std::uint16_t read_u16_le() { return static_cast<std::uint16_t>(read_i16_le()); }

    float read_f32_le() {
        std::uint32_t bits = read_u32_le();
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }

    std::vector<std::uint8_t> read_bytes(std::size_t length) {
        ensure_available(length);
        std::vector<std::uint8_t> slice(data_ + position_, data_ + position_ + length);
        position_ += length;
        return slice;
    }

    std::string read_cstring() {
        std::size_t terminator = size_;
        for (std::size_t i = position_; i < size_; ++i) {
            if (data_[i] == 0) {
                terminator = i;
                break;
            }
        }
        if (terminator >= size_) {
            fail("Missing null terminator while reading C string");
        }
        std::string result(reinterpret_cast<const char *>(data_ + position_), terminator - position_);
        position_ = terminator + 1;
        return result;
    }

    BinaryReader read_sub_reader(std::size_t length, const std::string &child_label) {
        auto bytes = read_bytes(length);
        owned_chunks_.push_back(std::move(bytes));
        const auto &chunk = owned_chunks_.back();
        return BinaryReader(chunk.data(), chunk.size(), child_label);
    }

    std::vector<std::uint8_t> read_remaining_bytes() { return read_bytes(remaining()); }

    void ensure_fully_consumed(const std::string &context) {
        if (!is_eof()) {
            fail(context + " left " + std::to_string(remaining()) + " unread byte(s)");
        }
    }

private:
    void ensure_available(std::size_t length) {
        if (remaining() < length) {
            fail("Attempted to read " + std::to_string(length) + " byte(s) with only " +
                 std::to_string(remaining()) + " remaining");
        }
    }

    [[noreturn]] void fail(const std::string &message) const {
        throw std::runtime_error(label_ + " @ " + std::to_string(position_) + ": " + message);
    }

    const std::uint8_t *data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t position_ = 0;
    std::string label_;
    // Keep ownership when spawning sub-readers from temporary vectors.
    std::vector<std::vector<std::uint8_t>> owned_chunks_;
};

} // namespace model
} // namespace m3g

/* internal: M3G Parser */


#include <string>
#include <vector>

namespace m3g {
namespace model {

class Parser {
public:
    File parse_path(const std::string &path) const;
    File parse(const std::vector<std::uint8_t> &bytes) const;
};

} // namespace model
} // namespace m3g



#include <cstring>
#include <fstream>
#include <stdexcept>


namespace m3g {
namespace model {
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
        const std::uint32_t checksum = deflate_adler32(static_cast<std::uint32_t>(ADLER32_INIT), checksum_input.data(),
                                                       checksum_input.size());
        if (checksum != expected_checksum) {
            throw std::runtime_error("Section checksum mismatch");
        }

        std::vector<std::uint8_t> object_bytes;
        if (compression_scheme == 0) {
            object_bytes = std::move(payload_bytes);
        } else {
            object_bytes.resize(static_cast<std::size_t>(uncompressed_length));
            std::size_t dest_len = static_cast<std::size_t>(uncompressed_length);
            const int rc = deflate_uncompress(object_bytes.data(), &dest_len, payload_bytes.data(), payload_bytes.size());
            if (rc != DEFLATE_OK) {
                throw std::runtime_error("Failed to inflate compressed M3G section (deflate rc " +
                                         std::to_string(rc) + ")");
            }
            if (dest_len != static_cast<std::size_t>(uncompressed_length)) {
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

} // namespace model
} // namespace m3g

/* internal: Scene builder */


#include <string>

namespace m3g {
namespace scene {

class SceneBuilder {
public:
    SceneBuilder(const model::File &file, std::string input_path);

    SceneIr build();

private:
    const model::File &file_;
    std::string input_path_;
};

} // namespace scene
} // namespace m3g



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

namespace m3g {
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
    BuilderImpl(const model::File &file, std::string input_path)
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
        build_animations();
        scene.animations = animations_;
        for (const auto &kv : warnings_) {
            scene.warnings.push_back(kv.second);
        }
        return scene;
    }

private:
    const model::File &file_;
    std::string input_path_;

    std::map<std::string, ConversionWarning> warnings_;
    std::vector<SceneNodeIr> nodes_;
    std::vector<SceneMeshIr> meshes_;
    std::vector<SceneMaterialIr> materials_;
    std::vector<SceneTextureIr> textures_;
    std::vector<SceneImageIr> images_;
    std::vector<SceneSamplerIr> samplers_;
    std::vector<SceneCameraIr> cameras_;
    std::vector<SceneAnimationIr> animations_;

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
            if (std::dynamic_pointer_cast<model::AnimationControllerObject>(obj) ||
                std::dynamic_pointer_cast<model::AnimationTrackObject>(obj) ||
                std::dynamic_pointer_cast<model::KeyframeSequenceObject>(obj)) {
                has_anim = true;
            }
            if (auto u = std::dynamic_pointer_cast<model::UnknownObject>(obj)) {
                has_unknown = true;
                if (u->object_type == model::ObjectTypes::MORPHING_MESH) {
                    has_morph = true;
                }
                if (!unknown_types.empty()) {
                    unknown_types += ", ";
                }
                unknown_types += std::to_string(u->object_type) + " (" + u->type_name() + ")";
            }
            if (std::dynamic_pointer_cast<model::LightObject>(obj)) {
                has_light = true;
            }
            if (std::dynamic_pointer_cast<model::FogObject>(obj)) {
                has_fog = true;
            }
            if (auto bg = std::dynamic_pointer_cast<model::BackgroundObject>(obj)) {
                if (bg->background_image_id) {
                    has_bg_img = true;
                }
            }
            if (auto sk = std::dynamic_pointer_cast<model::SkinnedMeshObject>(obj)) {
                if (!sk->bone_transforms.empty()) {
                    has_skin = true;
                }
            }
        }
        (void)has_anim;
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
            if (std::dynamic_pointer_cast<model::NodeObject>(obj)) {
                node_ids.push_back(obj->object_id);
            }
        }
        std::sort(node_ids.begin(), node_ids.end());
        if (node_ids.empty()) {
            return {};
        }

        std::set<int> child_reference_ids;
        for (const auto &obj : file_.objects_in_order()) {
            if (auto group = std::dynamic_pointer_cast<model::GroupLikeObject>(obj)) {
                for (int id : group->child_ids) {
                    child_reference_ids.insert(id);
                }
                if (auto world = std::dynamic_pointer_cast<model::WorldObject>(obj)) {
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

    std::string synthetic_name(const model::Object &obj) {
        int suffix = 0;
        if (auto n = dynamic_cast<const model::NodeObject *>(&obj)) {
            suffix = n->node_meta.transformable.object3d.user_id;
        } else if (auto v = dynamic_cast<const model::VertexArrayObject *>(&obj)) {
            suffix = v->object3d.user_id;
        } else if (auto vb = dynamic_cast<const model::VertexBufferObject *>(&obj)) {
            suffix = vb->object3d.user_id;
        } else if (auto a = dynamic_cast<const model::AppearanceObject *>(&obj)) {
            suffix = a->object3d.user_id;
        } else if (auto m = dynamic_cast<const model::MaterialObject *>(&obj)) {
            suffix = m->object3d.user_id;
        } else if (auto t = dynamic_cast<const model::Texture2DObject *>(&obj)) {
            suffix = t->transformable.object3d.user_id;
        } else if (auto im = dynamic_cast<const model::Image2DObject *>(&obj)) {
            suffix = im->object3d.user_id;
        } else if (auto bg = dynamic_cast<const model::BackgroundObject *>(&obj)) {
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
        if (auto world = std::dynamic_pointer_cast<model::WorldObject>(obj)) {
            return build_group_node(*world, world->active_camera_id);
        }
        if (auto group = std::dynamic_pointer_cast<model::GroupLikeObject>(obj)) {
            return build_group_node(*group, std::nullopt);
        }
        if (auto mesh = std::dynamic_pointer_cast<model::MeshLikeObject>(obj)) {
            return build_mesh_node(*mesh);
        }
        if (auto cam = std::dynamic_pointer_cast<model::CameraObject>(obj)) {
            return build_camera_node(*cam);
        }
        if (auto light = std::dynamic_pointer_cast<model::LightObject>(obj)) {
            return build_light_node(*light);
        }
        warn("unsupported-node-" + std::to_string(obj->object_id),
             "Unsupported node object " + obj->type_name() + " (" + std::to_string(obj->object_id) +
                 ") was skipped.");
        return std::nullopt;
    }

    int build_group_node(model::GroupLikeObject &group, std::optional<int> additional_child_id) {
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

    int build_mesh_node(model::MeshLikeObject &mesh_object) {
        const int mesh_index = build_mesh(mesh_object.object_id, mesh_object);
        return register_node(mesh_object.object_id, mesh_object, mesh_index, std::nullopt);
    }

    int build_camera_node(model::CameraObject &camera_object) {
        auto camera_index = build_camera(camera_object);
        return register_node(camera_object.object_id, camera_object, std::nullopt, camera_index);
    }

    int build_light_node(model::LightObject &light_object) {
        return register_node(light_object.object_id, light_object, std::nullopt, std::nullopt);
    }

    int register_node(int object_id, model::NodeObject &obj, std::optional<int> mesh_index,
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
        if (!model::is_identity_row_major(matrix)) {
            scene_node.matrix = matrix;
        }
        scene_node.mesh_index = mesh_index;
        scene_node.camera_index = camera_index;
        const int index = static_cast<int>(nodes_.size());
        nodes_.push_back(std::move(scene_node));
        node_index_by_object_id_[object_id] = index;
        return index;
    }

    void ensure_node_trs(int node_index) {
        auto &node = nodes_[static_cast<std::size_t>(node_index)];
        if (node.translation) {
            return;
        }
        const std::vector<float> matrix =
            node.matrix ? *node.matrix : identity_matrix_row_major();
        const auto trs = decompose_row_major_trs(matrix);
        node.translation = trs.translation;
        node.rotation = trs.rotation;
        node.scale = trs.scale;
        node.matrix.reset();
    }

    static const char *interpolation_name(int interpolation) {
        if (interpolation == model::KeyframeInterpolation::STEP) {
            return "STEP";
        }
        return "LINEAR";
    }

    static float sequence_time_to_seconds(int sequence_time, const model::AnimationControllerObject *controller) {
        float world = static_cast<float>(sequence_time);
        if (controller && std::abs(controller->speed) > 1e-8f) {
            world = (static_cast<float>(sequence_time) - controller->reference_sequence_time) / controller->speed +
                    static_cast<float>(controller->reference_world_time);
        }
        return world / 1000.f;
    }

    static void align_quaternions(std::vector<float> &values) {
        for (std::size_t i = 4; i + 3 < values.size(); i += 4) {
            const float dot = values[i - 4] * values[i] + values[i - 3] * values[i + 1] +
                              values[i - 2] * values[i + 2] + values[i - 1] * values[i + 3];
            if (dot < 0.f) {
                values[i] = -values[i];
                values[i + 1] = -values[i + 1];
                values[i + 2] = -values[i + 2];
                values[i + 3] = -values[i + 3];
            }
        }
    }

    bool convert_keyframes(const model::KeyframeSequenceObject &seq, const model::AnimationControllerObject *controller,
                           int property_id, SceneAnimationSamplerIr &out) {
        if (seq.keyframes.empty()) {
            return false;
        }
        int first = seq.valid_range_first;
        int last = seq.valid_range_last;
        if (last < 0 || first < 0) {
            first = 0;
            last = static_cast<int>(seq.keyframes.size()) - 1;
        }
        first = std::max(0, first);
        last = std::min(last, static_cast<int>(seq.keyframes.size()) - 1);
        if (last < first) {
            return false;
        }

        const char *path = nullptr;
        int comps = 0;
        if (property_id == model::AnimationProperty::TRANSLATION) {
            path = "translation";
            comps = 3;
        } else if (property_id == model::AnimationProperty::SCALE) {
            path = "scale";
            comps = seq.component_count == 1 ? 1 : 3;
        } else if (property_id == model::AnimationProperty::ORIENTATION) {
            path = "rotation";
            comps = 4;
        } else {
            return false;
        }
        if (seq.component_count < comps && property_id != model::AnimationProperty::SCALE) {
            return false;
        }
        if (property_id == model::AnimationProperty::SCALE && seq.component_count != 1 && seq.component_count < 3) {
            return false;
        }

        out.interpolation = interpolation_name(seq.interpolation);
        if (seq.interpolation == model::KeyframeInterpolation::SPLINE ||
            seq.interpolation == model::KeyframeInterpolation::SQUAD) {
            warn("animation-interp",
                 "Spline/squad keyframe interpolation is exported as LINEAR.");
        }
        out.component_count = property_id == model::AnimationProperty::ORIENTATION
                                  ? 4
                                  : (property_id == model::AnimationProperty::SCALE ? 3 : 3);
        (void)path;

        struct Sample {
            float time;
            std::vector<float> value;
        };
        std::vector<Sample> samples;
        samples.reserve(static_cast<std::size_t>(last - first + 1));
        for (int i = first; i <= last; ++i) {
            const auto &kf = seq.keyframes[static_cast<std::size_t>(i)];
            Sample sample;
            sample.time = sequence_time_to_seconds(kf.time, controller);
            if (property_id == model::AnimationProperty::TRANSLATION) {
                sample.value = {kf.values.size() > 0 ? kf.values[0] : 0.f,
                                kf.values.size() > 1 ? kf.values[1] : 0.f,
                                kf.values.size() > 2 ? kf.values[2] : 0.f};
            } else if (property_id == model::AnimationProperty::SCALE) {
                if (seq.component_count == 1) {
                    const float s = kf.values.empty() ? 1.f : kf.values[0];
                    sample.value = {s, s, s};
                } else {
                    sample.value = {kf.values.size() > 0 ? kf.values[0] : 1.f,
                                    kf.values.size() > 1 ? kf.values[1] : 1.f,
                                    kf.values.size() > 2 ? kf.values[2] : 1.f};
                }
            } else {
                const float angle = kf.values.size() > 0 ? kf.values[0] : 0.f;
                const float ax = kf.values.size() > 1 ? kf.values[1] : 0.f;
                const float ay = kf.values.size() > 2 ? kf.values[2] : 0.f;
                const float az = kf.values.size() > 3 ? kf.values[3] : 1.f;
                sample.value = quaternion_from_axis_angle_degrees(angle, ax, ay, az);
            }
            samples.push_back(std::move(sample));
        }
        std::sort(samples.begin(), samples.end(),
                  [](const Sample &a, const Sample &b) { return a.time < b.time; });

        out.times.clear();
        out.values.clear();
        for (const auto &sample : samples) {
            out.times.push_back(sample.time);
            out.values.insert(out.values.end(), sample.value.begin(), sample.value.end());
        }
        if (property_id == model::AnimationProperty::ORIENTATION) {
            align_quaternions(out.values);
        }
        return !out.times.empty();
    }

    void build_animations() {
        struct PendingChannel {
            int controller_id;
            int node_index;
            std::string path;
            SceneAnimationSamplerIr sampler;
        };
        std::vector<PendingChannel> pending;
        bool any_track = false;

        for (const auto &kv : node_index_by_object_id_) {
            const int object_id = kv.first;
            const int node_index = kv.second;
            auto found = file_.objects_by_id.find(object_id);
            if (found == file_.objects_by_id.end()) {
                continue;
            }
            auto node = std::dynamic_pointer_cast<model::NodeObject>(found->second);
            if (!node) {
                continue;
            }
            for (int track_id : node->node_meta.transformable.object3d.animation_track_ids) {
                any_track = true;
                auto track = std::dynamic_pointer_cast<model::AnimationTrackObject>(file_.object_or_null(track_id));
                if (!track) {
                    warn("animation-track", "Animation track reference is missing or invalid.");
                    continue;
                }
                auto seq = std::dynamic_pointer_cast<model::KeyframeSequenceObject>(
                    file_.object_or_null(track->keyframe_sequence_id));
                if (!seq) {
                    warn("animation-sequence", "Animation track is missing a KeyframeSequence.");
                    continue;
                }
                const char *path = nullptr;
                if (track->property_id == model::AnimationProperty::TRANSLATION) {
                    path = "translation";
                } else if (track->property_id == model::AnimationProperty::SCALE) {
                    path = "scale";
                } else if (track->property_id == model::AnimationProperty::ORIENTATION) {
                    path = "rotation";
                } else {
                    warn("animation-property",
                         "Animation property " + std::to_string(track->property_id) +
                             " is not exported (only translation, orientation, and scale)." );
                    continue;
                }
                const model::AnimationControllerObject *controller = nullptr;
                int controller_id = -1;
                if (track->animation_controller_id) {
                    controller_id = *track->animation_controller_id;
                    if (auto c = std::dynamic_pointer_cast<model::AnimationControllerObject>(
                            file_.object_or_null(track->animation_controller_id))) {
                        controller = c.get();
                    }
                }
                SceneAnimationSamplerIr sampler;
                if (!convert_keyframes(*seq, controller, track->property_id, sampler)) {
                    warn("animation-empty", "Animation track has no usable keyframes.");
                    continue;
                }
                PendingChannel channel;
                channel.controller_id = controller_id;
                channel.node_index = node_index;
                channel.path = path;
                channel.sampler = std::move(sampler);
                pending.push_back(std::move(channel));
            }
        }

        if (pending.empty()) {
            if (any_track) {
                warn("animation", "Animation tracks were present but none could be exported as node TRS.");
            }
            return;
        }

        std::map<int, SceneAnimationIr> by_controller;
        for (auto &channel : pending) {
            ensure_node_trs(channel.node_index);
            auto &anim = by_controller[channel.controller_id];
            if (anim.name.empty()) {
                if (channel.controller_id >= 0) {
                    anim.name = "Animation_" + std::to_string(channel.controller_id);
                } else {
                    anim.name = "Animation";
                }
            }
            SceneAnimationChannelIr ch;
            ch.sampler_index = static_cast<int>(anim.samplers.size());
            ch.node_index = channel.node_index;
            ch.path = channel.path;
            anim.samplers.push_back(std::move(channel.sampler));
            anim.channels.push_back(std::move(ch));
        }
        for (auto &kv : by_controller) {
            animations_.push_back(std::move(kv.second));
        }
    }

    std::optional<int> build_camera(model::CameraObject &camera_object) {
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

    int build_mesh(int object_id, model::MeshLikeObject &mesh_object) {
        auto it = mesh_index_by_object_id_.find(object_id);
        if (it != mesh_index_by_object_id_.end()) {
            return it->second;
        }
        auto vertex_buffer =
            std::dynamic_pointer_cast<model::VertexBufferObject>(file_.object_or_null(mesh_object.vertex_buffer_id));
        if (!vertex_buffer) {
            throw std::runtime_error("Mesh references missing vertex buffer");
        }
        auto positions_array =
            std::dynamic_pointer_cast<model::VertexArrayObject>(file_.object_or_null(vertex_buffer->positions_id));
        if (!positions_array) {
            throw std::runtime_error("Mesh references missing positions array");
        }
        auto positions =
            decode_scaled_array(*positions_array, vertex_buffer->position_scale, vertex_buffer->position_bias, 3);

        std::optional<std::vector<float>> normals;
        if (auto n = std::dynamic_pointer_cast<model::VertexArrayObject>(file_.object_or_null(vertex_buffer->normals_id))) {
            normals = decode_normals(*n);
        }
        std::optional<std::vector<float>> vertex_colors;
        if (auto c = std::dynamic_pointer_cast<model::VertexArrayObject>(file_.object_or_null(vertex_buffer->colors_id))) {
            vertex_colors = decode_vertex_colors(*c);
        }
        std::optional<std::vector<float>> tex_coords0;
        if (!vertex_buffer->tex_coord_bindings.empty()) {
            const auto &binding = vertex_buffer->tex_coord_bindings.front();
            auto vertex_array =
                std::dynamic_pointer_cast<model::VertexArrayObject>(file_.object_or_null(binding.vertex_array_id));
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
            auto index_buffer = std::dynamic_pointer_cast<model::TriangleStripArrayObject>(
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
            std::dynamic_pointer_cast<model::AppearanceObject>(file_.object_or_null(appearance_id));
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
            std::dynamic_pointer_cast<model::MaterialObject>(file_.object_or_null(appearance->material_id));
        auto polygon_mode =
            std::dynamic_pointer_cast<model::PolygonModeObject>(file_.object_or_null(appearance->polygon_mode_id));
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
        auto texture = std::dynamic_pointer_cast<model::Texture2DObject>(file_.object_or_null(texture_id));
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
        if (auto image = std::dynamic_pointer_cast<model::Image2DObject>(image_object)) {
            auto rgba = decode_embedded_image_to_rgba(*image);
            if (!rgba) {
                warn("image-format", "Embedded image uses unsupported format.");
            } else {
                scene_image.name = synthetic_name(*image);
                EmbeddedRgbaImageSource src;
                src.object_id = image->object_id;
                src.pixels = std::move(*rgba);
                src.width = image->width;
                src.height = image->height;
                if (src.width > 0 && src.pixels.size() % 4u == 0) {
                    const int count = static_cast<int>(src.pixels.size() / 4u);
                    if (count != src.width * src.height && count % src.width == 0) {
                        src.height = count / src.width;
                    }
                }
                scene_image.embedded = std::move(src);
                ok = true;
            }
        } else if (auto ext = std::dynamic_pointer_cast<model::ExternalReferenceObject>(image_object)) {
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

    static bool looks_like_zlib(const std::vector<std::uint8_t> &bytes) {
        if (bytes.size() < 2) {
            return false;
        }
        return bytes[0] == 0x78 &&
               (bytes[1] == 0x01 || bytes[1] == 0x5e || bytes[1] == 0x9c || bytes[1] == 0xda);
    }

    static bool looks_like_png_or_jpeg(const std::vector<std::uint8_t> &bytes) {
        if (bytes.size() >= 8 && bytes[0] == 0x89 && bytes[1] == 0x50 && bytes[2] == 0x4e && bytes[3] == 0x47) {
            return true;
        }
        if (bytes.size() >= 3 && bytes[0] == 0xff && bytes[1] == 0xd8 && bytes[2] == 0xff) {
            return true;
        }
        return false;
    }

    static std::optional<std::vector<std::uint8_t>> inflate_zlib(const std::vector<std::uint8_t> &src,
                                                                std::size_t expected) {
        if (src.empty()) {
            return std::nullopt;
        }
        try {
            if (expected > 0) {
                std::vector<std::uint8_t> dest(expected);
                std::size_t dest_len = expected;
                if (deflate_uncompress(dest.data(), &dest_len, src.data(), src.size()) == DEFLATE_OK &&
                    dest_len == expected) {
                    dest.resize(dest_len);
                    return dest;
                }
            }
            std::size_t out_len = 0;
            void *out = deflate_uncompress_to_heap(src.data(), src.size(), &out_len, /*zlib_header=*/1);
            if (!out) {
                out = deflate_uncompress_to_heap(src.data(), src.size(), &out_len, /*zlib_header=*/0);
            }
            if (!out) {
                return std::nullopt;
            }
            std::vector<std::uint8_t> dest(static_cast<const std::uint8_t *>(out),
                                           static_cast<const std::uint8_t *>(out) + out_len);
            deflate_free(out);
            if (expected > 0 && dest.size() != expected) {
                return std::nullopt;
            }
            return dest;
        } catch (const std::runtime_error &) {
            return std::nullopt;
        }
    }

    static std::optional<std::vector<std::uint8_t>> decode_png_jpeg_to_rgba(const std::vector<std::uint8_t> &src,
                                                                           int *out_w, int *out_h) {
        int w = 0, h = 0, n = 0;
        unsigned char *data = nullptr;
        try {
            data = image_load_memory(src.data(), static_cast<int>(src.size()), &w, &h, &n, 4);
        } catch (const std::runtime_error &) {
            return std::nullopt;
        }
        if (!data || w <= 0 || h <= 0) {
            if (data) {
                image_free_pixels(data);
            }
            return std::nullopt;
        }
        std::vector<std::uint8_t> rgba(data, data + static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4u);
        image_free_pixels(data);
        if (out_w) {
            *out_w = w;
        }
        if (out_h) {
            *out_h = h;
        }
        return rgba;
    }

    std::optional<std::vector<std::uint8_t>> decode_embedded_image_to_rgba(const model::Image2DObject &image) {
        if (!image.pixels) {
            return std::nullopt;
        }
        const std::vector<std::uint8_t> *pixels_ptr = &*image.pixels;
        std::vector<std::uint8_t> inflated;

        if (looks_like_png_or_jpeg(*image.pixels)) {
            int w = 0, h = 0;
            auto decoded = decode_png_jpeg_to_rgba(*image.pixels, &w, &h);
            if (decoded) {
                return decoded;
            }
        }

        auto entry_size = component_count_for_image_format(image.format);
        if (!entry_size) {
            return std::nullopt;
        }
        const int pixel_count = image.width * image.height;
        const int expected =
            image.palette.empty() ? pixel_count * *entry_size : pixel_count;

        if (static_cast<int>(image.pixels->size()) != expected || looks_like_zlib(*image.pixels)) {
            auto maybe = inflate_zlib(*image.pixels, static_cast<std::size_t>(expected));
            if (maybe) {
                inflated = std::move(*maybe);
                pixels_ptr = &inflated;
            } else if (static_cast<int>(image.pixels->size()) != expected) {
                int w = 0, h = 0;
                auto decoded = decode_png_jpeg_to_rgba(*image.pixels, &w, &h);
                if (decoded) {
                    return decoded;
                }
                return std::nullopt;
            }
        }

        const auto &pixels = *pixels_ptr;
        std::vector<std::uint8_t> rgba(static_cast<std::size_t>(pixel_count) * 4u);
        if (!image.palette.empty()) {
            if (static_cast<int>(pixels.size()) != pixel_count) {
                throw std::runtime_error("Palettized image size mismatch");
            }
            for (int pixel_index = 0; pixel_index < pixel_count; ++pixel_index) {
                const int palette_index = pixels[static_cast<std::size_t>(pixel_index)] & 0xFF;
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

    static std::vector<float> decode_vertex_colors(const model::VertexArrayObject &array) {
        if (array.component_count != 3 && array.component_count != 4) {
            throw std::runtime_error("Vertex color array component count invalid");
        }
        std::vector<float> values(static_cast<std::size_t>(array.vertex_count * array.component_count));
        for (std::size_t i = 0; i < values.size(); ++i) {
            values[i] = (array.components[i] & 0xFF) / 255.f;
        }
        return values;
    }

    static std::vector<float> decode_scaled_array(const model::VertexArrayObject &array, float scale,
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

    static std::vector<float> decode_normals(const model::VertexArrayObject &array) {
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

    static std::vector<float> decode_tex_coords(const model::VertexArrayObject &array,
                                                const model::TexCoordBinding &binding) {
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
            // Leave V as stored in M3G. Blender's glTF importer already does V=1-V;
            // flipping here makes every island need UV Mirror Y after import.
            values[static_cast<std::size_t>(vertex_index * 2 + 1)] = v;
        }
        return values;
    }

    static std::vector<int> expand_triangle_strips(const model::TriangleStripArrayObject &index_buffer) {
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

SceneBuilder::SceneBuilder(const model::File &file, std::string input_path)
    : file_(file), input_path_(std::move(input_path)) {}

SceneIr SceneBuilder::build() {
    BuilderImpl impl(file_, input_path_);
    return impl.build();
}

} // namespace scene
} // namespace m3g

/* internal: Pattern texture attacher */


#include <optional>
#include <string>

namespace m3g {
namespace scene {

struct PatternTextureAttacher {
    static SceneIr auto_attach(const SceneIr &scene, const std::optional<std::string> &pattern_path);
};

} // namespace scene
} // namespace m3g


#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <limits>
#include <set>

namespace m3g {
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
} // namespace m3g

/* Decoder methods */


#include <filesystem>

namespace m3g {
namespace decode {
namespace {

Decoded build_decoded(model::File file, std::string source_path, const DecodeOptions &options) {
    Decoded decoded;
    decoded.source_path = std::move(source_path);
    decoded.file = std::move(file);

    auto base_scene = scene::SceneBuilder(decoded.file, decoded.source_path).build();
    decoded.scene_ir = scene::PatternTextureAttacher::auto_attach(base_scene, options.pattern_path);
    return decoded;
}

} // namespace

Decoded Decoder::decode_file(const std::string &input_path, const DecodeOptions &options) const {
    model::Parser parser;
    auto file = parser.parse_path(input_path);
    const std::string normalized = std::filesystem::absolute(input_path).lexically_normal().string();
    return build_decoded(std::move(file), normalized, options);
}

Decoded Decoder::decode_bytes(const std::vector<std::uint8_t> &bytes, const std::string &source_path,
                              const DecodeOptions &options) const {
    model::Parser parser;
    auto file = parser.parse(bytes);
    std::string normalized = source_path;
    if (!normalized.empty()) {
        normalized = std::filesystem::absolute(normalized).lexically_normal().string();
    }
    return build_decoded(std::move(file), std::move(normalized), options);
}

} // namespace decode
} // namespace m3g


#endif /* M3G_DECODE_IMPL_INCLUDED */
#undef M3G_DECODE_IMPL
#endif /* M3G_DECODE_IMPL */
