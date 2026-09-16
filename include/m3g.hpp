/*
    m3g.hpp -- JSR-184 / M3G decode + glTF convert (C++17, legacy)

    API class tree and file layout follow:
      references/j2me_mobile_3d-1_1-mrel-spec.pdf
      (Mobile 3D Graphics API Technical Specification Version 1.1, 2005-06-22)

    Preferred portable C99 single-header API (decode + scene IR):
        #include "m3g.h"
        #define M3G_IMPLEMENTATION  // in one .c file
    Requires vecmath.h on the include path (vendors/libs/vecmath.h).

    This C++ header remains for glTF export / existing C++ tooling.

    Do this:
        #define M3G_IMPL
    before you include this file in *one* C++ file to create the
    implementation. Declarations are in the public section; definitions are
    under #ifdef M3G_IMPL / M3G_DECODE_IMPL (not inline).

    Optionally, for decode only:
        #define M3G_DECODE_IMPL

    In every other translation unit:
        #include <m3g.hpp>

    Put include/ on the compiler include path (-Iinclude).

    Optionally provide the following defines with your own implementations:
        M3G_ASSERT(c)             - your own assert macro
        M3G_API_DECL              - public declaration prefix (default: empty)
        M3G_DECODE_API_DECL       - same as M3G_API_DECL if unset
        M3G_API_IMPL              - public implementation prefix (default: empty)

    Backends are not compiled into this header. Install callbacks before
    decode/export (or link the tree adapters that auto-register):

        m3g::DeflateIo   // zlib/deflate     — install_miniz_deflate_io()
        m3g::ImageIo     // raster load      — install_stb_image_io()
        m3g::JsonIo      // optional JSON    — install_cjson_json_io()
        m3g::GltfIo      // glTF write/parse — install_cgltf_gltf_io()

        m3g::set_deflate_io(&dio);
        m3g::set_image_io(&iio);
        m3g::set_json_io(&jio);
        m3g::set_gltf_io(&gio);

    Public decode API (Java M3G-shaped object graph in m3g::model):
        m3g::decode::Decoder / m3g::Loader   // Loader::load like javax.microedition.m3g.Loader
        m3g::decode::DecodeOptions
        m3g::decode::Decoded   // .file (Object3D graph) + .scene_ir (export IR)

        Object3D, Transformable, Node, Group, World, Mesh, SkinnedMesh, ...
        Decoder::decode_file(path, options)
        Decoder::decode_bytes(bytes, source_path, options)
        Loader::load(path) / Loader::load(bytes)

    Convert / export:
        m3g::Converter
        m3g::exp::GltfExporter

    Ownership:
        Decoded is value-semantic (RAII). No free() required.
        Throws std::runtime_error (and related) on parse / I/O failure.

    Example:

        // m3g_impl.cpp
        #define M3G_IMPL
        #include <m3g.hpp>

        // main.cpp
        #include <m3g.hpp>
        int main() {
            m3g::decode::Decoder dec;
            auto d = dec.decode_file("model.m3g");
            // d.file, d.scene_ir
        }

    C metadata stub: #include <m3g.h>
*/

#ifndef M3G_HPP_INCLUDED
#define M3G_HPP_INCLUDED

/**
 * @file m3g.hpp
 * @brief JSR-184 / Mobile 3D Graphics API 1.1 C++ model, decode, and export.
 *
 * Public types under @ref m3g::model and Java-style aliases in @ref m3g mirror
 * `javax.microedition.m3g.*` class and method names from the M3G 1.1 specification
 * (`references/j2me_mobile_3d-1_1-mrel-spec.pdf`). Method documentation follows the
 * Java API reference (parameters, returns, defaults, and behavioral notes).
 *
 * Cross-object links in the decoded graph are typically object ids into
 * @ref m3g::model::File::objects_by_id rather than live Java references.
 */

#include <algorithm>
#include <array>
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

/** @brief Library major version. */
#define M3G_VERSION_MAJOR 0
/** @brief Library minor version. */
#define M3G_VERSION_MINOR 1
/** @brief Library patch version. */
#define M3G_VERSION_PATCH 0

/** @brief Byte length of the M3G file identifier (`\xABJSRI184\xBB\r\n\x1A\n`). */
#ifndef M3G_IDENTIFIER_LEN
#define M3G_IDENTIFIER_LEN 12
#endif

/**
 * @namespace m3g
 * @brief Root namespace for the M3G toolkit.
 *
 * @defgroup m3g_io Backend I/O callbacks
 * @brief Pluggable deflate, image, and JSON backends.
 *
 * @defgroup m3g_model M3G object model
 * @brief Parsed JSR-184 object graph (`m3g::model`).
 *
 * @defgroup m3g_scene Scene IR
 * @brief Export-oriented intermediate representation (`m3g::scene`).
 *
 * @defgroup m3g_decode Decode API
 * @brief Bytes/path → decoded asset (`m3g::decode::Decoded`).
 *
 * @defgroup m3g_export Export / convert API
 * @brief Scene IR → glTF via exporter or facade converter.
 */

namespace m3g {

class Transform; /* used by model::Transformable / Node API (defined below) */

/**
 * @ingroup m3g_io
 * @brief Success code for @ref DeflateIo::uncompress (matches zlib/miniz `Z_OK` / `MZ_OK`).
 */
enum { DEFLATE_OK = 0 };

/**
 * @ingroup m3g_io
 * @brief Initial Adler-32 seed (pass as @p adler when starting a new checksum).
 */
enum { ADLER32_INIT = 1 };

/**
 * @ingroup m3g_io
 * @brief Zlib / deflate function table (miniz-shaped).
 *
 * Install with @ref set_deflate_io or @ref install_miniz_deflate_io.
 * Required for compressed M3G sections and some embedded images.
 */
struct DeflateIo {
    /**
     * @brief Running Adler-32.
     * @param adler Previous sum, or @c ADLER32_INIT to start.
     * @param ptr Data pointer; if `NULL`, returns the initial seed for @p adler.
     * @param buf_len Byte count when @p ptr is non-null.
     * @param user Opaque @c user from this struct.
     */
    std::uint32_t (*adler32)(std::uint32_t adler, unsigned char const *ptr, std::size_t buf_len, void *user) = nullptr;

    /**
     * @brief Inflate zlib-wrapped deflate into a caller-owned buffer.
     * @param[in,out] dest_len On entry: capacity of @p dest; on success: bytes written.
     * @return @c DEFLATE_OK on success; non-zero on failure.
     */
    int (*uncompress)(unsigned char *dest, std::size_t *dest_len, unsigned char const *source, std::size_t source_len,
                      void *user) = nullptr;

    /**
     * @brief Inflate when the output size is unknown.
     * @param zlib_header Non-zero = zlib wrapper; `0` = raw deflate.
     * @param[out] out_len Size of returned buffer.
     * @return Heap block (free with @ref free_mem), or `NULL` on failure.
     */
    void *(*uncompress_to_heap)(unsigned char const *source, std::size_t source_len, std::size_t *out_len,
                                int zlib_header, void *user) = nullptr;

    /** @brief Free a pointer from @c uncompress_to_heap (`NULL`-safe). */
    void (*free_mem)(void *p, void *user) = nullptr;

    /** @brief Opaque pointer passed to every callback. */
    void *user = nullptr;
};

/**
 * @ingroup m3g_io
 * @brief Install process-global deflate backend (copied by value).
 * @param io New table, or `nullptr` to clear.
 */
void set_deflate_io(DeflateIo const *io);

/**
 * @ingroup m3g_io
 * @brief Current deflate backend, or `nullptr` if unset.
 */
DeflateIo const *deflate_io(void);

/**
 * @ingroup m3g_io
 * @brief Register the vendored miniz backend (`src/deflate_io_miniz.cpp`).
 */
void install_miniz_deflate_io(void);

std::uint32_t deflate_adler32(std::uint32_t adler, unsigned char const *ptr, std::size_t buf_len);

int deflate_uncompress(unsigned char *dest, std::size_t *dest_len, unsigned char const *source,
                              std::size_t source_len);

void *deflate_uncompress_to_heap(unsigned char const *source, std::size_t source_len, std::size_t *out_len,
                                        int zlib_header);

void deflate_free(void *p);

/**
 * @ingroup m3g_io
 * @brief Raster image load function table (stb_image-shaped).
 *
 * @p req_comp: `0` = source layout; `4` = force RGBA8.
 * On success returns a pixel pointer owned until @c free_pixels.
 */
struct ImageIo {
    /** @brief Load from filesystem path (like `stbi_load`). */
    unsigned char *(*load_file)(char const *filename, int *width, int *height, int *channels_in_file, int req_comp,
                                void *user) = nullptr;
    /** @brief Load from memory (like `stbi_load_from_memory`). */
    unsigned char *(*load_memory)(unsigned char const *buffer, int len, int *width, int *height, int *channels_in_file,
                                  int req_comp, void *user) = nullptr;
    /** @brief Free pixels from load_* (`NULL`-safe). */
    void (*free_pixels)(void *pixels, void *user) = nullptr;
    /** @brief Opaque pointer passed to every callback. */
    void *user = nullptr;
};

/**
 * @ingroup m3g_io
 * @brief Install process-global image backend.
 * @param io New table, or `nullptr` to clear.
 */
void set_image_io(ImageIo const *io);

/**
 * @brief Current image backend, or `nullptr` if unset.
 * @ingroup m3g_io
 */
ImageIo const *image_io(void);

/**
 * @ingroup m3g_io
 * @brief Register the vendored stb_image backend (`src/image_io_stb.cpp`).
 */
void install_stb_image_io(void);

/** @name Image helpers
 *  @ingroup m3g_io
 *  @brief Throw if callbacks missing; otherwise forward to @ref ImageIo.
 *  @{
 */
unsigned char *image_load_file(char const *filename, int *width, int *height, int *channels_in_file,
                                     int req_comp);

unsigned char *image_load_memory(unsigned char const *buffer, int len, int *width, int *height,
                                       int *channels_in_file, int req_comp);

void image_free_pixels(void *pixels);
/** @} */

/**
 * @ingroup m3g_io
 * @brief JSON DOM function table (cJSON-shaped); nodes are opaque `void*`.
 *
 * @note glTF export primarily uses cgltf_write; this table is optional/legacy.
 */
struct JsonIo {
    void *(*create_object)(void *user) = nullptr;              /**< @brief New `{}` or `NULL` on OOM. */
    void *(*create_array)(void *user) = nullptr;               /**< @brief New `[]` or `NULL` on OOM. */
    void *(*create_string)(char const *s, void *user) = nullptr;
    void *(*create_number)(double v, void *user) = nullptr;
    void *(*create_bool)(int v, void *user) = nullptr;
    /** @brief Attach @p item under @p key; takes ownership of @p item on success. */
    void (*add_item_to_object)(void *object, char const *key, void *item, void *user) = nullptr;
    /** @brief Append @p item; takes ownership on success. */
    void (*add_item_to_array)(void *array, void *item, void *user) = nullptr;
    int (*get_array_size)(void const *array, void *user) = nullptr;
    void (*delete_node)(void *node, void *user) = nullptr;    /**< @brief Free subtree. */
    char *(*print_unformatted)(void *node, void *user) = nullptr; /**< @brief Compact JSON; free with @c free_print. */
    char *(*print_formatted)(void *node, void *user) = nullptr;   /**< @brief Pretty JSON; free with @c free_print. */
    void (*free_print)(char *printed, void *user) = nullptr;
    void *user = nullptr;
};

/**
 * @brief Install process-global JSON backend (`nullptr` clears).
 * @ingroup m3g_io
 */
void set_json_io(JsonIo const *io);
/**
 * @brief Current JSON backend, or `nullptr` if unset.
 * @ingroup m3g_io
 */
JsonIo const *json_io(void);
/**
 * @brief Register vendored cJSON backend (`src/json_io_cjson.cpp`).
 * @ingroup m3g_io
 */
void install_cjson_json_io(void);

/**
 * @brief glTF container kind for @ref GltfIo::write_file.
 * @ingroup m3g_io
 */
enum GltfFileKind {
    GLTF_FILE_JSON = 0, /**< @brief `.gltf` JSON (external bin/images separate). */
    GLTF_FILE_GLB = 1   /**< @brief `.glb` binary container. */
};

/**
 * @brief Success code for @ref GltfIo operations.
 * @ingroup m3g_io
 */
enum { GLTF_IO_OK = 0 };

/**
 * @brief glTF write / parse / validate function table (cgltf-shaped).
 *
 * @p data is an opaque backend document pointer (default adapter: `cgltf_data*`).
 * All functions return @c GLTF_IO_OK (0) on success, non-zero on failure.
 *
 * @ingroup m3g_io
 */
struct GltfIo {
    /**
     * @brief Write a document to @p path.
     * @param kind @ref GLTF_FILE_JSON or @ref GLTF_FILE_GLB.
     * @param data Backend document (`cgltf_data*` for the default adapter).
     */
    int (*write_file)(char const *path, void const *data, int kind, void *user) = nullptr;

    /**
     * @brief Parse a glTF/GLB file into a backend document.
     * @param[out] out_data Receives opaque document; free with @c free_data.
     */
    int (*parse_file)(char const *path, void **out_data, void *user) = nullptr;

    /** @brief Validate a document from @ref parse_file. */
    int (*validate)(void *data, void *user) = nullptr;

    /** @brief Free a document from @ref parse_file (`NULL`-safe). */
    void (*free_data)(void *data, void *user) = nullptr;

    void *user = nullptr;
};

/**
 * @brief Install process-global glTF backend (`nullptr` clears).
 * @ingroup m3g_io
 */
void set_gltf_io(GltfIo const *io);
/**
 * @brief Current glTF backend, or `nullptr` if unset.
 * @ingroup m3g_io
 */
GltfIo const *gltf_io(void);
/**
 * @brief Register vendored cgltf write/parse backend (`src/gltf_io_cgltf.cpp`).
 * @ingroup m3g_io
 */
void install_cgltf_gltf_io(void);

/** @name Gltf helpers
 *  @ingroup m3g_io
 *  @{
 */
void gltf_write_file(char const *path, void const *data, int kind);

void gltf_parse_file(char const *path, void **out_data);

void gltf_validate(void *data);

void gltf_free_data(void *data);
/** @} */

void *json_create_object();
void *json_create_array();
void *json_create_string(char const *s);
void *json_create_number(double v);
void *json_create_bool(int v);
void json_add_item_to_object(void *object, char const *key, void *item);
void json_add_item_to_array(void *array, void *item);
int json_get_array_size(void const *array);
void json_delete(void *node);
char *json_print_unformatted(void *node);
char *json_print_formatted(void *node);
void json_free_print(char *printed);

/**
 * @ingroup m3g_model
 * @namespace m3g::model
 * @brief Parsed M3G (JSR-184) object graph and related constants.
 */
namespace model {

struct Camera;
struct Node;

/**
 * @ingroup m3g_model
 * @brief M3G object type byte values (JSR-184).
 */
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

/**
 * @ingroup m3g_model
 * @brief Animation track target property IDs.
 */
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

/**
 * @ingroup m3g_model
 * @brief KeyframeSequence interpolation constants.
 */
struct KeyframeInterpolation {
    static constexpr int LINEAR = 176;
    static constexpr int SLERP = 177;
    static constexpr int SPLINE = 178;
    static constexpr int SQUAD = 179;
    static constexpr int STEP = 180;
};

/**
 * @ingroup m3g_model
 * @brief Human-readable name for an @ref ObjectTypes value.
 */
std::string type_name_for_object_type(int object_type);

/**
 * @brief 8-bit RGB color.
 * @ingroup m3g_model
 */
struct RgbColor {
    int red = 0;
    int green = 0;
    int blue = 0;
    std::vector<float> to_float_array() const {
        return {red / 255.f, green / 255.f, blue / 255.f};
    }
};

/**
 * @brief 8-bit RGBA color.
 * @ingroup m3g_model
 */
struct RgbaColor {
    int red = 0;
    int green = 0;
    int blue = 0;
    int alpha = 255;
    std::vector<float> to_float_array() const {
        return {red / 255.f, green / 255.f, blue / 255.f, alpha / 255.f};
    }
};

/**
 * @brief One compressed/uncompressed section header from the file.
 * @ingroup m3g_model
 */
struct SectionInfo {
    int index = 0;
    int compression_scheme = 0;
    int total_section_length = 0;
    int uncompressed_length = 0;
};

/**
 * @ingroup m3g_model
 * @brief Polymorphic base for every M3G object in @ref File::objects_by_id.
 *
 * Mirrors the role of the JSR-184 object graph root; scene types further
 * derive from @ref Object3D like `javax.microedition.m3g.Object3D`.
 */
struct Object {
    int object_id = 0;     /**< @brief Sequential id assigned while parsing. */
    int object_type = 0;   /**< @brief @ref ObjectTypes value. */
    int raw_length = 0;    /**< @brief Payload size in the section. */
    virtual ~Object() = default;
    /** @brief @ref type_name_for_object_type for @c object_type. */
    std::string type_name() const { return type_name_for_object_type(object_type); }
};

/**
 * @brief Application user-parameter entry (`Object3D` Hashtable payload).
 * @ingroup m3g_model
 *
 * Spec §11.19: `parameterID` + `Byte[] parameterValue`.
 */
struct UserParameter {
    int parameter_id = 0;
    std::vector<std::uint8_t> value;
};

/**
 * @brief An abstract base class for all objects that can be part of a 3D world.
 * @ingroup m3g_model
 *
 * Java: `javax.microedition.m3g.Object3D`.
 * Spec: Mobile 3D Graphics API 1.1, class Object3D; file format §11.19.
 *
 * This includes the world itself, other scene graph nodes, animations, textures,
 * and so on. Everything in the M3G API is an Object3D except Loader, Transform,
 * RayIntersection, and Graphics3D.
 *
 * @par Animation
 * Animations are applied to an object and its descendants with @ref animate.
 * Tracks are associated via @ref addAnimationTrack.
 *
 * @par Finding objects
 * Every Object3D can be assigned a user ID (@ref setUserID). User IDs are typically
 * used to find a known object in a scene loaded from a data stream. @ref find
 * searches objects reachable through a chain of direct references. Parent and
 * alignment references in a Node do not count as direct references.
 *
 * @par Associated user data
 * The user object may contain arbitrary application data. When loaded from a file,
 * it may be a Hashtable of byte arrays keyed by integers (@ref UserParameter);
 * if there are no user parameters it is initially null.
 *
 * @par Instantiation defaults
 * - user ID: 0
 * - user object: null
 * - animation tracks: none
 *
 * @note In this C++ decode model, cross-object links are stored as object ids in
 * @ref File::objects_by_id rather than live Java references, unless noted.
 *
 * @see AnimationTrack, Loader, File
 */
struct Object3D : virtual Object {
    int user_id = 0; /**< @brief Application user ID (Java `userID`). */
    std::vector<int> animation_track_ids; /**< @brief Bound @ref AnimationTrack object ids. */
    /** @brief Spec Hashtable payload; empty ⇒ null user object (Java `userObject`). */
    std::vector<UserParameter> user_parameters;

    /**
     * @brief Gets the user ID of this object.
     * @return The current user ID.
     * @see setUserID
     */
    int getUserID() const { return user_id; }

    /**
     * @brief Sets the user ID for this object.
     * @param userID The ID to set.
     * @see getUserID
     */
    void setUserID(int userID) { user_id = userID; }

    /**
     * @brief Gets the number of AnimationTracks currently associated with this Object3D.
     * @return The number of AnimationTracks bound to this Object3D.
     */
    int getAnimationTrackCount() const { return static_cast<int>(animation_track_ids.size()); }

    /**
     * @brief Gets an AnimationTrack by index.
     *
     * Valid indices range from zero up to @ref getAnimationTrackCount minus one.
     * Note that the index of any track may change whenever a track is added or removed.
     *
     * @param index Index of the AnimationTrack to retrieve.
     * @return Object id of the AnimationTrack at @p index (Java returns the track reference).
     * @throws std::out_of_range if index is out of bounds.
     */
    int getAnimationTrack(int index) const {
        return animation_track_ids.at(static_cast<std::size_t>(index));
    }

    /**
     * @brief Adds the given AnimationTrack to this Object3D.
     *
     * Potentially changes the order and indices of previously added tracks. The
     * insertion position among existing tracks is deliberately left undefined.
     *
     * @param animationTrackId Object id of a compatible AnimationTrack (non-zero).
     * @throws std::invalid_argument if @p animationTrackId is 0 (null).
     */
    void addAnimationTrack(int animationTrackId) {
        if (animationTrackId == 0) {
            throw std::invalid_argument("Object3D::addAnimationTrack: null track");
        }
        animation_track_ids.push_back(animationTrackId);
    }

    /**
     * @brief Removes the given AnimationTrack from this Object3D.
     *
     * Potentially changes the order and indices of the remaining tracks. If the
     * track is not associated with this object, or id is 0, the request is ignored.
     *
     * @param animationTrackId Object id of the track to detach.
     */
    void removeAnimationTrack(int animationTrackId) {
        auto &v = animation_track_ids;
        v.erase(std::remove(v.begin(), v.end(), animationTrackId), v.end());
    }

    /**
     * @brief Updates all animated properties in this Object3D and all reachable Object3Ds.
     *
     * Animated properties are set to their interpolated values at world time @p time.
     * The unit of time is application-defined (often milliseconds by convention).
     *
     * @param time World time to update the animations to.
     * @return Validity interval: time units until this method needs to be called again
     *         for this or any reachable Object3D. A conservative estimate may be 0.
     *
     * @note Decode/export toolkit: no keyframe sampler is run; always returns 0.
     * @see AnimationTrack, AnimationController, KeyframeSequence
     */
    int animate(int /*time*/) { return 0; }

    /**
     * @brief Retrieves an object that has the given user ID and is reachable from this object.
     *
     * If multiple objects share the same ID, any one may be returned.
     *
     * @param userID The user ID to search for.
     * @return Matching object, or null if none found.
     *
     * @note Without a live @ref File graph walk, only this object's user id is matched.
     */
    Object3D *find(int userID) { return (user_id == userID) ? this : nullptr; }
    /** @copydoc find(int) */
    const Object3D *find(int userID) const { return (user_id == userID) ? this : nullptr; }

    /**
     * @brief Returns the number of direct Object3D references in this object.
     *
     * Fills @p references with object ids when non-null. Duplicate references are not
     * eliminated. Parent and alignment references in a Node do not count; null/0 is omitted.
     *
     * Typical usage: call with @p references == null to get the count, allocate, then call again.
     *
     * @param references Array of object ids to fill, or null to only return the count.
     * @param max_count Capacity of @p references when non-null (C++ adaptation of Java array length).
     * @return Number of direct Object3D references (unique count may be smaller).
     */
    virtual int getReferences(int * /*references*/, int /*max_count*/) const { return 0; }

    /**
     * @brief Creates a duplicate of this Object3D.
     *
     * As a general rule, a duplicate has the same properties as the original, including
     * attribute values and references. For Node subclasses, Java also duplicates the
     * descendant subgraph; this toolkit provides a polymorphic shallow value copy by default.
     *
     * @return A new Object3D that is a duplicate of this object.
     */
    virtual std::shared_ptr<Object3D> duplicate() const {
        return std::make_shared<Object3D>(*this);
    }

    /**
     * @brief Retrieves the user object currently associated with this Object3D.
     *
     * When constructed by the Loader, the user object may initially be a Hashtable of
     * persistent user data (byte arrays keyed by integers).
     *
     * @return Pointer to user parameters, or null if none.
     * @see setUserObject
     */
    const std::vector<UserParameter> *getUserObject() const {
        return user_parameters.empty() ? nullptr : &user_parameters;
    }

    /**
     * @brief Associates an arbitrary, application-specific object with this Object3D.
     *
     * The given user object replaces any previously set object. Stored by value in this port.
     *
     * @param userObject User parameters, or null to remove any existing association.
     * @see getUserObject
     */
    void setUserObject(const std::vector<UserParameter> *userObject) {
        if (!userObject) {
            user_parameters.clear();
        } else {
            user_parameters = *userObject;
        }
    }
    /** @brief Associates user parameters by move (C++ convenience overload). */
    void setUserObject(std::vector<UserParameter> userObject) {
        user_parameters = std::move(userObject);
    }
};

/**
 * @brief Translation, orientation (axis-angle), and scale components of a @ref Transformable.
 * @ingroup m3g_model
 *
 * Corresponds to the T, R, S factors in the Java composite `p' = T R S M p`.
 */
struct ComponentTransform {
    std::vector<float> translation{0, 0, 0}; /**< @brief Translation T; default (0,0,0). */
    std::vector<float> scale{1, 1, 1};       /**< @brief Non-uniform scale S; default (1,1,1). */
    float orientation_angle = 0.f;          /**< @brief Orientation angle in degrees; 0 ⇒ identity. */
    std::vector<float> orientation_axis{0, 0, 1}; /**< @brief Orientation axis; undefined when angle is 0. */
};

/**
 * @brief Abstract base class for Node and Texture2D node/texture transforms.
 * @ingroup m3g_model
 *
 * Java: `javax.microedition.m3g.Transformable` extends Object3D.
 *
 * Node and texture transformations consist of four components: translation (T),
 * orientation (R), scale (S), and a generic 4×4 matrix (M). A homogeneous vector
 * `p = (x, y, z, w)` is transformed as:
 *
 * @code
 *   p' = T R S M p
 * @endcode
 *
 * @par Instantiation defaults
 * - scale: (1,1,1)
 * - translation: (0,0,0)
 * - orientation: angle = 0, axis = undefined
 * - matrix: identity
 *
 * @see Node, Texture2D, Transform
 */
struct Transformable : Object3D {
    std::optional<ComponentTransform> component_transform; /**< @brief T/R/S components when present. */
    /** @brief Generic 4×4 matrix M (row-major); identity if absent. Bottom row must be (0,0,0,1). */
    std::optional<std::vector<float>> general_transform;

    /** @brief Ensures @ref component_transform is allocated (implementation helper). */
    void ensure_component() {
        if (!component_transform) component_transform = ComponentTransform{};
    }

    /**
     * @brief Retrieves the translation component of this Transformable.
     * @param[out] xyz Length ≥ 3; receives (tx, ty, tz).
     */
    void getTranslation(float *xyz) const {
        const auto &t = component_transform ? component_transform->translation : std::vector<float>{0, 0, 0};
        xyz[0] = t.size() > 0 ? t[0] : 0.f;
        xyz[1] = t.size() > 1 ? t[1] : 0.f;
        xyz[2] = t.size() > 2 ? t[2] : 0.f;
    }
    /**
     * @brief Sets the translation component of this Transformable.
     * @param tx X translation.
     * @param ty Y translation.
     * @param tz Z translation.
     */
    void setTranslation(float tx, float ty, float tz) {
        ensure_component();
        component_transform->translation = {tx, ty, tz};
    }
    /**
     * @brief Adds the given offset to the current translation component.
     * @param tx X offset.
     * @param ty Y offset.
     * @param tz Z offset.
     */
    void translate(float tx, float ty, float tz) {
        float cur[3];
        getTranslation(cur);
        setTranslation(cur[0] + tx, cur[1] + ty, cur[2] + tz);
    }

    /**
     * @brief Retrieves the scale component of this Transformable.
     * @param[out] xyz Length ≥ 3; receives (sx, sy, sz).
     */
    void getScale(float *xyz) const {
        const auto &s = component_transform ? component_transform->scale : std::vector<float>{1, 1, 1};
        xyz[0] = s.size() > 0 ? s[0] : 1.f;
        xyz[1] = s.size() > 1 ? s[1] : 1.f;
        xyz[2] = s.size() > 2 ? s[2] : 1.f;
    }
    /**
     * @brief Sets the scale component of this Transformable.
     * @param sx X scale factor.
     * @param sy Y scale factor.
     * @param sz Z scale factor.
     */
    void setScale(float sx, float sy, float sz) {
        ensure_component();
        component_transform->scale = {sx, sy, sz};
    }
    /**
     * @brief Multiplies the current scale component by the given scale factors.
     * @param sx X scale multiplier.
     * @param sy Y scale multiplier.
     * @param sz Z scale multiplier.
     */
    void scale(float sx, float sy, float sz) {
        float cur[3];
        getScale(cur);
        setScale(cur[0] * sx, cur[1] * sy, cur[2] * sz);
    }

    /**
     * @brief Retrieves the orientation component of this Transformable.
     * @param[out] angleAxis Length ≥ 4: angle (degrees), then axis (ax, ay, az).
     */
    void getOrientation(float *angleAxis) const {
        if (component_transform) {
            angleAxis[0] = component_transform->orientation_angle;
            const auto &a = component_transform->orientation_axis;
            angleAxis[1] = a.size() > 0 ? a[0] : 0.f;
            angleAxis[2] = a.size() > 1 ? a[1] : 0.f;
            angleAxis[3] = a.size() > 2 ? a[2] : 1.f;
        } else {
            angleAxis[0] = 0.f;
            angleAxis[1] = 0.f;
            angleAxis[2] = 0.f;
            angleAxis[3] = 1.f;
        }
    }
    /**
     * @brief Sets the orientation component of this Transformable.
     *
     * Looking along the rotation axis, rotation is @p angle degrees clockwise.
     * The axis need not be a unit vector.
     *
     * @param angle Angle of rotation about the axis, in degrees.
     * @param ax X component of the rotation axis.
     * @param ay Y component of the rotation axis.
     * @param az Z component of the rotation axis.
     * @see getOrientation, preRotate, postRotate
     */
    void setOrientation(float angle, float ax, float ay, float az) {
        ensure_component();
        component_transform->orientation_angle = angle;
        component_transform->orientation_axis = {ax, ay, az};
    }
    /**
     * @brief Multiplies the current orientation from the right by the given orientation.
     *
     * Denoting the given orientation by R' and the current by R: `R'' = R R'`.
     * Equivalent to @ref preRotate except for multiplication order.
     *
     * @param angle Angle of rotation about the axis, in degrees.
     * @param ax X component of the rotation axis.
     * @param ay Y component of the rotation axis.
     * @param az Z component of the rotation axis.
     */
    void postRotate(float angle, float ax, float ay, float az) {
        (void)ax;
        (void)ay;
        (void)az;
        ensure_component();
        /* Decode toolkit: accumulate angle on current axis if parallel; else replace. */
        component_transform->orientation_angle += angle;
        component_transform->orientation_axis = {ax, ay, az};
    }
    /**
     * @brief Multiplies the current orientation from the left by the given orientation.
     *
     * Denoting the given orientation by R' and the current by R: `R'' = R' R`.
     *
     * @param angle Angle of rotation about the axis, in degrees.
     * @param ax X component of the rotation axis.
     * @param ay Y component of the rotation axis.
     * @param az Z component of the rotation axis.
     * @see setOrientation, postRotate
     */
    void preRotate(float angle, float ax, float ay, float az) {
        postRotate(angle, ax, ay, az);
    }

    /**
     * @brief Retrieves the matrix component of this Transformable (not the full composite).
     * @param[out] m16_row_major Length ≥ 16; row-major 4×4 (identity if unset).
     * @see getCompositeTransform, setTransform
     */
    void getTransform(float *m16_row_major) const {
        if (general_transform && general_transform->size() >= 16) {
            for (int i = 0; i < 16; ++i) m16_row_major[i] = (*general_transform)[static_cast<std::size_t>(i)];
        } else {
            for (int i = 0; i < 16; ++i) m16_row_major[i] = (i % 5 == 0) ? 1.f : 0.f;
        }
    }
    /**
     * @brief Retrieves the matrix component into a @ref Transform.
     * @param[out] transform Non-null destination.
     */
    void getTransform(Transform *transform) const;
    /**
     * @brief Sets the matrix component of this Transformable by copying the given 4×4.
     * @param m16_row_major Row-major matrix; bottom row should be (0,0,0,1).
     */
    void setTransform(const float *m16_row_major) {
        general_transform = std::vector<float>(m16_row_major, m16_row_major + 16);
    }
    /**
     * @brief Sets the matrix component by copying the given Transform.
     * @param transform Source matrix; must not be null.
     */
    void setTransform(const Transform *transform);

    /**
     * @brief Retrieves the composite transformation matrix of this Transformable.
     *
     * Returns `T R S M` as a single 4×4 (row-major storage in this toolkit).
     *
     * @param[out] m16_row_major Length ≥ 16.
     * @see getTransform, getTranslation, getOrientation, getScale
     */
    void getCompositeTransform(float *m16_row_major) const {
        /* Start from matrix component. */
        getTransform(m16_row_major);
        float sx = 1.f, sy = 1.f, sz = 1.f;
        float tx = 0.f, ty = 0.f, tz = 0.f;
        float ang = 0.f, ax = 0.f, ay = 0.f, az = 1.f;
        if (component_transform) {
            const auto &s = component_transform->scale;
            const auto &t = component_transform->translation;
            sx = s.size() > 0 ? s[0] : 1.f;
            sy = s.size() > 1 ? s[1] : 1.f;
            sz = s.size() > 2 ? s[2] : 1.f;
            tx = t.size() > 0 ? t[0] : 0.f;
            ty = t.size() > 1 ? t[1] : 0.f;
            tz = t.size() > 2 ? t[2] : 0.f;
            ang = component_transform->orientation_angle;
            const auto &a = component_transform->orientation_axis;
            ax = a.size() > 0 ? a[0] : 0.f;
            ay = a.size() > 1 ? a[1] : 0.f;
            az = a.size() > 2 ? a[2] : 1.f;
        }
        /* Apply scale into upper 3×3 columns (column scale for row-major M*S). */
        for (int r = 0; r < 3; ++r) {
            m16_row_major[r * 4 + 0] *= sx;
            m16_row_major[r * 4 + 1] *= sy;
            m16_row_major[r * 4 + 2] *= sz;
        }
        /* Apply rotation if non-zero angle (Rodrigues into upper 3×3). */
        if (std::fabs(ang) > 1e-8f) {
            const float rad = ang * (3.14159265358979323846f / 180.f);
            float len = std::sqrt(ax * ax + ay * ay + az * az);
            if (len > 1e-8f) {
                ax /= len;
                ay /= len;
                az /= len;
                const float c = std::cos(rad), s = std::sin(rad), C = 1.f - c;
                float R[9] = {
                    ax * ax * C + c, ax * ay * C - az * s, ax * az * C + ay * s,
                    ay * ax * C + az * s, ay * ay * C + c, ay * az * C - ax * s,
                    az * ax * C - ay * s, az * ay * C + ax * s, az * az * C + c,
                };
                float M3[9];
                for (int r = 0; r < 3; ++r)
                    for (int c = 0; c < 3; ++c)
                        M3[r * 3 + c] = m16_row_major[r * 4 + c];
                for (int r = 0; r < 3; ++r) {
                    for (int c = 0; c < 3; ++c) {
                        m16_row_major[r * 4 + c] =
                            R[r * 3 + 0] * M3[0 * 3 + c] + R[r * 3 + 1] * M3[1 * 3 + c] +
                            R[r * 3 + 2] * M3[2 * 3 + c];
                    }
                }
            }
        }
        m16_row_major[12] += tx;
        m16_row_major[13] += ty;
        m16_row_major[14] += tz;
    }
    /** @brief Retrieves the composite transform into a @ref Transform. */
    void getCompositeTransform(Transform *transform) const;

    std::shared_ptr<Object3D> duplicate() const override {
        return std::make_shared<Transformable>(*this);
    }
};

/**
 * @brief Alignment target axes and reference node ids (`Node.setAlignment`).
 * @ingroup m3g_model
 *
 * Java selects Z and Y alignment independently; each may be @ref Node::NONE,
 * @ref Node::ORIGIN, or an axis of a reference node.
 */
struct Alignment {
    int z_target = 0; /**< @brief Z-axis alignment target (@ref Node::NONE / ORIGIN / *_AXIS). */
    int y_target = 0; /**< @brief Y-axis alignment target. */
    std::optional<int> z_reference_id; /**< @brief Z reference Node object id, or absent if null. */
    std::optional<int> y_reference_id; /**< @brief Y reference Node object id, or absent if null. */
};

/**
 * @brief Abstract base class for all scene graph nodes.
 * @ingroup m3g_model
 *
 * Java: `javax.microedition.m3g.Node` extends Transformable.
 *
 * There are five kinds of nodes: Camera, Mesh, Sprite3D, Light, and Group.
 *
 * @par Node transformation
 * Each node defines a local coordinate system relative to its parent. The node
 * transformation is `p' = T R S M p` (see @ref Transformable). T, R, and S are
 * independently animatable; M is not animatable and is set only via setTransform.
 *
 * @par Node alignment
 * A node may be aligned to reference node(s). Calling @ref align overwrites the
 * orientation component R with an aligned orientation A. Rendering does not
 * resolve alignments automatically — the application must call align explicitly
 * (typically once per frame).
 *
 * @par Inherited properties
 * Effective alpha factor is the product of this node and its ancestors (range [0,1]).
 * Effective rendering/picking enable is the logical AND along the ancestor chain.
 * Scope is **not** inherited.
 *
 * @par Scoping
 * Scope is an integer bitmask. Nodes A and B are in the same scope if
 * `(scopeA & scopeB) != 0`. Used for visibility culling, lighting, and picking.
 * Default scope is -1 (all bits set): all nodes share one scope.
 *
 * @par Instantiation defaults
 * - parent: null
 * - rendering enable: true
 * - picking enable: true
 * - alpha factor: 1.0
 * - scope: -1
 * - alignment: (NONE, null) for all axes
 *
 * @see Group, Mesh, Camera, Light, Sprite3D
 */
struct Node : Transformable {
    /** @brief No alignment for the specified axis (`setAlignment`). Constant field value 144. */
    static constexpr int NONE = 144;
    /** @brief Origin of the reference node as orientation reference. Constant 145. */
    static constexpr int ORIGIN = 145;
    /** @brief X axis of the reference node as orientation reference. Constant 146. */
    static constexpr int X_AXIS = 146;
    /** @brief Y axis of the reference node as orientation reference. Constant 147. */
    static constexpr int Y_AXIS = 147;
    /** @brief Z axis of the reference node as orientation reference. Constant 148. */
    static constexpr int Z_AXIS = 148;

    bool enable_rendering = true; /**< @brief Local rendering enable flag. */
    bool enable_picking = true;   /**< @brief Local picking enable flag. */
    int alpha_factor = 255;       /**< @brief Local alpha factor as file byte 0..255 (API float = /255). */
    std::uint32_t scope = 0xffffffffu; /**< @brief Scope bitmask; default -1 (all bits). */
    std::optional<Alignment> alignment; /**< @brief Alignment settings, or absent if disabled. */
    /** @brief Parent node object id in @ref File (0 = none). Not a file field on Node. */
    int parent_id = 0;

    /**
     * @brief Retrieves the rendering enable flag of this Node.
     * @return true if rendering is enabled at this node (ancestors still apply).
     */
    bool isRenderingEnabled() const { return enable_rendering; }
    /**
     * @brief Sets the rendering enable flag of this Node.
     * @param enable true to allow rendering when ancestors also enable it.
     */
    void setRenderingEnable(bool enable) { enable_rendering = enable; }
    /**
     * @brief Retrieves the picking enable flag of this Node.
     * @return true if picking is enabled at this node.
     */
    bool isPickingEnabled() const { return enable_picking; }
    /**
     * @brief Sets the picking enable flag of this Node.
     * @param enable true to allow picking when ancestors also enable it.
     */
    void setPickingEnable(bool enable) { enable_picking = enable; }
    /**
     * @brief Retrieves the alpha factor of this Node.
     * @return Alpha factor in [0, 1].
     */
    float getAlphaFactor() const { return alpha_factor / 255.f; }
    /**
     * @brief Sets the alpha factor for this Node.
     * @param alphaFactor Value clamped to [0, 1].
     */
    void setAlphaFactor(float alphaFactor) {
        if (alphaFactor < 0.f) alphaFactor = 0.f;
        if (alphaFactor > 1.f) alphaFactor = 1.f;
        alpha_factor = static_cast<int>(alphaFactor * 255.f + 0.5f);
    }
    /**
     * @brief Retrieves the scope of this Node.
     * @return Scope bitmask (default -1).
     */
    int getScope() const { return static_cast<int>(scope); }
    /**
     * @brief Sets the scope of this node.
     * @param scope_ Scope bitmask.
     */
    void setScope(int scope_) { scope = static_cast<std::uint32_t>(scope_); }

    /**
     * @brief Returns the scene graph parent of this node.
     * @return Parent object id, or 0 if detached (Java returns Node or null).
     */
    int getParent() const { return parent_id; }

    /**
     * @brief Returns the alignment target for the given axis.
     * @param axis @ref Y_AXIS or @ref Z_AXIS.
     * @return @ref NONE, @ref ORIGIN, or an axis constant.
     */
    int getAlignmentTarget(int axis) const {
        if (!alignment) return NONE;
        if (axis == Z_AXIS) return alignment->z_target;
        if (axis == Y_AXIS) return alignment->y_target;
        return NONE;
    }
    /**
     * @brief Returns the alignment reference node for the given axis.
     * @param axis @ref Y_AXIS or @ref Z_AXIS.
     * @return Reference node object id, or 0 if none (Java returns Node or null).
     */
    int getAlignmentReference(int axis) const {
        if (!alignment) return 0;
        if (axis == Z_AXIS) return alignment->z_reference_id.value_or(0);
        if (axis == Y_AXIS) return alignment->y_reference_id.value_or(0);
        return 0;
    }
    /**
     * @brief Sets this node to align with the given other node(s), or disables alignment.
     *
     * Does not compute the new orientation; call @ref align to apply. Passing
     * @ref NONE for both targets clears alignment.
     *
     * @param zRefId Z reference node object id, or 0 for null.
     * @param zTarget Z alignment target (@ref NONE / ORIGIN / *_AXIS).
     * @param yRefId Y reference node object id, or 0 for null.
     * @param yTarget Y alignment target.
     */
    void setAlignment(int zRefId, int zTarget, int yRefId, int yTarget) {
        if (zTarget == NONE && yTarget == NONE) {
            alignment.reset();
            return;
        }
        Alignment a;
        a.z_target = zTarget;
        a.y_target = yTarget;
        if (zRefId != 0) a.z_reference_id = zRefId;
        if (yRefId != 0) a.y_reference_id = yRefId;
        alignment = a;
    }

    /**
     * @brief Applies alignments to this Node and its descendants.
     *
     * @param reference Optional default reference node when a target was set with a null ref.
     *
     * @note Runtime scene-graph walk is not implemented in the decode toolkit (no-op).
     */
    void align(Node * /*reference*/) {}

    /**
     * @brief Gets the composite transformation from this node to the given node.
     *
     * @param target Destination node in the same scene graph.
     * @param[out] m16_row_major Receives the 4×4 transform on success.
     * @return true if the transform was computed; false if nodes are not in the same tree.
     *
     * @note Without resolved parent links against a @ref File, only @p target == this succeeds.
     */
    bool getTransformTo(const Node *target, float *m16_row_major) const {
        if (!target || target != this) return false;
        getCompositeTransform(m16_row_major);
        return true;
    }
    /** @brief @ref getTransformTo overload writing a @ref Transform. */
    bool getTransformTo(const Node *target, Transform *transform) const;

    std::shared_ptr<Object3D> duplicate() const override {
        return std::make_shared<Node>(*this);
    }
};

/**
 * @brief A scene graph node that stores an unordered set of nodes as its children.
 * @ingroup m3g_model
 *
 * Java: `javax.microedition.m3g.Group` extends Node.
 *
 * A Group is the only node type that can have children (except SkinnedMesh, which
 * has a separate skeleton Group). World is a specialized Group that serves as the
 * scene root. Child order is not significant for rendering; @ref getChild indices
 * may change when children are added or removed.
 *
 * @par Instantiation
 * Constructs a Group with an empty list of children (and Node defaults).
 *
 * @see World, Node, Mesh, RayIntersection
 */
struct Group : Node {
    Group() { object_type = ObjectTypes::GROUP; }
    std::vector<int> child_ids; /**< @brief Child Node object ids. */

    /**
     * @brief Gets the number of children in this Group.
     * @return Child count.
     */
    int getChildCount() const { return static_cast<int>(child_ids.size()); }
    /**
     * @brief Gets a child by index.
     * @param index Zero-based index; valid range is [0, getChildCount()).
     * @return Child object id (Java returns Node).
     * @throws std::out_of_range if index is invalid.
     */
    int getChild(int index) const { return child_ids.at(static_cast<std::size_t>(index)); }
    /**
     * @brief Adds the given node to this Group.
     *
     * Potentially changes the order and indices of previously added children.
     *
     * @param childId Non-zero object id of the child Node.
     * @throws std::invalid_argument if @p childId is 0.
     */
    void addChild(int childId) {
        if (childId == 0) throw std::invalid_argument("Group::addChild: null child");
        child_ids.push_back(childId);
    }
    /**
     * @brief Removes the given node from this Group.
     *
     * Potentially changes the order and indices of the remaining children.
     *
     * @param childId Object id of the child to remove.
     */
    void removeChild(int childId) {
        auto &v = child_ids;
        v.erase(std::remove(v.begin(), v.end(), childId), v.end());
    }

    /**
     * @brief Picks the first Mesh or scaled Sprite3D intercepted by a screen-space ray.
     *
     * Java: `pick(scope, x, y, camera, ri)` — ray through viewport (x,y) in [0,1]
     * through the given Camera. Only objects in @p scope with picking enabled are tested.
     *
     * @param scope Scope bitmask for the pick ray.
     * @param x Viewport X in [0,1].
     * @param y Viewport Y in [0,1].
     * @param camera Camera defining the projection.
     * @param ri Optional @ref RayIntersection to fill; may be null.
     * @return true if an intersection was found.
     *
     * @note Not implemented without scene/render state; always returns false.
     */
    bool pick(int /*scope*/, float /*x*/, float /*y*/, const Camera * /*camera*/,
              void * /*ri*/) const {
        return false;
    }
    /**
     * @brief Picks the first Mesh intercepted by a world-space ray.
     *
     * Java: `pick(scope, ox,oy,oz, dx,dy,dz, ri)` — ray origin + direction.
     *
     * @return true if an intersection was found.
     * @note Not implemented without mesh walk; always returns false.
     */
    bool pick(int /*scope*/, float /*ox*/, float /*oy*/, float /*oz*/, float /*dx*/, float /*dy*/,
              float /*dz*/, void * /*ri*/) const {
        return false;
    }

    /** @brief Direct references are the child node ids. */
    int getReferences(int *references, int max_count) const override {
        const int n = static_cast<int>(child_ids.size());
        if (references && max_count > 0) {
            const int copy = n < max_count ? n : max_count;
            for (int i = 0; i < copy; ++i) references[i] = child_ids[static_cast<std::size_t>(i)];
        }
        return n;
    }

    std::shared_ptr<Object3D> duplicate() const override {
        return std::make_shared<Group>(*this);
    }
};

/**
 * @brief A special Group that is a complete scene — the root of the scene graph.
 * @ingroup m3g_model
 *
 * Java: `javax.microedition.m3g.World` extends Group.
 *
 * A World has an active Camera (required for retained-mode rendering) and an
 * optional Background. World is never the child of another node.
 *
 * @see Camera, Background, Group, Graphics3D
 */
struct World : Group {
    World() { object_type = ObjectTypes::WORLD; }
    std::optional<int> active_camera_id; /**< @brief Active Camera object id. */
    std::optional<int> background_id;    /**< @brief Background object id, or absent if null. */

    /**
     * @brief Gets the currently active Camera for this World.
     * @return Camera object id, or 0 if unset (Java returns Camera or null).
     */
    int getActiveCamera() const { return active_camera_id.value_or(0); }
    /**
     * @brief Gets the Background of this World.
     * @return Background object id, or 0 if none.
     */
    int getBackground() const { return background_id.value_or(0); }
};

/**
 * @brief M3G file header object (format, not a public JSR-184 scene class).
 * @ingroup m3g_model
 */
struct Header : Object {
    Header() { object_type = ObjectTypes::HEADER; }
    int version_major = 0;
    int version_minor = 0;
    bool has_external_references = false;
    std::uint32_t total_file_size = 0;
    std::uint32_t approximate_content_size = 0;
    std::string authoring_field;
};

/**
 * @brief External reference URI object.
 * @ingroup m3g_model
 */
struct ExternalReference : Object {
    ExternalReference() { object_type = ObjectTypes::EXTERNAL_REFERENCE; }
    std::string uri;
};

/**
 * @brief Perspective projection parameters (`Camera.setPerspective`).
 * @ingroup m3g_model
 */
struct PerspectiveProjection {
    float field_of_view_degrees = 45.f;
    float aspect_ratio = 1.f;
    float near_distance = 0.1f;
    float far_distance = 100.f;
};

/**
 * @brief Generic projection matrix/type payload (`Camera.setGeneric`).
 * @ingroup m3g_model
 */
struct GenericProjection {
    int projection_type = 0;
    std::vector<float> values;
};

/**
 * @brief Scene graph node defining the viewer position and 3D→2D projection.
 * @ingroup m3g_model
 *
 * Java: `javax.microedition.m3g.Camera` extends Node.
 *
 * The camera faces the negative Z axis (0,0,-1) in its local space. Position and
 * orientation use the usual node transform. The projection matrix maps camera
 * space to clip space; clipping and NDC follow OpenGL-like rules (see the Java
 * class description).
 *
 * @par Projection types
 * - @ref GENERIC — arbitrary 4×4 matrix
 * - @ref PARALLEL — orthographic (parallel) projection from fovy/aspect/near/far
 * - @ref PERSPECTIVE — perspective projection from fovy/aspect/near/far
 *
 * @par Instantiation
 * Constructs a Camera with default Node values and a default projection
 * (implementation-defined; often perspective).
 *
 * @see Node, Graphics3D, World
 */
struct Camera : Node {
    Camera() { object_type = ObjectTypes::CAMERA; }
    /** @brief Specifies a generic 4×4 projection matrix. Constant field value 48. */
    static constexpr int GENERIC = 48;
    /** @brief Specifies a parallel (orthographic) projection matrix. Constant 49. */
    static constexpr int PARALLEL = 49;
    /** @brief Specifies a perspective projection matrix. Constant 50. */
    static constexpr int PERSPECTIVE = 50;

    int projection_type = 0; /**< @brief @ref GENERIC, @ref PARALLEL, or @ref PERSPECTIVE. */
    std::optional<PerspectiveProjection> perspective; /**< @brief Params for PARALLEL/PERSPECTIVE. */
    std::optional<GenericProjection> generic;         /**< @brief Matrix payload for GENERIC. */

    /** @brief Convenience: current projection type constant. */
    int getProjectionType() const { return projection_type; }

    /**
     * @brief Gets the current projection parameters and type.
     *
     * For @ref PERSPECTIVE and @ref PARALLEL, writes four floats:
     * fovy (degrees or height), aspectRatio, near, far.
     *
     * @param[out] params Length ≥ 4, or null to query type only.
     * @return Projection type (@ref GENERIC / PARALLEL / PERSPECTIVE).
     */
    int getProjection(float *params) const {
        if (projection_type == PERSPECTIVE && perspective) {
            if (params) {
                params[0] = perspective->field_of_view_degrees;
                params[1] = perspective->aspect_ratio;
                params[2] = perspective->near_distance;
                params[3] = perspective->far_distance;
            }
            return PERSPECTIVE;
        }
        if (projection_type == PARALLEL && perspective) {
            /* Parallel stores fovy-as-height in same struct fields. */
            if (params) {
                params[0] = perspective->field_of_view_degrees;
                params[1] = perspective->aspect_ratio;
                params[2] = perspective->near_distance;
                params[3] = perspective->far_distance;
            }
            return PARALLEL;
        }
        return GENERIC;
    }

    /**
     * @brief Gets the current projection matrix and type.
     * @param[out] transform Receives the 4×4 projection (GENERIC matrix, or identity stub).
     * @return Projection type (@ref GENERIC / PARALLEL / PERSPECTIVE).
     */
    int getProjection(Transform *transform) const;

    /**
     * @brief Sets the given 4×4 transformation as the current projection matrix.
     * @param m16_row_major Row-major projection matrix.
     */
    void setGeneric(const float *m16_row_major) {
        projection_type = GENERIC;
        perspective.reset();
        GenericProjection g;
        g.projection_type = GENERIC;
        g.values.assign(m16_row_major, m16_row_major + 16);
        generic = std::move(g);
    }
    /** @brief Sets the projection from a @ref Transform (GENERIC). */
    void setGeneric(const Transform *transform);

    /**
     * @brief Constructs a parallel projection matrix and sets it as current.
     * @param fovy Height of the view volume in world units at the near plane (Java fovy).
     * @param aspectRatio Width / height of the viewport.
     * @param near Distance to the near clipping plane (must be > 0).
     * @param far Distance to the far clipping plane (must be > near).
     * @throws std::invalid_argument if parameters are out of range.
     */
    void setParallel(float fovy, float aspectRatio, float near, float far) {
        if (fovy <= 0.f || aspectRatio <= 0.f || near <= 0.f || far <= 0.f || near >= far) {
            throw std::invalid_argument("Camera::setParallel: invalid parameters");
        }
        projection_type = PARALLEL;
        generic.reset();
        PerspectiveProjection p;
        p.field_of_view_degrees = fovy;
        p.aspect_ratio = aspectRatio;
        p.near_distance = near;
        p.far_distance = far;
        perspective = p;
    }

    /**
     * @brief Constructs a perspective projection matrix and sets it as current.
     * @param fovy Vertical field of view, in degrees (0, 180).
     * @param aspectRatio Width / height of the viewport.
     * @param near Distance to the near clipping plane (must be > 0).
     * @param far Distance to the far clipping plane (must be > near).
     * @throws std::invalid_argument if parameters are out of range.
     */
    void setPerspective(float fovy, float aspectRatio, float near, float far) {
        if (fovy <= 0.f || fovy >= 180.f || aspectRatio <= 0.f || near <= 0.f || far <= 0.f ||
            near >= far) {
            throw std::invalid_argument("Camera::setPerspective: invalid parameters");
        }
        projection_type = PERSPECTIVE;
        generic.reset();
        PerspectiveProjection p;
        p.field_of_view_degrees = fovy;
        p.aspect_ratio = aspectRatio;
        p.near_distance = near;
        p.far_distance = far;
        perspective = p;
    }

    std::shared_ptr<Object3D> duplicate() const override {
        return std::make_shared<Camera>(*this);
    }
};

/**
 * @brief Scene graph node representing ambient, directional, omni, or spot lights.
 * @ingroup m3g_model
 *
 * Java: `javax.microedition.m3g.Light` extends Node.
 *
 * Lights determine object color together with @ref Material. Direction of directional
 * and spot lights is the negative Z axis of the Light node's local space. Scope selects
 * which Meshes a light affects; @ref Node::setRenderingEnable turns lights on/off.
 * Picking ignores lights.
 *
 * @par Light source types
 * - @ref AMBIENT — illuminates from all directions; position/direction ignored
 * - @ref DIRECTIONAL — constant direction (sun); position ignored
 * - @ref OMNI — point light from node origin; orientation ignored
 * - @ref SPOT — cone about −Z; spot angle/exponent apply
 *
 * RGB contribution is intensity × color; intensity may exceed 1.0 for highlights.
 *
 * @see Material, Node, Mesh
 */
struct Light : Node {
    Light() { object_type = ObjectTypes::LIGHT; }
    /** @brief Ambient light source (`setMode`). Constant field value 128. */
    static constexpr int AMBIENT = 128;
    /** @brief Directional light source. Constant 129. */
    static constexpr int DIRECTIONAL = 129;
    /** @brief Omnidirectional (point) light source. Constant 130. */
    static constexpr int OMNI = 130;
    /** @brief Spot light source. Constant 131. */
    static constexpr int SPOT = 131;

    float attenuation_constant = 1.f;  /**< @brief Constant attenuation coefficient. */
    float attenuation_linear = 0.f;    /**< @brief Linear attenuation coefficient. */
    float attenuation_quadratic = 0.f; /**< @brief Quadratic attenuation coefficient. */
    RgbColor color; /**< @brief Light RGB color (file default white). */
    int mode = DIRECTIONAL; /**< @brief @ref AMBIENT / DIRECTIONAL / OMNI / SPOT. */
    float intensity = 1.f;  /**< @brief Scalar intensity multiplier (may be > 1). */
    float spot_angle = 45.f; /**< @brief Spot cone half-angle in degrees [0, 90]. */
    float spot_exponent = 0.f; /**< @brief Spot concentration exponent [0, 128]. */

    /** @brief Retrieves the current type of this Light. */
    int getMode() const { return mode; }
    /**
     * @brief Sets the type of this Light.
     * @param mode_ @ref AMBIENT, @ref DIRECTIONAL, @ref OMNI, or @ref SPOT.
     * @throws std::invalid_argument if @p mode_ is invalid.
     */
    void setMode(int mode_) {
        if (mode_ != AMBIENT && mode_ != DIRECTIONAL && mode_ != OMNI && mode_ != SPOT) {
            throw std::invalid_argument("Light::setMode: invalid mode");
        }
        mode = mode_;
    }
    /** @brief Retrieves the current intensity of this Light. */
    float getIntensity() const { return intensity; }
    /**
     * @brief Sets the intensity of this Light.
     * @param intensity_ Non-negative intensity (values > 1 allowed in spirit of the Java API).
     * @throws std::invalid_argument if negative.
     */
    void setIntensity(float intensity_) {
        if (intensity_ < 0.f) throw std::invalid_argument("Light::setIntensity: negative");
        intensity = intensity_;
    }
    /**
     * @brief Retrieves the current color of this Light as 0x00RRGGBB.
     * @return Packed RGB (alpha unused).
     */
    int getColor() const {
        return ((color.red & 0xff) << 16) | ((color.green & 0xff) << 8) | (color.blue & 0xff);
    }
    /**
     * @brief Sets the color of this Light.
     * @param RGB Packed 0x00RRGGBB.
     */
    void setColor(int RGB) {
        color.red = (RGB >> 16) & 0xff;
        color.green = (RGB >> 8) & 0xff;
        color.blue = RGB & 0xff;
    }
    /** @brief Retrieves the constant attenuation coefficient. */
    float getConstantAttenuation() const { return attenuation_constant; }
    /** @brief Retrieves the linear attenuation coefficient. */
    float getLinearAttenuation() const { return attenuation_linear; }
    /** @brief Retrieves the quadratic attenuation coefficient. */
    float getQuadraticAttenuation() const { return attenuation_quadratic; }
    /**
     * @brief Sets the attenuation coefficients for this Light.
     *
     * Attenuation factor is `1 / (c + l·d + q·d²)` for distance d (omni/spot).
     *
     * @param constant Constant term (≥ 0).
     * @param linear Linear term (≥ 0).
     * @param quadratic Quadratic term (≥ 0).
     */
    void setAttenuation(float constant, float linear, float quadratic) {
        if (constant < 0.f || linear < 0.f || quadratic < 0.f) {
            throw std::invalid_argument("Light::setAttenuation: negative coefficient");
        }
        attenuation_constant = constant;
        attenuation_linear = linear;
        attenuation_quadratic = quadratic;
    }
    /** @brief Retrieves the current spot angle of this Light (degrees). */
    float getSpotAngle() const { return spot_angle; }
    /**
     * @brief Sets the spot cone angle for this Light.
     * @param angle Half-angle of the spot cone in degrees, in [0, 90].
     */
    void setSpotAngle(float angle) {
        if (angle < 0.f || angle > 90.f) {
            throw std::invalid_argument("Light::setSpotAngle: out of range");
        }
        spot_angle = angle;
    }
    /** @brief Retrieves the current spot exponent for this Light. */
    float getSpotExponent() const { return spot_exponent; }
    /**
     * @brief Sets the spot exponent for this Light.
     * @param exponent Concentration in [0, 128]; higher = tighter beam.
     */
    void setSpotExponent(float exponent) {
        if (exponent < 0.f || exponent > 128.f) {
            throw std::invalid_argument("Light::setSpotExponent: out of range");
        }
        spot_exponent = exponent;
    }

    std::shared_ptr<Object3D> duplicate() const override {
        return std::make_shared<Light>(*this);
    }
};

/**
 * @brief JSR-184 `Background`.
 * @ingroup m3g_model
 */
struct Background : Object3D {
    Background() { object_type = ObjectTypes::BACKGROUND; }
    static constexpr int BORDER = 32;
    static constexpr int REPEAT = 33;

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

/**
 * @brief JSR-184 `Fog`.
 * @ingroup m3g_model
 */
struct Fog : Object3D {
    Fog() { object_type = ObjectTypes::FOG; }
    static constexpr int EXPONENTIAL = 80;
    static constexpr int LINEAR = 81;

    RgbColor color;
    int mode = 0;
    float density = 0.f;
    float near_distance = 0.f;
    float far_distance = 0.f;
};

/**
 * @brief JSR-184 `PolygonMode`.
 * @ingroup m3g_model
 */
struct PolygonMode : Object3D {
    PolygonMode() { object_type = ObjectTypes::POLYGON_MODE; }
    static constexpr int CULL_BACK = 160;
    static constexpr int CULL_FRONT = 161;
    static constexpr int CULL_NONE = 162;
    static constexpr int SHADE_FLAT = 164;
    static constexpr int SHADE_SMOOTH = 165;
    static constexpr int WINDING_CCW = 168;
    static constexpr int WINDING_CW = 169;

    int culling = 0;
    int shading = 0;
    int winding = 0;
    bool two_sided_lighting_enabled = false;
    bool local_camera_lighting_enabled = false;
    bool perspective_correction_enabled = false;
};

/**
 * @brief JSR-184 `Material`.
 * @ingroup m3g_model
 */
struct Material : Object3D {
    Material() { object_type = ObjectTypes::MATERIAL; }
    static constexpr int AMBIENT = 1024;
    static constexpr int DIFFUSE = 2048;
    static constexpr int EMISSIVE = 4096;
    static constexpr int SPECULAR = 8192;

    RgbColor ambient_color;
    RgbaColor diffuse_color;
    RgbColor emissive_color;
    RgbColor specular_color;
    float shininess = 0.f;
    bool vertex_color_tracking_enabled = false;

    float getShininess() const { return shininess; }
    void setShininess(float s) { shininess = s; }
};

/**
 * @brief JSR-184 `VertexArray`.
 * @ingroup m3g_model
 */
struct VertexArray : Object3D {
    VertexArray() { object_type = ObjectTypes::VERTEX_ARRAY; }
    int component_size = 0;
    int component_count = 0;
    int encoding = 0;
    int vertex_count = 0;
    std::vector<int> components;

    int getComponentCount() const { return component_count; }
    int getComponentType() const { return component_size; }
    int getVertexCount() const { return vertex_count; }
};

/**
 * @brief Texture coordinate stream binding.
 * @ingroup m3g_model
 */
struct TexCoordBinding {
    std::optional<int> vertex_array_id;
    std::vector<float> bias{0, 0, 0};
    float scale = 1.f;
};

/**
 * @brief JSR-184 `VertexBuffer`.
 * @ingroup m3g_model
 */
struct VertexBuffer : Object3D {
    VertexBuffer() { object_type = ObjectTypes::VERTEX_BUFFER; }
    RgbaColor default_color;
    std::optional<int> positions_id;
    std::vector<float> position_bias{0, 0, 0};
    float position_scale = 1.f;
    std::optional<int> normals_id;
    std::optional<int> colors_id;
    std::vector<TexCoordBinding> tex_coord_bindings;
};

/**
 * @brief JSR-184 `IndexBuffer` — abstract connectivity for mesh geometry.
 * @ingroup m3g_model
 *
 * Only @ref TriangleStripArray is defined by the file format / API in JSR-184.
 */
struct IndexBuffer : Object3D {
    virtual ~IndexBuffer() = default;
};

/**
 * @brief JSR-184 `TriangleStripArray` — triangle-strip @ref IndexBuffer.
 * @ingroup m3g_model
 */
struct TriangleStripArray : IndexBuffer {
    TriangleStripArray() { object_type = ObjectTypes::TRIANGLE_STRIP_ARRAY; }
    int encoding = 0;
    std::vector<int> indices;
    std::vector<int> strip_lengths;
};

/**
 * @brief JSR-184 `CompositingMode` — per-pixel blending / depth attributes.
 * @ingroup m3g_model
 */
struct CompositingMode : Object3D {
    CompositingMode() { object_type = ObjectTypes::COMPOSITING_MODE; }
    static constexpr int ALPHA = 64;
    static constexpr int ALPHA_ADD = 65;
    static constexpr int MODULATE = 66;
    static constexpr int MODULATE_X2 = 67;
    static constexpr int REPLACE = 68;

    bool depth_test_enabled = true;
    bool depth_write_enabled = true;
    bool color_write_enabled = true;
    bool alpha_write_enabled = true;
    int blending = REPLACE;
    int alpha_threshold = 0; /**< @brief File byte 0..255; API float is /255. */
    float depth_offset_factor = 0.f;
    float depth_offset_units = 0.f;

    int getBlending() const { return blending; }
    void setBlending(int mode) { blending = mode; }
    float getAlphaThreshold() const { return alpha_threshold / 255.f; }
    void setAlphaThreshold(float t) {
        if (t < 0.f) t = 0.f;
        if (t > 1.f) t = 1.f;
        alpha_threshold = static_cast<int>(t * 255.f + 0.5f);
    }
    float getDepthOffsetFactor() const { return depth_offset_factor; }
    float getDepthOffsetUnits() const { return depth_offset_units; }
    void setDepthOffset(float factor, float units) {
        depth_offset_factor = factor;
        depth_offset_units = units;
    }
    bool isDepthTestEnabled() const { return depth_test_enabled; }
    void setDepthTestEnable(bool e) { depth_test_enabled = e; }
    bool isDepthWriteEnabled() const { return depth_write_enabled; }
    void setDepthWriteEnable(bool e) { depth_write_enabled = e; }
    bool isColorWriteEnabled() const { return color_write_enabled; }
    void setColorWriteEnable(bool e) { color_write_enabled = e; }
    bool isAlphaWriteEnabled() const { return alpha_write_enabled; }
    void setAlphaWriteEnable(bool e) { alpha_write_enabled = e; }
};

/**
 * @brief JSR-184 `Appearance` — rendering attributes for Mesh / Sprite3D.
 * @ingroup m3g_model
 */
struct Appearance : Object3D {
    Appearance() { object_type = ObjectTypes::APPEARANCE; }
    int layer = 0;
    std::optional<int> compositing_mode_id;
    std::optional<int> fog_id;
    std::optional<int> polygon_mode_id;
    std::optional<int> material_id;
    std::vector<int> texture_ids;

    int getLayer() const { return layer; }
    void setLayer(int l) { layer = l; }
    int getCompositingMode() const { return compositing_mode_id.value_or(0); }
    int getFog() const { return fog_id.value_or(0); }
    int getPolygonMode() const { return polygon_mode_id.value_or(0); }
    int getMaterial() const { return material_id.value_or(0); }
    int getTexture(int index) const {
        return texture_ids.at(static_cast<std::size_t>(index));
    }
};

/**
 * @brief JSR-184 `Texture2D` (extends Transformable).
 * @ingroup m3g_model
 */
struct Texture2D : Transformable {
    Texture2D() { object_type = ObjectTypes::TEXTURE_2D; }
    static constexpr int FILTER_BASE_LEVEL = 208;
    static constexpr int FILTER_LINEAR = 209;
    static constexpr int FILTER_NEAREST = 210;
    static constexpr int FUNC_ADD = 224;
    static constexpr int FUNC_BLEND = 225;
    static constexpr int FUNC_DECAL = 226;
    static constexpr int FUNC_MODULATE = 227;
    static constexpr int FUNC_REPLACE = 228;
    static constexpr int WRAP_CLAMP = 240;
    static constexpr int WRAP_REPEAT = 241;

    std::optional<int> image_id;
    RgbColor blend_color;
    int blending = 0;
    int wrapping_s = 0;
    int wrapping_t = 0;
    int level_filter = 0;
    int image_filter = 0;
};

/**
 * @brief JSR-184 `Image2D`.
 * @ingroup m3g_model
 */
struct Image2D : Object3D {
    Image2D() { object_type = ObjectTypes::IMAGE_2D; }
    static constexpr int ALPHA = 96;
    static constexpr int LUMINANCE = 97;
    static constexpr int LUMINANCE_ALPHA = 98;
    static constexpr int RGB = 99;
    static constexpr int RGBA = 100;

    int format = 0;
    bool is_mutable = false;
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> palette;
    std::optional<std::vector<std::uint8_t>> pixels;

    int getFormat() const { return format; }
    int getWidth() const { return width; }
    int getHeight() const { return height; }
    bool isMutable() const { return is_mutable; }
};

/**
 * @brief Index buffer + appearance reference for a submesh.
 * @ingroup m3g_model
 */
struct SubmeshRef {
    std::optional<int> index_buffer_id;
    std::optional<int> appearance_id;
};

/**
 * @brief Scene graph node representing a polygonal 3D object (rigid body mesh).
 * @ingroup m3g_model
 *
 * Java: `javax.microedition.m3g.Mesh` extends Node.
 *
 * A Mesh has one shared @ref VertexBuffer and one or more submeshes. Each submesh is
 * an @ref IndexBuffer (triangle strips) plus an optional @ref Appearance. Submeshes
 * render in ascending Appearance layer order; null Appearance disables the submesh.
 * MorphingMesh and SkinnedMesh extend Mesh with per-vertex animation.
 *
 * @see VertexBuffer, IndexBuffer, Appearance, MorphingMesh, SkinnedMesh
 */
struct Mesh : Node {
    Mesh() { object_type = ObjectTypes::MESH; }
    std::optional<int> vertex_buffer_id; /**< @brief Shared VertexBuffer object id. */
    std::vector<SubmeshRef> submeshes;  /**< @brief IndexBuffer + Appearance per submesh. */

    /**
     * @brief Gets the number of submeshes in this Mesh.
     * @return Submesh count.
     */
    int getSubmeshCount() const { return static_cast<int>(submeshes.size()); }
    /**
     * @brief Gets the vertex buffer of this Mesh.
     * @return VertexBuffer object id (0 if none).
     */
    int getVertexBuffer() const { return vertex_buffer_id.value_or(0); }
    /**
     * @brief Retrieves the submesh IndexBuffer at the given index.
     * @param index Submesh index in [0, getSubmeshCount()).
     * @return IndexBuffer object id.
     */
    int getIndexBuffer(int index) const {
        return submeshes.at(static_cast<std::size_t>(index)).index_buffer_id.value_or(0);
    }
    /**
     * @brief Gets the current Appearance of the specified submesh.
     * @param index Submesh index.
     * @return Appearance object id, or 0 if null.
     */
    int getAppearance(int index) const {
        return submeshes.at(static_cast<std::size_t>(index)).appearance_id.value_or(0);
    }
    /**
     * @brief Sets the Appearance for the specified submesh.
     * @param index Submesh index.
     * @param appearanceId Appearance object id, or 0 to clear (disable rendering/picking).
     */
    void setAppearance(int index, int appearanceId) {
        auto &sm = submeshes.at(static_cast<std::size_t>(index));
        if (appearanceId == 0) sm.appearance_id.reset();
        else sm.appearance_id = appearanceId;
    }

    int getReferences(int *references, int max_count) const override {
        std::vector<int> refs;
        if (vertex_buffer_id && *vertex_buffer_id != 0) refs.push_back(*vertex_buffer_id);
        for (const auto &sm : submeshes) {
            if (sm.index_buffer_id && *sm.index_buffer_id != 0) refs.push_back(*sm.index_buffer_id);
            if (sm.appearance_id && *sm.appearance_id != 0) refs.push_back(*sm.appearance_id);
        }
        const int n = static_cast<int>(refs.size());
        if (references && max_count > 0) {
            const int copy = n < max_count ? n : max_count;
            for (int i = 0; i < copy; ++i) references[i] = refs[static_cast<std::size_t>(i)];
        }
        return n;
    }

    std::shared_ptr<Object3D> duplicate() const override {
        return std::make_shared<Mesh>(*this);
    }
};

/**
 * @brief Bone influence range on a skinned mesh (`SkinnedMesh.addTransform`).
 * @ingroup m3g_model
 */
struct BoneWeight {
    std::optional<int> transform_node_id;
    int first_vertex = 0;
    int vertex_count = 0;
    int weight = 0;
};

/**
 * @brief Scene graph node for a skeletally animated polygon mesh.
 * @ingroup m3g_model
 *
 * Java: `javax.microedition.m3g.SkinnedMesh` extends Mesh.
 *
 * Vertices may be associated with multiple bone Nodes and weights. The skeleton
 * @ref Group is the only child of the SkinnedMesh (`getSkeleton().getParent() == this`
 * in Java). The skeleton branch is traversed for rendering/picking like any other.
 *
 * @see Mesh, Group, BoneWeight
 */
struct SkinnedMesh : Mesh {
    SkinnedMesh() { object_type = ObjectTypes::SKINNED_MESH; }
    std::optional<int> skeleton_id; /**< @brief Skeleton Group object id. */
    std::vector<BoneWeight> bone_transforms; /**< @brief Weighted bone influences. */

    /**
     * @brief Returns the skeleton Group of this SkinnedMesh.
     * @return Skeleton Group object id.
     */
    int getSkeleton() const { return skeleton_id.value_or(0); }

    /**
     * @brief Associates a weighted transformation ("bone") with a range of vertices.
     * @param boneId Bone Node object id (descendant of the skeleton).
     * @param weight Positive integer weight.
     * @param firstVertex First vertex index in the mesh VertexBuffer.
     * @param numVertices Number of consecutive vertices influenced.
     */
    void addTransform(int boneId, int weight, int firstVertex, int numVertices) {
        if (boneId == 0) throw std::invalid_argument("SkinnedMesh::addTransform: null bone");
        if (weight <= 0 || numVertices <= 0 || firstVertex < 0) {
            throw std::invalid_argument("SkinnedMesh::addTransform: invalid range/weight");
        }
        BoneWeight bw;
        bw.transform_node_id = boneId;
        bw.weight = weight;
        bw.first_vertex = firstVertex;
        bw.vertex_count = numVertices;
        bone_transforms.push_back(bw);
    }

    /**
     * @brief Returns the at-rest transformation for a bone node.
     * @param boneId Bone node id.
     * @param[out] m16_row_major 4×4 at-rest transform.
     * @note Decode toolkit stores influences only; returns identity.
     */
    void getBoneTransform(int /*boneId*/, float *m16_row_major) const {
        if (!m16_row_major) return;
        for (int i = 0; i < 16; ++i) m16_row_major[i] = (i % 5 == 0) ? 1.f : 0.f;
    }

    /**
     * @brief Returns vertices influenced by the given bone.
     * @param boneId Bone node id.
     * @param[out] indices Vertex indices (may be null).
     * @param[out] weights Corresponding weights (may be null).
     * @return Number of influenced vertices written / counted.
     */
    int getBoneVertices(int boneId, int *indices, float *weights) const {
        int count = 0;
        for (const auto &bw : bone_transforms) {
            if (!bw.transform_node_id || *bw.transform_node_id != boneId) continue;
            for (int i = 0; i < bw.vertex_count; ++i) {
                if (indices) indices[count] = bw.first_vertex + i;
                if (weights) weights[count] = static_cast<float>(bw.weight);
                ++count;
            }
        }
        return count;
    }

    std::shared_ptr<Object3D> duplicate() const override {
        return std::make_shared<SkinnedMesh>(*this);
    }
};

/**
 * @brief Scene graph node for a vertex-morphing polygon mesh.
 * @ingroup m3g_model
 *
 * Java: `javax.microedition.m3g.MorphingMesh` extends Mesh.
 *
 * Rendered vertices are a weighted linear combination of the base VertexBuffer and
 * morph target VertexBuffers. Targets must share array layout; the base must be a
 * superset of target attributes. Only arrays present in the targets are morphed.
 *
 * @see Mesh, VertexBuffer
 */
struct MorphingMesh : Mesh {
    MorphingMesh() { object_type = ObjectTypes::MORPHING_MESH; }
    std::vector<int> morph_target_ids; /**< @brief Morph target VertexBuffer object ids. */
    std::vector<float> morph_weights;  /**< @brief Weight per morph target. */

    /**
     * @brief Returns the number of morph targets in this MorphingMesh.
     * @return Morph target count.
     */
    int getMorphTargetCount() const { return static_cast<int>(morph_target_ids.size()); }
    /**
     * @brief Returns the morph target VertexBuffer at the given index.
     * @param index Target index in [0, getMorphTargetCount()).
     * @return VertexBuffer object id.
     */
    int getMorphTarget(int index) const {
        return morph_target_ids.at(static_cast<std::size_t>(index));
    }
    /**
     * @brief Gets the current morph target weights for this mesh.
     * @param[out] weights Length ≥ getMorphTargetCount().
     */
    void getWeights(float *weights) const {
        for (std::size_t i = 0; i < morph_weights.size(); ++i) {
            weights[i] = morph_weights[i];
        }
    }
    /**
     * @brief Sets the weights for all morph targets in this mesh.
     * @param weights Array of length getMorphTargetCount().
     */
    void setWeights(const float *weights) {
        if (!weights) throw std::invalid_argument("MorphingMesh::setWeights: null");
        morph_weights.resize(morph_target_ids.size());
        for (std::size_t i = 0; i < morph_weights.size(); ++i) {
            morph_weights[i] = weights[i];
        }
    }
    void setWeights(const std::vector<float> &weights) {
        if (weights.size() < morph_target_ids.size()) {
            throw std::invalid_argument("MorphingMesh::setWeights: too few weights");
        }
        setWeights(weights.data());
    }

    std::shared_ptr<Object3D> duplicate() const override {
        return std::make_shared<MorphingMesh>(*this);
    }
};

/**
 * @brief Scene graph node for a screen-aligned 2D image with a 3D position.
 * @ingroup m3g_model
 *
 * Java: `javax.microedition.m3g.Sprite3D` extends Node.
 *
 * A fast, restricted alternative to textured geometry. Rendered as a screen-aligned
 * pixel rectangle at constant depth (depth of the node origin). Size is either in
 * pixels (unscaled) or derived from the node-to-camera transform (scaled).
 * Negative crop width/height mirrors the image.
 *
 * @see Image2D, Appearance, Node
 */
struct Sprite3D : Node {
    Sprite3D() { object_type = ObjectTypes::SPRITE_3D; }
    std::optional<int> image_id;      /**< @brief Source Image2D object id. */
    std::optional<int> appearance_id; /**< @brief Appearance object id (compositing, etc.). */
    bool is_scaled = false;           /**< @brief true = size from node transform. */
    int crop_x = 0;                   /**< @brief Crop rectangle X in the source image. */
    int crop_y = 0;                   /**< @brief Crop rectangle Y in the source image. */
    int crop_width = 0;               /**< @brief Crop width (negative ⇒ mirror X). */
    int crop_height = 0;              /**< @brief Crop height (negative ⇒ mirror Y). */

    /**
     * @brief Gets the current Sprite3D image.
     * @return Image2D object id.
     */
    int getImage() const { return image_id.value_or(0); }
    /**
     * @brief Sets the sprite image to display.
     * @param imageId Non-zero Image2D object id.
     */
    void setImage(int imageId) {
        if (imageId == 0) throw std::invalid_argument("Sprite3D::setImage: null image");
        image_id = imageId;
    }
    /**
     * @brief Gets the current Appearance of this Sprite3D.
     * @return Appearance object id, or 0 if none.
     */
    int getAppearance() const { return appearance_id.value_or(0); }
    /**
     * @brief Sets the Appearance of this Sprite3D.
     * @param appearanceId Appearance object id, or 0 to clear.
     */
    void setAppearance(int appearanceId) {
        if (appearanceId == 0) appearance_id.reset();
        else appearance_id = appearanceId;
    }
    /**
     * @brief Returns the automatic scaling status of this Sprite3D.
     * @return true if scaled by the node-to-camera transform.
     */
    bool isScaled() const { return is_scaled; }
    /** @brief Crop rectangle X offset relative to the source image top-left. */
    int getCropX() const { return crop_x; }
    /** @brief Crop rectangle Y offset relative to the source image top-left. */
    int getCropY() const { return crop_y; }
    /** @brief Current cropping rectangle width within the source image. */
    int getCropWidth() const { return crop_width; }
    /** @brief Current cropping rectangle height within the source image. */
    int getCropHeight() const { return crop_height; }
    /**
     * @brief Sets a cropping rectangle within the source image.
     * @param cropX X offset of the crop rectangle.
     * @param cropY Y offset of the crop rectangle.
     * @param width Crop width (negative mirrors horizontally).
     * @param height Crop height (negative mirrors vertically).
     */
    void setCrop(int cropX, int cropY, int width, int height) {
        crop_x = cropX;
        crop_y = cropY;
        crop_width = width;
        crop_height = height;
    }

    std::shared_ptr<Object3D> duplicate() const override {
        return std::make_shared<Sprite3D>(*this);
    }
};

/**
 * @brief Controls the playback position, speed, and weight of one or more AnimationTracks.
 * @ingroup m3g_model
 *
 * Java: `javax.microedition.m3g.AnimationController` extends Object3D.
 *
 * Maps world time to sequence time via a reference pair (world time, sequence time)
 * and a speed factor. Weight blends multiple tracks targeting the same property.
 * The active interval limits when the controller contributes.
 *
 * @see AnimationTrack, KeyframeSequence, Object3D::animate
 */
struct AnimationController : Object3D {
    AnimationController() { object_type = ObjectTypes::ANIMATION_CONTROLLER; }
    float speed = 1.f;  /**< @brief Playback speed (1 = real-time relative to world time). */
    float weight = 1.f; /**< @brief Blend weight when multiple tracks target one property. */
    int active_interval_start = 0; /**< @brief Inclusive start of active world-time interval. */
    int active_interval_end = 0;   /**< @brief Exclusive end; start==end ⇒ always active (Java). */
    float reference_sequence_time = 0.f; /**< @brief Sequence time at the reference world time. */
    int reference_world_time = 0;        /**< @brief World time corresponding to the sequence reference. */

    /** @brief Gets the current playback speed. */
    float getSpeed() const { return speed; }
    /** @brief Sets the playback speed. */
    void setSpeed(float s) { speed = s; }
    /** @brief Gets the current blend weight. */
    float getWeight() const { return weight; }
    /** @brief Sets the blend weight. */
    void setWeight(float w) { weight = w; }
};

/**
 * @brief Associates a KeyframeSequence with a target property on an Object3D.
 * @ingroup m3g_model
 *
 * Java: `javax.microedition.m3g.AnimationTrack` extends Object3D.
 *
 * Attached to targets via @ref Object3D::addAnimationTrack. The target property is
 * one of @ref AnimationProperty (e.g. TRANSLATION, DIFFUSE_COLOR). An optional
 * @ref AnimationController drives timing and weight.
 *
 * @see KeyframeSequence, AnimationController, AnimationProperty
 */
struct AnimationTrack : Object3D {
    AnimationTrack() { object_type = ObjectTypes::ANIMATION_TRACK; }
    std::optional<int> keyframe_sequence_id;     /**< @brief KeyframeSequence object id. */
    std::optional<int> animation_controller_id;  /**< @brief AnimationController object id, or none. */
    int property_id = 0; /**< @brief Target property (@ref AnimationProperty constant). */

    /**
     * @brief Gets the property targeted by this AnimationTrack.
     * @return @ref AnimationProperty constant (e.g. TRANSLATION = 275).
     */
    int getTargetProperty() const { return property_id; }
};

/**
 * @brief One keyframe time + value vector.
 * @ingroup m3g_model
 */
struct Keyframe {
    int time = 0;
    std::vector<float> values;
};

/**
 * @brief A sequence of time-keyed values for animating a single property.
 * @ingroup m3g_model
 *
 * Java: `javax.microedition.m3g.KeyframeSequence` extends Object3D.
 *
 * Stores keyframes (time + value vector), interpolation mode, repeat mode, duration,
 * and valid range. Used by @ref AnimationTrack; sampled during @ref Object3D::animate.
 *
 * @see AnimationTrack, Keyframe, KeyframeInterpolation
 */
struct KeyframeSequence : Object3D {
    KeyframeSequence() { object_type = ObjectTypes::KEYFRAME_SEQUENCE; }
    /** @brief Linear interpolation between keyframes. Constant 176. */
    static constexpr int LINEAR = 176;
    /** @brief Spherical linear interpolation (orientations). Constant 177. */
    static constexpr int SLERP = 177;
    /** @brief Spline interpolation. Constant 178. */
    static constexpr int SPLINE = 178;
    /** @brief Spherical spline interpolation. Constant 179. */
    static constexpr int SQUAD = 179;
    /** @brief Step (constant until next keyframe). Constant 180. */
    static constexpr int STEP = 180;
    /** @brief Repeat mode: clamp outside the valid range. Constant 192. */
    static constexpr int CONSTANT = 192;
    /** @brief Repeat mode: loop the valid range. Constant 193. */
    static constexpr int LOOP = 193;

    int interpolation = 0;
    int repeat_mode = 0;
    int encoding = 0;
    int duration = 0;
    int valid_range_first = 0;
    int valid_range_last = 0;
    int component_count = 0;
    std::vector<Keyframe> keyframes;

    int getComponentCount() const { return component_count; }
    int getDuration() const { return duration; }
    int getInterpolationType() const { return interpolation; }
    int getRepeatMode() const { return repeat_mode; }
};

/**
 * @brief Unrecognized or skipped object payload.
 * @ingroup m3g_model
 */
struct Unknown : Object {
    std::vector<std::uint8_t> raw_data;
};

/**
 * @ingroup m3g_model
 * @brief Complete parsed M3G file (header, sections, object map).
 *
 * Closest analogue to the array returned by JSR-184 `Loader.load(...)`.
 */
struct File {
    std::shared_ptr<Header> header;                       /**< @brief File header object, if present. */
    std::vector<SectionInfo> sections;                    /**< @brief Section table. */
    std::map<int, std::shared_ptr<Object>> objects_by_id;  /**< @brief Objects keyed by @ref Object::object_id. */

    std::vector<std::shared_ptr<Object>> objects_in_order() const {
        std::vector<std::shared_ptr<Object>> out;
        out.reserve(objects_by_id.size());
        for (const auto &kv : objects_by_id) {
            out.push_back(kv.second);
        }
        return out;
    }

    /** @brief First World in id order, or null (like finding the scene root). */
    std::shared_ptr<World> world_or_null() const {
        for (const auto &obj : objects_in_order()) {
            if (auto w = std::dynamic_pointer_cast<World>(obj)) {
                return w;
            }
        }
        return nullptr;
    }

    std::shared_ptr<World> require_world() const {
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

    /** @brief All root objects as a flat list (Loader.load style). */
    std::vector<std::shared_ptr<Object3D>> getRootObjects() const {
        std::vector<std::shared_ptr<Object3D>> roots;
        for (const auto &obj : objects_in_order()) {
            if (auto o3d = std::dynamic_pointer_cast<Object3D>(obj)) {
                roots.push_back(o3d);
            }
        }
        return roots;
    }
};

bool is_identity_row_major(const std::vector<float> &m, float epsilon = 1e-5f);

} // namespace model

/**
 * @brief A generic 4×4 floating-point matrix representing a transformation.
 *
 * Java: `javax.microedition.m3g.Transform` (does **not** extend Object3D).
 *
 * By default methods accept arbitrary 4×4 matrices. Non-invertible matrices may
 * yield undefined results for normal transform and fogging. Storage is row-major
 * in this C++ port.
 *
 * @see Transformable, Graphics3D, Camera
 */
class Transform {
public:
    /** @brief Constructs a new Transform initialized to the 4×4 identity matrix. */
    Transform() { setIdentity(); }
    /** @brief Constructs a Transform by copying a 16-element row-major array. */
    explicit Transform(const float *m16_row_major) { set(m16_row_major); }
    /** @brief Constructs a Transform from a vector of at least 16 floats. */
    explicit Transform(const std::vector<float> &m16) {
        if (m16.size() >= 16) set(m16.data());
        else setIdentity();
    }

    /** @brief Sets this transformation to the 4×4 identity matrix. */
    void setIdentity() {
        for (int i = 0; i < 16; ++i) m_[static_cast<std::size_t>(i)] = (i % 5 == 0) ? 1.f : 0.f;
    }
    /**
     * @brief Sets this transformation by copying from a 16-element float array.
     * @param m16_row_major Source matrix in row-major order.
     */
    void set(const float *m16_row_major) {
        for (int i = 0; i < 16; ++i) m_[static_cast<std::size_t>(i)] = m16_row_major[i];
    }
    /**
     * @brief Retrieves the contents of this transformation as 16 floats.
     * @param[out] m16_row_major Destination length ≥ 16, row-major.
     */
    void get(float *m16_row_major) const {
        for (int i = 0; i < 16; ++i) m16_row_major[i] = m_[static_cast<std::size_t>(i)];
    }
    /** @brief Direct access to the internal 16 floats (row-major). */
    const float *data() const { return m_.data(); }
    /** @brief Mutable direct access to the internal 16 floats. */
    float *data() { return m_.data(); }

    /**
     * @brief Multiplies this transformation from the right by the given transformation.
     *
     * Computes `this = this * other` (Java `postMultiply`).
     *
     * @param other Right-hand matrix.
     */
    void postMultiply(const Transform &other) {
        float out[16];
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                out[r * 4 + c] = m_[static_cast<std::size_t>(r * 4 + 0)] * other.m_[static_cast<std::size_t>(0 * 4 + c)] +
                                 m_[static_cast<std::size_t>(r * 4 + 1)] * other.m_[static_cast<std::size_t>(1 * 4 + c)] +
                                 m_[static_cast<std::size_t>(r * 4 + 2)] * other.m_[static_cast<std::size_t>(2 * 4 + c)] +
                                 m_[static_cast<std::size_t>(r * 4 + 3)] * other.m_[static_cast<std::size_t>(3 * 4 + c)];
            }
        }
        set(out);
    }

    /**
     * @brief Multiplies this transformation from the right by a translation matrix.
     * @param tx X translation.
     * @param ty Y translation.
     * @param tz Z translation.
     */
    void postTranslate(float tx, float ty, float tz) {
        Transform t;
        t.m_[12] = tx;
        t.m_[13] = ty;
        t.m_[14] = tz;
        postMultiply(t);
    }

    /**
     * @brief Multiplies this transformation from the right by a scale matrix.
     * @param sx X scale.
     * @param sy Y scale.
     * @param sz Z scale.
     */
    void postScale(float sx, float sy, float sz) {
        Transform t;
        t.m_[0] = sx;
        t.m_[5] = sy;
        t.m_[10] = sz;
        postMultiply(t);
    }

private:
    std::array<float, 16> m_{};
};

/* --- Transformable / Node / Camera methods needing complete Transform --- */

inline void model::Transformable::getTransform(Transform *transform) const {
    if (!transform) throw std::invalid_argument("getTransform: null Transform");
    float m[16];
    getTransform(m);
    transform->set(m);
}
inline void model::Transformable::setTransform(const Transform *transform) {
    if (!transform) throw std::invalid_argument("setTransform: null Transform");
    float m[16];
    transform->get(m);
    setTransform(m);
}
inline void model::Transformable::getCompositeTransform(Transform *transform) const {
    if (!transform) throw std::invalid_argument("getCompositeTransform: null Transform");
    float m[16];
    getCompositeTransform(m);
    transform->set(m);
}
inline bool model::Node::getTransformTo(const Node *target, Transform *transform) const {
    if (!transform) return false;
    float m[16];
    if (!getTransformTo(target, m)) return false;
    transform->set(m);
    return true;
}
inline int model::Camera::getProjection(Transform *transform) const {
    if (!transform) throw std::invalid_argument("Camera::getProjection: null Transform");
    float m[16];
    if (projection_type == GENERIC && generic && generic->values.size() >= 16) {
        for (int i = 0; i < 16; ++i) m[i] = generic->values[static_cast<std::size_t>(i)];
        transform->set(m);
        return GENERIC;
    }
    for (int i = 0; i < 16; ++i) m[i] = (i % 5 == 0) ? 1.f : 0.f;
    transform->set(m);
    return projection_type;
}
inline void model::Camera::setGeneric(const Transform *transform) {
    if (!transform) throw std::invalid_argument("Camera::setGeneric: null Transform");
    float m[16];
    transform->get(m);
    setGeneric(m);
}

/**
 * @brief Result filled in by the pick methods in Group.
 *
 * Java: `javax.microedition.m3g.RayIntersection` (does **not** extend Object3D).
 *
 * Stores a reference to the intersected Mesh or Sprite3D and information about the
 * intersection point. Strictly a run-time object; cannot be loaded by Loader.
 *
 * @see Group::pick
 */
struct RayIntersection {
    std::shared_ptr<model::Node> intersected; /**< @brief Picked Mesh or Sprite3D, if any. */
    float distance = 0.f; /**< @brief Distance from pick ray origin to the intersection. */
    float normal[3]{0.f, 0.f, 1.f}; /**< @brief Surface normal at the intersection. */
    float texture_s[2]{}; /**< @brief S texture coordinates (up to 2 units). */
    float texture_t[2]{}; /**< @brief T texture coordinates (up to 2 units). */
    int submesh_index = 0; /**< @brief Submesh index within the intersected Mesh. */

    /** @brief Retrieves the distance from the pick ray origin to the intersection point. */
    float getDistance() const { return distance; }
    /** @brief Retrieves the picked Mesh or Sprite3D object (may be null). */
    model::Node *getIntersected() const { return intersected.get(); }
    /** @brief Retrieves the submesh index of the intersection within the Mesh. */
    int getSubmeshIndex() const { return submesh_index; }
    /**
     * @brief Retrieves the surface normal at the intersection point.
     * @param[out] xyz Length ≥ 3.
     */
    void getNormal(float *xyz) const {
        xyz[0] = normal[0];
        xyz[1] = normal[1];
        xyz[2] = normal[2];
    }
};

/**
 * @brief Singleton 3D graphics context bound to a rendering target.
 *
 * Java: `javax.microedition.m3g.Graphics3D` (does **not** extend Object3D).
 *
 * All rendering goes through this class, including World retained-mode rendering.
 * Typical usage: getInstance → bindTarget → render/clear → releaseTarget.
 *
 * @par Immediate vs retained mode
 * - Retained: @ref render(const model::World*) uses the World's camera and lights.
 * - Immediate: node/submesh render methods use Graphics3D current camera/lights.
 *
 * @note This toolkit focuses on load/convert; bind/render are API-shaped stubs.
 *
 * @see World, Camera, Light, Background
 */
class Graphics3D {
public:
    /** @brief Hint: enable antialiasing if available (`bindTarget`). */
    static constexpr int ANTIALIAS = 2;
    /** @brief Hint: enable dithering if available. */
    static constexpr int DITHER = 4;
    /** @brief Hint: prefer true color if available. */
    static constexpr int TRUE_COLOR = 8;
    /** @brief Hint: overwrite existing framebuffer contents. */
    static constexpr int OVERWRITE = 16;

    /**
     * @brief Returns the singleton Graphics3D instance.
     * @return Process-wide Graphics3D.
     */
    static Graphics3D &getInstance() {
        static Graphics3D instance;
        return instance;
    }

    /**
     * @brief Binds a rendering target to this Graphics3D.
     * @param target Platform-specific target (e.g. Graphics); unused in this stub.
     * @param depth_buffer Whether a depth buffer is requested.
     * @param hints Bitwise OR of @ref ANTIALIAS, @ref DITHER, @ref TRUE_COLOR, @ref OVERWRITE.
     * @note No-op stub (no framebuffer backend).
     */
    void bindTarget(void * /*target*/, bool /*depth_buffer*/ = true, int /*hints*/ = 0) {}
    /** @brief Releases the currently bound rendering target. */
    void releaseTarget() {}
    /**
     * @brief Sets the viewport on the currently bound target.
     * @param x Viewport lower-left X in pixels.
     * @param y Viewport lower-left Y in pixels.
     * @param width Viewport width in pixels.
     * @param height Viewport height in pixels.
     */
    void setViewport(int x, int y, int width, int height) {
        viewport_x_ = x;
        viewport_y_ = y;
        viewport_w_ = width;
        viewport_h_ = height;
    }
    /**
     * @brief Retrieves the current viewport as (x, y, width, height).
     * @param[out] xywh Length ≥ 4.
     */
    void getViewport(int *xywh) const {
        xywh[0] = viewport_x_;
        xywh[1] = viewport_y_;
        xywh[2] = viewport_w_;
        xywh[3] = viewport_h_;
    }

    /**
     * @brief Renders an entire World (retained mode).
     * @param world World to render; uses its active camera and lights.
     */
    void render(const model::World * /*world*/) {}
    /**
     * @brief Renders a scene graph node in immediate mode.
     * @param node Node (or Group) to render.
     * @param transform Optional transform from node to world; may be null.
     */
    void render(const model::Node * /*node*/, const Transform * /*transform*/) {}
    /**
     * @brief Clears the viewport using the given Background.
     * @param background Background attributes; may be null for defaults.
     */
    void clear(const model::Background * /*background*/) {}

    /** @brief Current viewport width in pixels. */
    int getViewportWidth() const { return viewport_w_; }
    /** @brief Current viewport height in pixels. */
    int getViewportHeight() const { return viewport_h_; }

private:
    Graphics3D() = default;
    int viewport_x_ = 0;
    int viewport_y_ = 0;
    int viewport_w_ = 0;
    int viewport_h_ = 0;
};

/* Java package-style aliases: m3g::World instead of m3g::model::World. */
using Object3D = model::Object3D;
using Transformable = model::Transformable;
using Node = model::Node;
using Group = model::Group;
using World = model::World;
using Camera = model::Camera;
using Light = model::Light;
using Mesh = model::Mesh;
using SkinnedMesh = model::SkinnedMesh;
using MorphingMesh = model::MorphingMesh;
using Sprite3D = model::Sprite3D;
using Background = model::Background;
using Fog = model::Fog;
using PolygonMode = model::PolygonMode;
using CompositingMode = model::CompositingMode;
using Material = model::Material;
using Appearance = model::Appearance;
using Texture2D = model::Texture2D;
using Image2D = model::Image2D;
using VertexArray = model::VertexArray;
using VertexBuffer = model::VertexBuffer;
using IndexBuffer = model::IndexBuffer;
using TriangleStripArray = model::TriangleStripArray;
using AnimationController = model::AnimationController;
using AnimationTrack = model::AnimationTrack;
using KeyframeSequence = model::KeyframeSequence;

/**
 * @ingroup m3g_scene
 * @namespace m3g::scene
 * @brief Intermediate scene graph used for glTF export.
 */
namespace scene {

/**
 * @ingroup m3g_scene
 * @brief Non-fatal conversion note (code + message).
 */
struct ConversionWarning {
    std::string code;
    std::string message;
    bool operator==(const ConversionWarning &o) const {
        return code == o.code && message == o.message;
    }
};

/**
 * @brief RGBA8 pixels embedded in the IR (from M3G Image2D).
 * @ingroup m3g_scene
 */
struct EmbeddedRgbaImageSource {
    int object_id = 0;
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> pixels;
};

/**
 * @brief Image referenced by filesystem path.
 * @ingroup m3g_scene
 */
struct ExternalFileImageSource {
    int object_id = 0;
    std::string source_path;
};

/**
 * @brief One glTF image source (exactly one of embedded / external).
 * @ingroup m3g_scene
 */
struct SceneImageIr {
    std::string name;
    std::optional<EmbeddedRgbaImageSource> embedded;
    std::optional<ExternalFileImageSource> external;
};

/**
 * @brief glTF sampler parameters.
 * @ingroup m3g_scene
 */
struct SceneSamplerIr {
    std::optional<int> mag_filter;
    std::optional<int> min_filter;
    int wrap_s = 0;
    int wrap_t = 0;
};

/**
 * @brief glTF texture linking image + optional sampler.
 * @ingroup m3g_scene
 */
struct SceneTextureIr {
    std::string name;
    int image_index = 0;
    std::optional<int> sampler_index;
};

/**
 * @brief glTF PBR metallic-roughness material fields.
 * @ingroup m3g_scene
 */
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

/**
 * @brief Triangle mesh primitive (interleaved-friendly arrays).
 * @ingroup m3g_scene
 */
struct ScenePrimitiveIr {
    std::string name;
    std::vector<float> positions;
    std::optional<std::vector<float>> normals;
    std::optional<std::vector<float>> tex_coords0;
    std::optional<std::vector<float>> vertex_colors;
    std::vector<int> indices;
    std::optional<int> material_index;
};

/**
 * @brief Mesh with one or more primitives.
 * @ingroup m3g_scene
 */
struct SceneMeshIr {
    std::string name;
    std::vector<ScenePrimitiveIr> primitives;
};

/**
 * @brief Perspective camera parameters (radians).
 * @ingroup m3g_scene
 */
struct ScenePerspectiveCameraIr {
    float yfov_radians = 0.f;
    std::optional<float> aspect_ratio;
    float znear = 0.1f;
    std::optional<float> zfar;
};

/**
 * @brief Named camera (perspective optional).
 * @ingroup m3g_scene
 */
struct SceneCameraIr {
    std::string name;
    std::optional<ScenePerspectiveCameraIr> perspective;
};

/**
 * @brief Scene graph node; TRS and/or matrix, mesh/camera refs, children.
 * @ingroup m3g_scene
 */
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

/**
 * @brief Animation sampler input/output floats.
 * @ingroup m3g_scene
 */
struct SceneAnimationSamplerIr {
    std::vector<float> times;
    std::vector<float> values;
    std::string interpolation = "LINEAR";
    int component_count = 3;
};

/**
 * @brief Channel binding sampler → node path.
 * @ingroup m3g_scene
 */
struct SceneAnimationChannelIr {
    int sampler_index = 0;
    int node_index = 0;
    std::string path;
};

/**
 * @brief Named animation clip.
 * @ingroup m3g_scene
 */
struct SceneAnimationIr {
    std::string name;
    std::vector<SceneAnimationSamplerIr> samplers;
    std::vector<SceneAnimationChannelIr> channels;
};

/**
 * @ingroup m3g_scene
 * @brief Full intermediate representation ready for glTF export.
 *
 * Indices in nodes/meshes/materials/… refer to positions in the parallel arrays.
 */
struct SceneIr {
    std::vector<SceneNodeIr> nodes;
    std::vector<int> root_node_indices;  /**< @brief Indices into @c nodes for the default scene. */
    std::vector<SceneMeshIr> meshes;
    std::vector<SceneMaterialIr> materials;
    std::vector<SceneTextureIr> textures;
    std::vector<SceneImageIr> images;
    std::vector<SceneSamplerIr> samplers;
    std::vector<SceneCameraIr> cameras;
    std::vector<SceneAnimationIr> animations;
    std::vector<ConversionWarning> warnings;
};

/**
 * @ingroup m3g_scene
 * @brief Paths produced by a glTF write (mirrors @ref m3g::exp::GltfPaths).
 */
struct GltfWriteResult {
    std::string gltf_path;                 /**< @brief Written `.gltf` / `.glb` path. */
    std::string bin_path;                  /**< @brief Companion `.bin` when applicable. */
    std::vector<std::string> image_paths;  /**< @brief External image files written, if any. */
};

} // namespace scene

/**
 * @ingroup m3g_decode
 * @namespace m3g::decode
 * @brief High-level M3G decode entry points.
 */
namespace decode {

/**
 * @ingroup m3g_decode
 * @brief Result of decoding one M3G asset.
 */
struct Decoded {
    std::string source_path;   /**< @brief Normalized path or caller-supplied label. */
    model::File file;          /**< @brief Parsed object graph. */
    scene::SceneIr scene_ir;   /**< @brief Export-ready intermediate scene. */

    /** @name Scene tallies
     *  @{
     */
    std::size_t node_count() const { return scene_ir.nodes.size(); }
    std::size_t mesh_count() const { return scene_ir.meshes.size(); }
    std::size_t material_count() const { return scene_ir.materials.size(); }
    std::size_t texture_count() const { return scene_ir.textures.size(); }
    std::size_t image_count() const { return scene_ir.images.size(); }
    std::size_t camera_count() const { return scene_ir.cameras.size(); }
    std::size_t animation_count() const { return scene_ir.animations.size(); }
    /** @} */

    /** @brief Conversion warnings collected while building @c scene_ir. */
    const std::vector<scene::ConversionWarning> &warnings() const { return scene_ir.warnings; }
};

/**
 * @ingroup m3g_decode
 * @brief Optional decode knobs.
 */
struct DecodeOptions {
    /**
     * @brief Optional external pattern image applied to untextured materials.
     */
    std::optional<std::string> pattern_path;
};

/**
 * @ingroup m3g_decode
 * @brief M3G path/bytes → @ref Decoded (parse + scene build + optional pattern).
 *
 * Requires @ref m3g::DeflateIo when the file uses compressed sections, and
 * @ref m3g::ImageIo when decoding embedded/external raster images.
 */
class Decoder {
public:
    /**
     * @brief Decode an `.m3g` file from disk.
     * @param input_path Filesystem path.
     * @param options Optional pattern path, etc.
     * @return Value-semantic @ref Decoded.
     * @throws std::runtime_error on I/O or parse failure.
     */
    Decoded decode_file(const std::string &input_path, const DecodeOptions &options = {}) const;

    /**
     * @brief Decode M3G bytes already in memory.
     * @param bytes File contents.
     * @param source_path Label used for diagnostics / relative resolves (may be empty).
     * @param options Optional pattern path, etc.
     */
    Decoded decode_bytes(const std::vector<std::uint8_t> &bytes, const std::string &source_path,
                         const DecodeOptions &options = {}) const;
};

/**
 * @ingroup m3g_decode
 * @brief JSR-184-style `Loader` facade over @ref Decoder.
 *
 * Java: `Object3D[] Loader.load(String name)` / `Loader.load(byte[] data, int offset)`.
 * Here @ref load returns the full @ref Decoded (object graph + scene IR).
 */
class Loader {
public:
    static Decoded load(const std::string &name, const DecodeOptions &options = {}) {
        return Decoder{}.decode_file(name, options);
    }
    static Decoded load(const std::vector<std::uint8_t> &data, const std::string &source_path = {},
                        const DecodeOptions &options = {}) {
        return Decoder{}.decode_bytes(data, source_path, options);
    }
};

} // namespace decode

/**
 * @ingroup m3g_decode
 * @brief Alias at `m3g::Loader` for Java-like `javax.microedition.m3g.Loader`.
 */
using Loader = decode::Loader;

/**
 * @ingroup m3g_export
 * @namespace m3g::exp
 * @brief glTF export types and writer.
 */
namespace exp {

/**
 * @ingroup m3g_export
 * @brief On-disk paths produced by a successful export.
 */
struct GltfPaths {
    std::string gltf_path;                /**< @brief Output `.gltf` or `.glb`. */
    std::string bin_path;                 /**< @brief `.bin` path when separate. */
    std::vector<std::string> image_paths; /**< @brief External image files written. */
};

/**
 * @ingroup m3g_export
 * @brief Decode result plus export paths (full pipeline report).
 */
struct ExportReport {
    decode::Decoded decoded; /**< @brief Decoded source asset. */
    GltfPaths paths;         /**< @brief Written glTF artifacts. */
};

/**
 * @ingroup m3g_export
 * @brief Write @ref m3g::scene::SceneIr (or a decode result) as glTF 2.0.
 *
 * Implementation uses cgltf_write; PNG encode may use stb_image_write.
 */
class GltfExporter {
public:
    /**
     * @brief Export @p decoded.scene_ir.
     * @param decoded Previously decoded asset (uses @c scene_ir).
     * @param output_path Destination ending in `.gltf` or `.glb`.
     * @param overwrite Replace existing outputs when true.
     * @param png_compression_level zlib level 0–9 for written PNGs.
     */
    GltfPaths write(const decode::Decoded &decoded, const std::string &output_path, bool overwrite,
                    int png_compression_level = 8) const;

    /** @brief Export a scene IR directly. */
    GltfPaths write(const scene::SceneIr &scene_ir, const std::string &output_path, bool overwrite,
                    int png_compression_level = 8) const;
};

} // namespace exp

/**
 * @ingroup m3g_export
 * @brief Facade: decode M3G and/or export glTF in one object.
 *
 * Prefer @ref m3g::decode::Decoder / @ref m3g::exp::GltfExporter for a single stage.
 */
class Converter {
public:
    /**
     * @brief Decode only (`Decoder` + optional pattern).
     * @param input_path Filesystem path to the `.m3g` file.
     * @param pattern_path External pattern image for untextured materials.
     */
    decode::Decoded decode(const std::string &input_path,
                           const std::optional<std::string> &pattern_path = std::nullopt) const;

    /** @brief Export a previously decoded asset. */
    exp::GltfPaths export_gltf(const decode::Decoded &decoded, const std::string &output_path, bool overwrite,
                               int png_compression_level = 8) const;

    /**
     * @brief Full pipeline: decode @p input_path then write @p output_path.
     * @return Combined @ref exp::ExportReport.
     */
    exp::ExportReport convert(const std::string &input_path, const std::string &output_path, bool overwrite,
                              const std::optional<std::string> &pattern_path = std::nullopt,
                              int png_compression_level = 8) const;
};

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

std::vector<float> identity_matrix_row_major();

std::vector<float> multiply_row_major(const std::vector<float> &left, const std::vector<float> &right);

std::vector<float> translation_matrix_row_major(float x, float y, float z);

std::vector<float> scale_matrix_row_major(float x, float y, float z);

std::vector<float> axis_angle_matrix_row_major(float angle_radians, float axis_x, float axis_y, float axis_z);

std::vector<float> component_transform_to_row_major(const model::ComponentTransform &component);

std::vector<float> node_matrix_row_major(const model::Transformable &transformable);

struct DecomposedTrs {
    std::vector<float> translation{0.f, 0.f, 0.f};
    std::vector<float> rotation{0.f, 0.f, 0.f, 1.f}; // xyzw
    std::vector<float> scale{1.f, 1.f, 1.f};
};

float vec3_length(float x, float y, float z);

std::vector<float> quaternion_from_axis_angle_degrees(float angle_degrees, float ax, float ay, float az);

std::vector<float> quaternion_from_row_major_rotation(const float r[3][3]);

DecomposedTrs decompose_row_major_trs(const std::vector<float> &matrix);

std::vector<double> row_major_to_column_major_list(const std::vector<float> &matrix);

} // namespace scene
} // namespace m3g

#endif /* M3G_MATRIX_UTIL_INCLUDED */

#endif /* M3G_HPP_INCLUDED */

/* ============================ IMPLEMENTATION ============================ */
#ifndef DOXYGEN
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


/* ---- non-inline helpers (M3G_IMPL / M3G_DECODE_IMPL) ---- */
namespace m3g {

namespace {

static DeflateIo &deflate_io_storage(){
    static DeflateIo storage{};
    return storage;
}

static ImageIo &image_io_storage(){
    static ImageIo storage{};
    return storage;
}

static JsonIo &json_io_storage(){
    static JsonIo storage{};
    return storage;
}

static GltfIo &gltf_io_storage(){
    static GltfIo storage{};
    return storage;
}

} // namespace

std::uint32_t deflate_adler32(std::uint32_t adler, unsigned char const *ptr, std::size_t buf_len){
    DeflateIo const *io = deflate_io();
    if (!io || !io->adler32) {
        throw std::runtime_error("m3g deflate I/O: adler32 callback not set (call m3g::set_deflate_io or install_miniz_deflate_io)");
    }
    return io->adler32(adler, ptr, buf_len, io->user);
}

int deflate_uncompress(unsigned char *dest, std::size_t *dest_len, unsigned char const *source,
                              std::size_t source_len){
    DeflateIo const *io = deflate_io();
    if (!io || !io->uncompress) {
        throw std::runtime_error("m3g deflate I/O: uncompress callback not set (call m3g::set_deflate_io or install_miniz_deflate_io)");
    }
    return io->uncompress(dest, dest_len, source, source_len, io->user);
}

void *deflate_uncompress_to_heap(unsigned char const *source, std::size_t source_len, std::size_t *out_len,
                                        int zlib_header){
    DeflateIo const *io = deflate_io();
    if (!io || !io->uncompress_to_heap) {
        throw std::runtime_error("m3g deflate I/O: uncompress_to_heap callback not set (call m3g::set_deflate_io or install_miniz_deflate_io)");
    }
    return io->uncompress_to_heap(source, source_len, out_len, zlib_header, io->user);
}

void deflate_free(void *p){
    if (!p) {
        return;
    }
    DeflateIo const *io = deflate_io();
    if (!io || !io->free_mem) {
        throw std::runtime_error("m3g deflate I/O: free_mem callback not set");
    }
    io->free_mem(p, io->user);
}

unsigned char *image_load_file(char const *filename, int *width, int *height, int *channels_in_file,
                                     int req_comp){
    ImageIo const *io = image_io();
    if (!io || !io->load_file) {
        throw std::runtime_error("m3g image I/O: load_file callback not set (call m3g::set_image_io or install_stb_image_io)");
    }
    return io->load_file(filename, width, height, channels_in_file, req_comp, io->user);
}

unsigned char *image_load_memory(unsigned char const *buffer, int len, int *width, int *height,
                                       int *channels_in_file, int req_comp){
    ImageIo const *io = image_io();
    if (!io || !io->load_memory) {
        throw std::runtime_error("m3g image I/O: load_memory callback not set (call m3g::set_image_io or install_stb_image_io)");
    }
    return io->load_memory(buffer, len, width, height, channels_in_file, req_comp, io->user);
}

void image_free_pixels(void *pixels){
    if (!pixels) {
        return;
    }
    ImageIo const *io = image_io();
    if (!io || !io->free_pixels) {
        throw std::runtime_error("m3g image I/O: free_pixels callback not set");
    }
    io->free_pixels(pixels, io->user);
}

static GltfIo const *require_gltf_io(char const *what){
    GltfIo const *io = gltf_io();
    if (!io) {
        throw std::runtime_error(std::string("m3g glTF I/O: not set (need ") + what +
                                 "; call m3g::set_gltf_io or install_cgltf_gltf_io)");
    }
    return io;
}

void gltf_write_file(char const *path, void const *data, int kind){
    GltfIo const *io = require_gltf_io("write_file");
    if (!io->write_file) {
        throw std::runtime_error("m3g glTF I/O: write_file callback not set");
    }
    const int rc = io->write_file(path, data, kind, io->user);
    if (rc != GLTF_IO_OK) {
        throw std::runtime_error(std::string("m3g glTF I/O: write_file failed (rc ") + std::to_string(rc) +
                                 ") for " + path);
    }
}

void gltf_parse_file(char const *path, void **out_data){
    GltfIo const *io = require_gltf_io("parse_file");
    if (!io->parse_file) {
        throw std::runtime_error("m3g glTF I/O: parse_file callback not set");
    }
    const int rc = io->parse_file(path, out_data, io->user);
    if (rc != GLTF_IO_OK) {
        throw std::runtime_error(std::string("m3g glTF I/O: parse_file failed (rc ") + std::to_string(rc) +
                                 ") for " + path);
    }
}

void gltf_validate(void *data){
    GltfIo const *io = require_gltf_io("validate");
    if (!io->validate) {
        throw std::runtime_error("m3g glTF I/O: validate callback not set");
    }
    const int rc = io->validate(data, io->user);
    if (rc != GLTF_IO_OK) {
        throw std::runtime_error(std::string("m3g glTF I/O: validate failed (rc ") + std::to_string(rc) + ")");
    }
}

void gltf_free_data(void *data){
    if (!data) {
        return;
    }
    GltfIo const *io = require_gltf_io("free_data");
    if (!io->free_data) {
        throw std::runtime_error("m3g glTF I/O: free_data callback not set");
    }
    io->free_data(data, io->user);
}

static JsonIo const *require_json_io(char const *what){
    JsonIo const *io = json_io();
    if (!io) {
        throw std::runtime_error(std::string("m3g JSON I/O: not set (need ") + what +
                                 "; call m3g::set_json_io or install_cjson_json_io)");
    }
    return io;
}

void *json_create_object(){
    JsonIo const *io = require_json_io("create_object");
    if (!io->create_object) {
        throw std::runtime_error("m3g JSON I/O: create_object callback not set");
    }
    return io->create_object(io->user);
}

void *json_create_array(){
    JsonIo const *io = require_json_io("create_array");
    if (!io->create_array) {
        throw std::runtime_error("m3g JSON I/O: create_array callback not set");
    }
    return io->create_array(io->user);
}

void *json_create_string(char const *s){
    JsonIo const *io = require_json_io("create_string");
    if (!io->create_string) {
        throw std::runtime_error("m3g JSON I/O: create_string callback not set");
    }
    return io->create_string(s, io->user);
}

void *json_create_number(double v){
    JsonIo const *io = require_json_io("create_number");
    if (!io->create_number) {
        throw std::runtime_error("m3g JSON I/O: create_number callback not set");
    }
    return io->create_number(v, io->user);
}

void *json_create_bool(int v){
    JsonIo const *io = require_json_io("create_bool");
    if (!io->create_bool) {
        throw std::runtime_error("m3g JSON I/O: create_bool callback not set");
    }
    return io->create_bool(v, io->user);
}

void json_add_item_to_object(void *object, char const *key, void *item){
    JsonIo const *io = require_json_io("add_item_to_object");
    if (!io->add_item_to_object) {
        throw std::runtime_error("m3g JSON I/O: add_item_to_object callback not set");
    }
    io->add_item_to_object(object, key, item, io->user);
}

void json_add_item_to_array(void *array, void *item){
    JsonIo const *io = require_json_io("add_item_to_array");
    if (!io->add_item_to_array) {
        throw std::runtime_error("m3g JSON I/O: add_item_to_array callback not set");
    }
    io->add_item_to_array(array, item, io->user);
}

int json_get_array_size(void const *array){
    JsonIo const *io = require_json_io("get_array_size");
    if (!io->get_array_size) {
        throw std::runtime_error("m3g JSON I/O: get_array_size callback not set");
    }
    return io->get_array_size(array, io->user);
}

void json_delete(void *node){
    if (!node) {
        return;
    }
    JsonIo const *io = require_json_io("delete_node");
    if (!io->delete_node) {
        throw std::runtime_error("m3g JSON I/O: delete_node callback not set");
    }
    io->delete_node(node, io->user);
}

char *json_print_unformatted(void *node){
    JsonIo const *io = require_json_io("print_unformatted");
    if (!io->print_unformatted) {
        throw std::runtime_error("m3g JSON I/O: print_unformatted callback not set");
    }
    return io->print_unformatted(node, io->user);
}

char *json_print_formatted(void *node){
    JsonIo const *io = require_json_io("print_formatted");
    if (!io->print_formatted) {
        throw std::runtime_error("m3g JSON I/O: print_formatted callback not set");
    }
    return io->print_formatted(node, io->user);
}

void json_free_print(char *printed){
    if (!printed) {
        return;
    }
    JsonIo const *io = require_json_io("free_print");
    if (!io->free_print) {
        throw std::runtime_error("m3g JSON I/O: free_print callback not set");
    }
    io->free_print(printed, io->user);
}

void set_deflate_io(DeflateIo const *io){
    if (!io) {
        deflate_io_storage() = DeflateIo{};
        return;
    }
    deflate_io_storage() = *io;
}

DeflateIo const *deflate_io(void){
    DeflateIo const &s = deflate_io_storage();
    if (!s.adler32 && !s.uncompress && !s.uncompress_to_heap && !s.free_mem) {
        return nullptr;
    }
    return &s;
}

void set_image_io(ImageIo const *io){
    if (!io) {
        image_io_storage() = ImageIo{};
        return;
    }
    image_io_storage() = *io;
}

ImageIo const *image_io(void){
    ImageIo const &s = image_io_storage();
    if (!s.load_file && !s.load_memory && !s.free_pixels) {
        return nullptr;
    }
    return &s;
}

void set_json_io(JsonIo const *io){
    if (!io) {
        json_io_storage() = JsonIo{};
        return;
    }
    json_io_storage() = *io;
}

JsonIo const *json_io(void){
    JsonIo const &s = json_io_storage();
    if (!s.create_object && !s.create_array && !s.create_string && !s.create_number && !s.create_bool &&
        !s.add_item_to_object && !s.add_item_to_array && !s.get_array_size && !s.delete_node &&
        !s.print_unformatted && !s.print_formatted && !s.free_print) {
        return nullptr;
    }
    return &s;
}

void set_gltf_io(GltfIo const *io){
    if (!io) {
        gltf_io_storage() = GltfIo{};
        return;
    }
    gltf_io_storage() = *io;
}

GltfIo const *gltf_io(void){
    GltfIo const &s = gltf_io_storage();
    if (!s.write_file && !s.parse_file && !s.validate && !s.free_data) {
        return nullptr;
    }
    return &s;
}

namespace model {

std::string type_name_for_object_type(int object_type){
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

bool is_identity_row_major(const std::vector<float> &m, float epsilon ){
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

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

std::vector<float> identity_matrix_row_major(){
    return {
        1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f,
    };
}

std::vector<float> multiply_row_major(const std::vector<float> &left, const std::vector<float> &right){
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

std::vector<float> translation_matrix_row_major(float x, float y, float z){
    auto m = identity_matrix_row_major();
    m[3] = x;
    m[7] = y;
    m[11] = z;
    return m;
}

std::vector<float> scale_matrix_row_major(float x, float y, float z){
    return {
        x, 0.f, 0.f, 0.f, 0.f, y, 0.f, 0.f, 0.f, 0.f, z, 0.f, 0.f, 0.f, 0.f, 1.f,
    };
}

std::vector<float> axis_angle_matrix_row_major(float angle_radians, float axis_x, float axis_y, float axis_z){
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

std::vector<float> component_transform_to_row_major(const model::ComponentTransform &component){
    const auto translation =
        translation_matrix_row_major(component.translation[0], component.translation[1], component.translation[2]);
    const auto rotation = axis_angle_matrix_row_major(component.orientation_angle, component.orientation_axis[0],
                                                      component.orientation_axis[1], component.orientation_axis[2]);
    const auto scale = scale_matrix_row_major(component.scale[0], component.scale[1], component.scale[2]);
    return multiply_row_major(translation, multiply_row_major(rotation, scale));
}

std::vector<float> node_matrix_row_major(const model::Transformable &transformable){
    if (transformable.general_transform) {
        return *transformable.general_transform;
    }
    if (transformable.component_transform) {
        return component_transform_to_row_major(*transformable.component_transform);
    }
    return identity_matrix_row_major();
}

float vec3_length(float x, float y, float z){ return std::sqrt(x * x + y * y + z * z); }

std::vector<float> quaternion_from_axis_angle_degrees(float angle_degrees, float ax, float ay, float az){
    const float len = vec3_length(ax, ay, az);
    if (len < 1e-8f || std::abs(angle_degrees) < 1e-8f) {
        return {0.f, 0.f, 0.f, 1.f};
    }
    const float half = (angle_degrees * static_cast<float>(M_PI) / 180.f) * 0.5f;
    const float s = std::sin(half);
    const float c = std::cos(half);
    return {ax / len * s, ay / len * s, az / len * s, c};
}

std::vector<float> quaternion_from_row_major_rotation(const float r[3][3]){
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

DecomposedTrs decompose_row_major_trs(const std::vector<float> &matrix){
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

std::vector<double> row_major_to_column_major_list(const std::vector<float> &matrix){
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

/* internal: BinaryReader */

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace m3g {
namespace model {
namespace { // private

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

} // namespace
} // namespace model
} // namespace m3g

/* internal: M3G Parser */


#include <string>
#include <vector>

namespace m3g {
namespace model {
namespace { // private

class Parser {
public:
    File parse_path(const std::string &path) const;
    File parse(const std::vector<std::uint8_t> &bytes) const;
};

} // namespace
} // namespace model
} // namespace m3g



#include <cstring>
#include <fstream>
#include <stdexcept>


namespace m3g {
namespace model {
namespace { // private

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

void read_object3d(BinaryReader &reader, Object3D &obj) {
    /* Spec §11.19 Object3D */
    obj.user_id = to_checked_int(reader.read_u32_le(), "user ID");
    const int animation_track_count = to_checked_int(reader.read_u32_le(), "animation track count");
    obj.animation_track_ids.reserve(static_cast<std::size_t>(animation_track_count));
    for (int i = 0; i < animation_track_count; ++i) {
        obj.animation_track_ids.push_back(read_object_ref(reader));
    }
    const int user_parameter_count = to_checked_int(reader.read_u32_le(), "user parameter count");
    obj.user_parameters.clear();
    obj.user_parameters.reserve(static_cast<std::size_t>(user_parameter_count));
    for (int i = 0; i < user_parameter_count; ++i) {
        UserParameter up;
        up.parameter_id = to_checked_int(reader.read_u32_le(), "user parameter ID");
        const int value_len = to_checked_int(reader.read_u32_le(), "user parameter value length");
        up.value = reader.read_bytes(static_cast<std::size_t>(value_len));
        obj.user_parameters.push_back(std::move(up));
    }
}

void read_transformable(BinaryReader &reader, Transformable &obj) {
    read_object3d(reader, obj);
    if (reader.read_bool_byte()) {
        ComponentTransform ct;
        ct.translation = read_float_array(reader, 3);
        ct.scale = read_float_array(reader, 3);
        ct.orientation_angle = reader.read_f32_le();
        ct.orientation_axis = read_float_array(reader, 3);
        obj.component_transform = ct;
    }
    if (reader.read_bool_byte()) {
        obj.general_transform = read_float_array(reader, 16);
    }
}

void read_node(BinaryReader &reader, Node &obj) {
    read_transformable(reader, obj);
    obj.enable_rendering = reader.read_bool_byte();
    obj.enable_picking = reader.read_bool_byte();
    obj.alpha_factor = reader.read_u8();
    obj.scope = reader.read_u32_le();
    if (reader.read_bool_byte()) {
        Alignment alignment;
        alignment.z_target = reader.read_u8();
        alignment.y_target = reader.read_u8();
        alignment.z_reference_id = read_object_ref_nullable(reader);
        alignment.y_reference_id = read_object_ref_nullable(reader);
        obj.alignment = alignment;
    }
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
    auto obj = std::make_shared<Header>();
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
    auto obj = std::make_shared<Group>();
    obj->object_id = object_id;
    obj->raw_length = raw_length;
    read_node(reader, *obj);
    const int child_count = to_checked_int(reader.read_u32_le(), "group child count");
    for (int i = 0; i < child_count; ++i) {
        obj->child_ids.push_back(read_object_ref(reader));
    }
    return obj;
}

std::shared_ptr<Object> parse_world(int object_id, int raw_length, BinaryReader &reader) {
    auto obj = std::make_shared<World>();
    obj->object_id = object_id;
    obj->raw_length = raw_length;
    read_node(reader, *obj);
    const int child_count = to_checked_int(reader.read_u32_le(), "world child count");
    for (int i = 0; i < child_count; ++i) {
        obj->child_ids.push_back(read_object_ref(reader));
    }
    obj->active_camera_id = read_object_ref_nullable(reader);
    obj->background_id = read_object_ref_nullable(reader);
    return obj;
}

std::shared_ptr<Object> parse_camera(int object_id, int raw_length, BinaryReader &reader) {
    auto obj = std::make_shared<Camera>();
    obj->object_id = object_id;
    obj->raw_length = raw_length;
    read_node(reader, *obj);
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
    auto obj = std::make_shared<Light>();
    obj->object_id = object_id;
    obj->raw_length = raw_length;
    read_node(reader, *obj);
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
    auto obj = std::make_shared<Background>();
    obj->object_id = object_id;
    obj->raw_length = raw_length;
    read_object3d(reader, *obj);
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
    /* Spec §11.7 Fog */
    auto obj = std::make_shared<Fog>();
    obj->object_id = object_id;
    obj->raw_length = raw_length;
    read_object3d(reader, *obj);
    obj->color = read_rgb(reader);
    obj->mode = reader.read_u8();
    if (obj->mode == Fog::EXPONENTIAL) {
        obj->density = reader.read_f32_le();
    } else if (obj->mode == Fog::LINEAR) {
        obj->near_distance = reader.read_f32_le();
        obj->far_distance = reader.read_f32_le();
    } else {
        throw std::runtime_error("Unsupported Fog mode " + std::to_string(obj->mode));
    }
    return obj;
}

std::shared_ptr<Object> parse_polygon_mode(int object_id, int raw_length, BinaryReader &reader) {
    auto obj = std::make_shared<PolygonMode>();
    obj->object_id = object_id;
    obj->raw_length = raw_length;
    read_object3d(reader, *obj);
    obj->culling = reader.read_u8();
    obj->shading = reader.read_u8();
    obj->winding = reader.read_u8();
    obj->two_sided_lighting_enabled = reader.read_bool_byte();
    obj->local_camera_lighting_enabled = reader.read_bool_byte();
    obj->perspective_correction_enabled = reader.read_bool_byte();
    return obj;
}

std::shared_ptr<Object> parse_material(int object_id, int raw_length, BinaryReader &reader) {
    auto obj = std::make_shared<Material>();
    obj->object_id = object_id;
    obj->raw_length = raw_length;
    read_object3d(reader, *obj);
    obj->ambient_color = read_rgb(reader);
    obj->diffuse_color = read_rgba(reader);
    obj->emissive_color = read_rgb(reader);
    obj->specular_color = read_rgb(reader);
    obj->shininess = reader.read_f32_le();
    obj->vertex_color_tracking_enabled = reader.read_bool_byte();
    return obj;
}

std::shared_ptr<Object> parse_vertex_array(int object_id, int raw_length, BinaryReader &reader) {
    auto obj = std::make_shared<VertexArray>();
    obj->object_id = object_id;
    obj->raw_length = raw_length;
    read_object3d(reader, *obj);
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
    auto obj = std::make_shared<VertexBuffer>();
    obj->object_id = object_id;
    obj->raw_length = raw_length;
    read_object3d(reader, *obj);
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
    auto obj = std::make_shared<TriangleStripArray>();
    obj->object_id = object_id;
    obj->raw_length = raw_length;
    read_object3d(reader, *obj);
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
    auto obj = std::make_shared<Appearance>();
    obj->object_id = object_id;
    obj->raw_length = raw_length;
    read_object3d(reader, *obj);
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
    auto obj = std::make_shared<Texture2D>();
    obj->object_id = object_id;
    obj->raw_length = raw_length;
    read_transformable(reader, *obj);
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
    auto obj = std::make_shared<Image2D>();
    obj->object_id = object_id;
    obj->raw_length = raw_length;
    read_object3d(reader, *obj);
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

void read_mesh_body(BinaryReader &reader, Mesh &obj) {
    read_node(reader, obj);
    obj.vertex_buffer_id = read_object_ref_nullable(reader);
    const int submesh_count = to_checked_int(reader.read_u32_le(), "submesh count");
    for (int i = 0; i < submesh_count; ++i) {
        SubmeshRef ref;
        ref.index_buffer_id = read_object_ref_nullable(reader);
        ref.appearance_id = read_object_ref_nullable(reader);
        obj.submeshes.push_back(ref);
    }
}

std::shared_ptr<Object> parse_mesh(int object_id, int raw_length, BinaryReader &reader) {
    auto obj = std::make_shared<Mesh>();
    obj->object_id = object_id;
    obj->raw_length = raw_length;
    read_mesh_body(reader, *obj);
    return obj;
}

std::shared_ptr<Object> parse_skinned_mesh(int object_id, int raw_length, BinaryReader &reader) {
    auto obj = std::make_shared<SkinnedMesh>();
    obj->object_id = object_id;
    obj->raw_length = raw_length;
    read_mesh_body(reader, *obj);
    obj->skeleton_id = read_object_ref_nullable(reader);
    const int transform_count = to_checked_int(reader.read_u32_le(), "bone transform count");
    for (int i = 0; i < transform_count; ++i) {
        BoneWeight bt;
        bt.transform_node_id = read_object_ref_nullable(reader);
        bt.first_vertex = to_checked_int(reader.read_u32_le(), "bone first vertex");
        bt.vertex_count = to_checked_int(reader.read_u32_le(), "bone vertex count");
        bt.weight = reader.read_i32_le();
        obj->bone_transforms.push_back(bt);
    }
    return obj;
}

std::shared_ptr<Object> parse_morphing_mesh(int object_id, int raw_length, BinaryReader &reader) {
    /* Spec §11.17: FOR each target { ObjectIndex morphTarget; Float32 initialWeight; } */
    auto obj = std::make_shared<MorphingMesh>();
    obj->object_id = object_id;
    obj->raw_length = raw_length;
    read_mesh_body(reader, *obj);
    const int target_count = to_checked_int(reader.read_u32_le(), "morph target count");
    obj->morph_target_ids.reserve(static_cast<std::size_t>(target_count));
    obj->morph_weights.reserve(static_cast<std::size_t>(target_count));
    for (int i = 0; i < target_count; ++i) {
        obj->morph_target_ids.push_back(read_object_ref(reader));
        obj->morph_weights.push_back(reader.read_f32_le());
    }
    return obj;
}

std::shared_ptr<Object> parse_sprite3d(int object_id, int raw_length, BinaryReader &reader) {
    auto obj = std::make_shared<Sprite3D>();
    obj->object_id = object_id;
    obj->raw_length = raw_length;
    read_node(reader, *obj);
    obj->image_id = read_object_ref_nullable(reader);
    obj->appearance_id = read_object_ref_nullable(reader);
    obj->is_scaled = reader.read_bool_byte();
    obj->crop_x = reader.read_i32_le();
    obj->crop_y = reader.read_i32_le();
    obj->crop_width = reader.read_i32_le();
    obj->crop_height = reader.read_i32_le();
    return obj;
}

std::shared_ptr<Object> parse_compositing_mode(int object_id, int raw_length, BinaryReader &reader) {
    auto obj = std::make_shared<CompositingMode>();
    obj->object_id = object_id;
    obj->raw_length = raw_length;
    read_object3d(reader, *obj);
    obj->depth_test_enabled = reader.read_bool_byte();
    obj->depth_write_enabled = reader.read_bool_byte();
    obj->color_write_enabled = reader.read_bool_byte();
    obj->alpha_write_enabled = reader.read_bool_byte();
    obj->blending = reader.read_u8();
    obj->alpha_threshold = reader.read_u8();
    obj->depth_offset_factor = reader.read_f32_le();
    obj->depth_offset_units = reader.read_f32_le();
    return obj;
}

std::shared_ptr<Object> parse_animation_controller(int object_id, int raw_length, BinaryReader &reader) {
    auto obj = std::make_shared<AnimationController>();
    obj->object_id = object_id;
    obj->raw_length = raw_length;
    read_object3d(reader, *obj);
    obj->speed = reader.read_f32_le();
    obj->weight = reader.read_f32_le();
    obj->active_interval_start = reader.read_i32_le();
    obj->active_interval_end = reader.read_i32_le();
    obj->reference_sequence_time = reader.read_f32_le();
    obj->reference_world_time = reader.read_i32_le();
    return obj;
}

std::shared_ptr<Object> parse_animation_track(int object_id, int raw_length, BinaryReader &reader) {
    auto obj = std::make_shared<AnimationTrack>();
    obj->object_id = object_id;
    obj->raw_length = raw_length;
    read_object3d(reader, *obj);
    obj->keyframe_sequence_id = read_object_ref_nullable(reader);
    obj->animation_controller_id = read_object_ref_nullable(reader);
    obj->property_id = to_checked_int(reader.read_u32_le(), "animation property id");
    return obj;
}

std::shared_ptr<Object> parse_keyframe_sequence(int object_id, int raw_length, BinaryReader &reader) {
    auto obj = std::make_shared<KeyframeSequence>();
    obj->object_id = object_id;
    obj->raw_length = raw_length;
    read_object3d(reader, *obj);
    obj->interpolation = reader.read_u8();
    obj->repeat_mode = reader.read_u8();
    obj->encoding = reader.read_u8();
    obj->duration = to_checked_int(reader.read_u32_le(), "sequence duration");
    obj->valid_range_first = to_checked_int(reader.read_u32_le(), "sequence validRangeFirst");
    obj->valid_range_last = to_checked_int(reader.read_u32_le(), "sequence validRangeLast");
    obj->component_count = to_checked_int(reader.read_u32_le(), "sequence componentCount");
    const int keyframe_count = to_checked_int(reader.read_u32_le(), "sequence keyframeCount");
    /* Spec §11.12 KeyframeSequence encodings 0 / 1 / 2 */
    if (obj->encoding == 0) {
        for (int i = 0; i < keyframe_count; ++i) {
            Keyframe kf;
            kf.time = to_checked_int(reader.read_u32_le(), "keyframe time");
            kf.values = read_float_array(reader, obj->component_count);
            obj->keyframes.push_back(std::move(kf));
        }
    } else if (obj->encoding == 1 || obj->encoding == 2) {
        /* Float32[componentCount] bias and scale (not a single scale). */
        const auto bias = read_float_array(reader, obj->component_count);
        const auto scale = read_float_array(reader, obj->component_count);
        const float quant_max = obj->encoding == 1 ? 255.f : 65535.f;
        for (int i = 0; i < keyframe_count; ++i) {
            Keyframe kf;
            kf.time = to_checked_int(reader.read_u32_le(), "keyframe time");
            kf.values.resize(static_cast<std::size_t>(obj->component_count));
            for (int c = 0; c < obj->component_count; ++c) {
                const float q = obj->encoding == 1 ? static_cast<float>(reader.read_u8())
                                                   : static_cast<float>(reader.read_u16_le());
                kf.values[static_cast<std::size_t>(c)] =
                    (q / quant_max) * scale[static_cast<std::size_t>(c)] +
                    bias[static_cast<std::size_t>(c)];
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
        auto obj = std::make_shared<ExternalReference>();
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
    case ObjectTypes::COMPOSITING_MODE:
        return parse_compositing_mode(object_id, raw_length, reader);
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
    case ObjectTypes::MORPHING_MESH:
        return parse_morphing_mesh(object_id, raw_length, reader);
    case ObjectTypes::SPRITE_3D:
        return parse_sprite3d(object_id, raw_length, reader);
    case ObjectTypes::ANIMATION_CONTROLLER:
        return parse_animation_controller(object_id, raw_length, reader);
    case ObjectTypes::ANIMATION_TRACK:
        return parse_animation_track(object_id, raw_length, reader);
    case ObjectTypes::KEYFRAME_SEQUENCE:
        return parse_keyframe_sequence(object_id, raw_length, reader);
    default: {
        auto obj = std::make_shared<Unknown>();
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
            if (!std::dynamic_pointer_cast<Unknown>(parsed)) {
                payload_reader.ensure_fully_consumed("Object " + std::to_string(parsed->object_id) + " (" +
                                                     parsed->type_name() + ")");
            }
            file.objects_by_id[next_object_id] = parsed;
            ++next_object_id;
        }
    }

    for (const auto &kv : file.objects_by_id) {
        if (auto h = std::dynamic_pointer_cast<Header>(kv.second)) {
            file.header = h;
            break;
        }
    }
    return file;
}

} // namespace
} // namespace model
} // namespace m3g

/* internal: Scene builder */


#include <string>

namespace m3g {
namespace scene {
namespace { // private

class SceneBuilder {
public:
    SceneBuilder(const model::File &file, std::string input_path);

    SceneIr build();

private:
    const model::File &file_;
    std::string input_path_;
};

} // namespace
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
namespace { // private

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
            if (std::dynamic_pointer_cast<model::AnimationController>(obj) ||
                std::dynamic_pointer_cast<model::AnimationTrack>(obj) ||
                std::dynamic_pointer_cast<model::KeyframeSequence>(obj)) {
                has_anim = true;
            }
            if (std::dynamic_pointer_cast<model::MorphingMesh>(obj)) {
                has_morph = true;
            }
            if (auto u = std::dynamic_pointer_cast<model::Unknown>(obj)) {
                has_unknown = true;
                if (!unknown_types.empty()) {
                    unknown_types += ", ";
                }
                unknown_types += std::to_string(u->object_type) + " (" + u->type_name() + ")";
            }
            if (std::dynamic_pointer_cast<model::Light>(obj)) {
                has_light = true;
            }
            if (std::dynamic_pointer_cast<model::Fog>(obj)) {
                has_fog = true;
            }
            if (auto bg = std::dynamic_pointer_cast<model::Background>(obj)) {
                if (bg->background_image_id) {
                    has_bg_img = true;
                }
            }
            if (auto sk = std::dynamic_pointer_cast<model::SkinnedMesh>(obj)) {
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
            if (std::dynamic_pointer_cast<model::Node>(obj)) {
                node_ids.push_back(obj->object_id);
            }
        }
        std::sort(node_ids.begin(), node_ids.end());
        if (node_ids.empty()) {
            return {};
        }

        std::set<int> child_reference_ids;
        for (const auto &obj : file_.objects_in_order()) {
            if (auto group = std::dynamic_pointer_cast<model::Group>(obj)) {
                for (int id : group->child_ids) {
                    child_reference_ids.insert(id);
                }
                if (auto world = std::dynamic_pointer_cast<model::World>(obj)) {
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
        if (auto o3d = dynamic_cast<const model::Object3D *>(&obj)) {
            suffix = o3d->getUserID();
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
        if (auto world = std::dynamic_pointer_cast<model::World>(obj)) {
            return build_group_node(*world, world->active_camera_id);
        }
        if (auto group = std::dynamic_pointer_cast<model::Group>(obj)) {
            return build_group_node(*group, std::nullopt);
        }
        if (auto mesh = std::dynamic_pointer_cast<model::Mesh>(obj)) {
            return build_mesh_node(*mesh);
        }
        if (auto cam = std::dynamic_pointer_cast<model::Camera>(obj)) {
            return build_camera_node(*cam);
        }
        if (auto light = std::dynamic_pointer_cast<model::Light>(obj)) {
            return build_light_node(*light);
        }
        if (auto sprite = std::dynamic_pointer_cast<model::Sprite3D>(obj)) {
            warn("sprite3d-" + std::to_string(sprite->object_id),
                 "Sprite3D is retained in the object graph but not exported to glTF in v1.");
            return register_node(sprite->object_id, *sprite, std::nullopt, std::nullopt);
        }
        warn("unsupported-node-" + std::to_string(obj->object_id),
             "Unsupported node object " + obj->type_name() + " (" + std::to_string(obj->object_id) +
                 ") was skipped.");
        return std::nullopt;
    }

    int build_group_node(model::Group &group, std::optional<int> additional_child_id) {
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

    int build_mesh_node(model::Mesh &mesh_object) {
        const int mesh_index = build_mesh(mesh_object.object_id, mesh_object);
        return register_node(mesh_object.object_id, mesh_object, mesh_index, std::nullopt);
    }

    int build_camera_node(model::Camera &camera_object) {
        auto camera_index = build_camera(camera_object);
        return register_node(camera_object.object_id, camera_object, std::nullopt, camera_index);
    }

    int build_light_node(model::Light &light_object) {
        return register_node(light_object.object_id, light_object, std::nullopt, std::nullopt);
    }

    int register_node(int object_id, model::Node &obj, std::optional<int> mesh_index,
                      std::optional<int> camera_index) {
        auto it = node_index_by_object_id_.find(object_id);
        if (it != node_index_by_object_id_.end()) {
            return it->second;
        }
        if (obj.alignment) {
            warn("alignment", "Node alignment is present but not exported in v1.");
        }
        auto matrix = node_matrix_row_major(obj);
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

    static float sequence_time_to_seconds(int sequence_time, const model::AnimationController *controller) {
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

    bool convert_keyframes(const model::KeyframeSequence &seq, const model::AnimationController *controller,
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
            auto node = std::dynamic_pointer_cast<model::Node>(found->second);
            if (!node) {
                continue;
            }
            for (int track_id : node->animation_track_ids) {
                any_track = true;
                auto track = std::dynamic_pointer_cast<model::AnimationTrack>(file_.object_or_null(track_id));
                if (!track) {
                    warn("animation-track", "Animation track reference is missing or invalid.");
                    continue;
                }
                auto seq = std::dynamic_pointer_cast<model::KeyframeSequence>(
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
                const model::AnimationController *controller = nullptr;
                int controller_id = -1;
                if (track->animation_controller_id) {
                    controller_id = *track->animation_controller_id;
                    if (auto c = std::dynamic_pointer_cast<model::AnimationController>(
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

    std::optional<int> build_camera(model::Camera &camera_object) {
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

    int build_mesh(int object_id, model::Mesh &mesh_object) {
        auto it = mesh_index_by_object_id_.find(object_id);
        if (it != mesh_index_by_object_id_.end()) {
            return it->second;
        }
        auto vertex_buffer =
            std::dynamic_pointer_cast<model::VertexBuffer>(file_.object_or_null(mesh_object.vertex_buffer_id));
        if (!vertex_buffer) {
            throw std::runtime_error("Mesh references missing vertex buffer");
        }
        auto positions_array =
            std::dynamic_pointer_cast<model::VertexArray>(file_.object_or_null(vertex_buffer->positions_id));
        if (!positions_array) {
            throw std::runtime_error("Mesh references missing positions array");
        }
        auto positions =
            decode_scaled_array(*positions_array, vertex_buffer->position_scale, vertex_buffer->position_bias, 3);

        std::optional<std::vector<float>> normals;
        if (auto n = std::dynamic_pointer_cast<model::VertexArray>(file_.object_or_null(vertex_buffer->normals_id))) {
            normals = decode_normals(*n);
        }
        std::optional<std::vector<float>> vertex_colors;
        if (auto c = std::dynamic_pointer_cast<model::VertexArray>(file_.object_or_null(vertex_buffer->colors_id))) {
            vertex_colors = decode_vertex_colors(*c);
        }
        std::optional<std::vector<float>> tex_coords0;
        if (!vertex_buffer->tex_coord_bindings.empty()) {
            const auto &binding = vertex_buffer->tex_coord_bindings.front();
            auto vertex_array =
                std::dynamic_pointer_cast<model::VertexArray>(file_.object_or_null(binding.vertex_array_id));
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
            auto index_buffer = std::dynamic_pointer_cast<model::TriangleStripArray>(
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
            std::dynamic_pointer_cast<model::Appearance>(file_.object_or_null(appearance_id));
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
            std::dynamic_pointer_cast<model::Material>(file_.object_or_null(appearance->material_id));
        auto polygon_mode =
            std::dynamic_pointer_cast<model::PolygonMode>(file_.object_or_null(appearance->polygon_mode_id));
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
        if (appearance->user_id != 0) {
            material.name = "u" + std::to_string(appearance->user_id);
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
        auto texture = std::dynamic_pointer_cast<model::Texture2D>(file_.object_or_null(texture_id));
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
        if (auto image = std::dynamic_pointer_cast<model::Image2D>(image_object)) {
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
        } else if (auto ext = std::dynamic_pointer_cast<model::ExternalReference>(image_object)) {
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

    std::optional<std::vector<std::uint8_t>> decode_embedded_image_to_rgba(const model::Image2D &image) {
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

    static std::vector<float> decode_vertex_colors(const model::VertexArray &array) {
        if (array.component_count != 3 && array.component_count != 4) {
            throw std::runtime_error("Vertex color array component count invalid");
        }
        std::vector<float> values(static_cast<std::size_t>(array.vertex_count * array.component_count));
        for (std::size_t i = 0; i < values.size(); ++i) {
            values[i] = (array.components[i] & 0xFF) / 255.f;
        }
        return values;
    }

    static std::vector<float> decode_scaled_array(const model::VertexArray &array, float scale,
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

    static std::vector<float> decode_normals(const model::VertexArray &array) {
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

    static std::vector<float> decode_tex_coords(const model::VertexArray &array,
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

    static std::vector<int> expand_triangle_strips(const model::TriangleStripArray &index_buffer) {
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

} // namespace
} // namespace scene
} // namespace m3g

/* internal: Pattern texture attacher */


#include <optional>
#include <string>

namespace m3g {
namespace scene {
namespace { // private

struct PatternTextureAttacher {
    static SceneIr auto_attach(const SceneIr &scene, const std::optional<std::string> &pattern_path);
};

} // namespace
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
namespace { // private

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

} // namespace
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
#endif /* DOXYGEN */
