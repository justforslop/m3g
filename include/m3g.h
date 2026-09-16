/**
 * @file m3g.h
 * @brief JSR-184 / M3G decode + scene IR (portable C99 single-header).
 *
 * @mainpage m3g C API
 *
 * This header is the portable **C99** public API for parsing M3G files and
 * building an export-oriented scene IR. The optional C++ API lives in
 * `m3g.hpp` (glTF export / legacy types).
 *
 * ## Integration (stb-style)
 *
 * In **one** translation unit:
 * @code
 * #define M3G_IMPLEMENTATION
 * #include "m3g.h"
 * @endcode
 *
 * Everywhere else:
 * @code
 * #include "m3g.h"
 * @endcode
 *
 * ## Backends
 *
 * Deflate, image, and math are **not** compiled into this header. Install
 * callbacks (or link the adapter TUs) before decode:
 *
 * - @ref m3g_c_io — @ref m3g_deflate_io, @ref m3g_image_io, @ref m3g_math_io
 * - @ref m3g_install_miniz_deflate_io / @ref m3g_install_stb_image_io
 * - @ref m3g_install_portable_math_io (default) or @ref m3g_install_vecmath_math_io
 *
 * ## Decode
 *
 * @code
 * m3g_decoded d;
 * m3g_install_miniz_deflate_io();
 * if (m3g_decode_file("model.m3g", NULL, &d) == M3G_OK) {
 *     // d.file, d.scene
 *     m3g_decoded_free(&d);
 * }
 * @endcode
 *
 * Errors return a non-zero @ref m3g_result; details via @ref m3g_last_error.
 *
 * @defgroup m3g_c_version Version
 * @brief Library version macros.
 *
 * @defgroup m3g_c_error Errors
 * @brief Result codes and last-error string.
 *
 * @defgroup m3g_c_io Backend I/O and math
 * @brief Pluggable deflate, image, and vector/matrix backends.
 *
 * @defgroup m3g_c_model M3G object model
 * @brief Parsed JSR-184 object graph (`m3g_file` / `m3g_object`).
 *
 * @defgroup m3g_c_scene Scene IR
 * @brief Export-oriented intermediate representation (`m3g_scene_ir`).
 *
 * @defgroup m3g_c_decode Decode API
 * @brief Path/bytes → @ref m3g_decoded.
 *
 * @defgroup m3g_c_math Matrix helpers
 * @brief Row-major 4×4 helpers (via @ref m3g_math_io).
 */

#ifndef M3G_H_INCLUDED
#define M3G_H_INCLUDED

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef M3G_API
/** @brief Public API linkage prefix (override for DLL export/import). */
#define M3G_API
#endif

/**
 * @addtogroup m3g_c_version
 * @{
 */
/** @brief Library major version. */
#define M3G_VERSION_MAJOR 0
/** @brief Library minor version. */
#define M3G_VERSION_MINOR 2
/** @brief Library patch version. */
#define M3G_VERSION_PATCH 0
/** @brief Byte length of the M3G file identifier (`\\xABJSRI184\\xBB\\r\\n\\x1A\\n`). */
#define M3G_IDENTIFIER_LEN 12
/** @} */

/**
 * @addtogroup m3g_c_error
 * @{
 */
/** @brief Result codes for decode and helpers (`0` = success). */
enum {
    M3G_OK = 0,           /**< @brief Success. */
    M3G_ERR = -1,         /**< @brief Generic failure. */
    M3G_ERR_IO = -2,      /**< @brief Filesystem / read failure. */
    M3G_ERR_FORMAT = -3,  /**< @brief Invalid or unsupported M3G data. */
    M3G_ERR_NOMEM = -4,   /**< @brief Allocation failure. */
    M3G_ERR_BACKEND = -5, /**< @brief Required I/O backend missing or failed. */
    M3G_ERR_RANGE = -6    /**< @brief Integer / size out of range. */
};

/** @brief Function result type (`M3G_OK` or negative @c M3G_ERR_*). */
typedef int m3g_result;

/**
 * @brief Lightweight error descriptor (reserved / diagnostic).
 * Prefer @ref m3g_last_error for human-readable messages after a failed call.
 */
typedef struct m3g_error {
    const char *message; /**< @brief NUL-terminated message, or NULL. */
    size_t offset;       /**< @brief Byte offset in the input stream, if known. */
} m3g_error;

/**
 * @brief Thread-local-ish last error string from the implementation unit.
 * @return Never NULL; empty string if no error was recorded.
 */
M3G_API char const *m3g_last_error(void);
/** @} */

/**
 * @addtogroup m3g_c_io
 * @{
 */

/** @brief Success code for deflate @c uncompress (matches zlib/miniz `Z_OK`). */
enum { M3G_DEFLATE_OK = 0 };
/** @brief Initial Adler-32 seed (pass as @p adler when starting a new checksum). */
enum { M3G_ADLER32_INIT = 1 };

/**
 * @brief Zlib / deflate function table (miniz-shaped).
 *
 * Install with @ref m3g_set_deflate_io or @ref m3g_install_miniz_deflate_io.
 * Required for compressed M3G sections and some embedded images.
 */
typedef struct m3g_deflate_io {
    /**
     * @brief Running Adler-32.
     * @param adler Previous sum, or @c M3G_ADLER32_INIT to start.
     * @param ptr Data; if NULL, returns the initial seed for @p adler.
     * @param buf_len Byte count when @p ptr is non-NULL.
     * @param user Opaque @c user from this struct.
     */
    uint32_t (*adler32)(uint32_t adler, unsigned char const *ptr, size_t buf_len, void *user);
    /**
     * @brief Inflate zlib-wrapped deflate into a caller-owned buffer.
     * @param[in,out] dest_len On entry: capacity of @p dest; on success: bytes written.
     * @return @c M3G_DEFLATE_OK on success; non-zero on failure.
     */
    int (*uncompress)(unsigned char *dest, size_t *dest_len, unsigned char const *source, size_t source_len,
                      void *user);
    /**
     * @brief Inflate when the output size is unknown.
     * @param zlib_header Non-zero = zlib wrapper; `0` = raw deflate.
     * @param[out] out_len Size of returned buffer.
     * @return Heap block (free with @ref free_mem), or NULL on failure.
     */
    void *(*uncompress_to_heap)(unsigned char const *source, size_t source_len, size_t *out_len, int zlib_header,
                                void *user);
    /** @brief Free a pointer from @c uncompress_to_heap (NULL-safe). */
    void (*free_mem)(void *p, void *user);
    /** @brief Opaque pointer passed to every callback. */
    void *user;
} m3g_deflate_io;

/**
 * @brief Raster image load table (stb_image-shaped).
 *
 * @p req_comp: `0` = source layout; `4` = force RGBA8.
 * On success returns pixels owned until @c free_pixels.
 */
typedef struct m3g_image_io {
    /** @brief Load from filesystem path (like `stbi_load`). */
    unsigned char *(*load_file)(char const *filename, int *width, int *height, int *channels_in_file, int req_comp,
                                void *user);
    /** @brief Load from memory (like `stbi_load_from_memory`). */
    unsigned char *(*load_memory)(unsigned char const *buffer, int len, int *width, int *height, int *channels_in_file,
                                  int req_comp, void *user);
    /** @brief Free pixels from load_* (NULL-safe). */
    void (*free_pixels)(void *pixels, void *user);
    /** @brief Opaque pointer passed to every callback. */
    void *user;
} m3g_image_io;

/**
 * @brief Vector / matrix backend (row-major 4×4).
 *
 * Translation lives in @c m[3], @c m[7], @c m[11] (M3G / glTF intermediate).
 * All callbacks receive the opaque @c user pointer from this struct.
 */
typedef struct m3g_math_io {
    float (*vec3_length)(float x, float y, float z, void *user);
    void (*mat4_identity)(float out[16], void *user);
    void (*mat4_mul)(float const a[16], float const b[16], float out[16], void *user);
    int (*mat4_is_identity)(float const m[16], float epsilon, void *user);
    void (*mat4_translation)(float x, float y, float z, float out[16], void *user);
    void (*mat4_scale)(float x, float y, float z, float out[16], void *user);
    /**
     * @brief Axis-angle rotation matrix.
     * @param angle_rad Angle in radians; axis need not be unit length.
     */
    void (*mat4_rotation_axis)(float angle_rad, float ax, float ay, float az, float out[16], void *user);
    /**
     * @brief Decompose TRS from a row-major matrix.
     * @param[out] q_xyzw Unit quaternion as xyzw.
     */
    void (*decompose_trs)(float const m[16], float t[3], float q_xyzw[4], float s[3], void *user);
    void (*quat_from_axis_angle_deg)(float angle_deg, float ax, float ay, float az, float q_xyzw[4], void *user);
    void *user;
} m3g_math_io;

/**
 * @brief Install process-global deflate backend (copied by value).
 * @param io New table, or NULL to clear.
 */
M3G_API void m3g_set_deflate_io(m3g_deflate_io const *io);
/** @brief Current deflate backend, or NULL if unset. */
M3G_API m3g_deflate_io const *m3g_get_deflate_io(void);
/**
 * @brief Install process-global image backend.
 * @param io New table, or NULL to clear.
 */
M3G_API void m3g_set_image_io(m3g_image_io const *io);
/** @brief Current image backend, or NULL if unset. */
M3G_API m3g_image_io const *m3g_get_image_io(void);
/**
 * @brief Install process-global math backend.
 * @param io New table, or NULL to restore @ref m3g_install_portable_math_io.
 */
M3G_API void m3g_set_math_io(m3g_math_io const *io);
/** @brief Current math backend (never NULL after first use; portable default). */
M3G_API m3g_math_io const *m3g_get_math_io(void);

/** @brief Portable `math.h` backend (default). */
M3G_API void m3g_install_portable_math_io(void);
/** @brief `vecmath.h` backend — defined in `src/math_io_vecmath.c` when linked. */
M3G_API void m3g_install_vecmath_math_io(void);
/** @brief miniz deflate adapter — `src/deflate_io_miniz.c`. */
M3G_API void m3g_install_miniz_deflate_io(void);
/** @brief stb_image adapter — `src/image_io_stb.c`. */
M3G_API void m3g_install_stb_image_io(void);

/** @name Deflate / image helpers
 *  Forward to the installed backends; set last-error on missing callbacks.
 *  @{
 */
M3G_API uint32_t m3g_deflate_adler32(uint32_t adler, unsigned char const *ptr, size_t buf_len);
M3G_API int m3g_deflate_uncompress(unsigned char *dest, size_t *dest_len, unsigned char const *source,
                                   size_t source_len);
M3G_API void *m3g_deflate_uncompress_to_heap(unsigned char const *source, size_t source_len, size_t *out_len,
                                             int zlib_header);
M3G_API void m3g_deflate_free(void *p);
M3G_API unsigned char *m3g_image_load_memory(unsigned char const *buffer, int len, int *width, int *height,
                                             int *channels_in_file, int req_comp);
M3G_API void m3g_image_free_pixels(void *pixels);
/** @} */
/** @} */ /* m3g_c_io */

/**
 * @addtogroup m3g_c_model
 * @{
 */

/** @brief M3G object type byte values (JSR-184). */
enum {
    M3G_OBJ_HEADER = 0,
    M3G_OBJ_ANIMATION_CONTROLLER = 1,
    M3G_OBJ_ANIMATION_TRACK = 2,
    M3G_OBJ_APPEARANCE = 3,
    M3G_OBJ_BACKGROUND = 4,
    M3G_OBJ_CAMERA = 5,
    M3G_OBJ_COMPOSITING_MODE = 6,
    M3G_OBJ_FOG = 7,
    M3G_OBJ_POLYGON_MODE = 8,
    M3G_OBJ_GROUP = 9,
    M3G_OBJ_IMAGE_2D = 10,
    M3G_OBJ_TRIANGLE_STRIP_ARRAY = 11,
    M3G_OBJ_LIGHT = 12,
    M3G_OBJ_MATERIAL = 13,
    M3G_OBJ_MESH = 14,
    M3G_OBJ_MORPHING_MESH = 15,
    M3G_OBJ_SKINNED_MESH = 16,
    M3G_OBJ_TEXTURE_2D = 17,
    M3G_OBJ_SPRITE_3D = 18,
    M3G_OBJ_KEYFRAME_SEQUENCE = 19,
    M3G_OBJ_VERTEX_ARRAY = 20,
    M3G_OBJ_VERTEX_BUFFER = 21,
    M3G_OBJ_WORLD = 22,
    M3G_OBJ_EXTERNAL_REFERENCE = 0xFF
};

/** @brief Animation track target property IDs. */
enum {
    M3G_ANIM_ALPHA = 256,
    M3G_ANIM_AMBIENT_COLOR = 257,
    M3G_ANIM_COLOR = 258,
    M3G_ANIM_CROP = 259,
    M3G_ANIM_DENSITY = 260,
    M3G_ANIM_DIFFUSE_COLOR = 261,
    M3G_ANIM_EMISSIVE_COLOR = 262,
    M3G_ANIM_FAR_DISTANCE = 263,
    M3G_ANIM_FIELD_OF_VIEW = 264,
    M3G_ANIM_INTENSITY = 265,
    M3G_ANIM_MORPH_WEIGHTS = 266,
    M3G_ANIM_NEAR_DISTANCE = 267,
    M3G_ANIM_ORIENTATION = 268,
    M3G_ANIM_PICKABILITY = 269,
    M3G_ANIM_SCALE = 270,
    M3G_ANIM_SHININESS = 271,
    M3G_ANIM_SPECULAR_COLOR = 272,
    M3G_ANIM_SPOT_ANGLE = 273,
    M3G_ANIM_SPOT_EXPONENT = 274,
    M3G_ANIM_TRANSLATION = 275,
    M3G_ANIM_VISIBILITY = 276
};

/** @brief KeyframeSequence interpolation constants. */
enum {
    M3G_KF_LINEAR = 176,
    M3G_KF_SLERP = 177,
    M3G_KF_SPLINE = 178,
    M3G_KF_SQUAD = 179,
    M3G_KF_STEP = 180
};

/**
 * @brief Human-readable name for an @c M3G_OBJ_* value.
 * @return Static string (do not free).
 */
M3G_API char const *m3g_type_name(int object_type);

/** @brief 8-bit RGB color. */
typedef struct m3g_rgb {
    int red;   /**< @brief 0–255. */
    int green; /**< @brief 0–255. */
    int blue;  /**< @brief 0–255. */
} m3g_rgb;

/** @brief 8-bit RGBA color. */
typedef struct m3g_rgba {
    int red;
    int green;
    int blue;
    int alpha; /**< @brief 0–255; default often 255. */
} m3g_rgba;

/** @brief One compressed/uncompressed section header from the file. */
typedef struct m3g_section_info {
    int index;                 /**< @brief Section order index. */
    int compression_scheme;    /**< @brief `0` none, `1` zlib. */
    int total_section_length;  /**< @brief Including header + checksum. */
    int uncompressed_length;   /**< @brief Payload size after inflate. */
} m3g_section_info;

/** @brief One Object3D user-parameter entry (spec §11.19 Hashtable entry). */
typedef struct m3g_user_parameter {
    int parameter_id;
    uint8_t *value;
    int value_len;
} m3g_user_parameter;

/** @brief Common Object3D user id / animation track list / user parameters. */
typedef struct m3g_object3d_meta {
    int user_id;
    int *animation_track_ids; /**< @brief Heap array; length @c animation_track_count. */
    int animation_track_count;
    m3g_user_parameter *user_parameters; /**< @brief Heap; length @c user_parameter_count. */
    int user_parameter_count;
} m3g_object3d_meta;

/**
 * @brief TRS component transform (translation, orientation, scale).
 * @note @c orientation_angle is in **radians** (JSR-184 ComponentTransform).
 */
typedef struct m3g_component_transform {
    float translation[3];
    float scale[3];
    float orientation_angle; /**< @brief Radians. */
    float orientation_axis[3];
    int present; /**< @brief Non-zero if this block was present in the file. */
} m3g_component_transform;

/** @brief Object3D meta plus optional component or matrix transform. */
typedef struct m3g_transformable_meta {
    m3g_object3d_meta object3d;
    m3g_component_transform component;
    float general_transform[16]; /**< @brief Row-major 4×4 when @c has_general_transform is set. */
    int has_general_transform;
} m3g_transformable_meta;

/** @brief Node alignment targets and references. */
typedef struct m3g_alignment {
    int z_target;
    int y_target;
    int z_reference_id; /**< @brief `0` = none. */
    int y_reference_id; /**< @brief `0` = none. */
    int present;
} m3g_alignment;

/** @brief Node rendering/picking flags and transformable meta. */
typedef struct m3g_node_meta {
    m3g_transformable_meta transformable;
    int enable_rendering;
    int enable_picking;
    int alpha_factor; /**< @brief 0–255. */
    uint32_t scope;
    m3g_alignment alignment;
} m3g_node_meta;

/** @brief Texture coordinate stream binding. */
typedef struct m3g_tex_coord_binding {
    int vertex_array_id; /**< @brief `0` = none. */
    float bias[3];
    float scale;
} m3g_tex_coord_binding;

/** @brief Index buffer + appearance reference for a submesh. */
typedef struct m3g_submesh_ref {
    int index_buffer_id; /**< @brief `0` = none. */
    int appearance_id;   /**< @brief `0` = none. */
} m3g_submesh_ref;

/** @brief Bone influence range on a skinned mesh. */
typedef struct m3g_bone_transform {
    int transform_node_id;
    int first_vertex;
    int vertex_count;
    int weight;
} m3g_bone_transform;

/** @brief One keyframe time + value vector. */
typedef struct m3g_keyframe {
    int time;
    float *values;
    int value_count;
} m3g_keyframe;

/** @brief M3G file header object payload. */
typedef struct m3g_header_data {
    int version_major;
    int version_minor;
    int has_external_references;
    uint32_t total_file_size;
    uint32_t approximate_content_size;
    char *authoring_field; /**< @brief Heap string. */
} m3g_header_data;

/** @brief External reference URI object. */
typedef struct m3g_external_ref_data {
    char *uri; /**< @brief Heap string. */
} m3g_external_ref_data;

/** @brief Group or World node. */
typedef struct m3g_group_data {
    m3g_node_meta node;
    int *child_ids;
    int child_count;
    int active_camera_id; /**< @brief World only; `0` = none. */
    int background_id;    /**< @brief World only; `0` = none. */
    int is_world;         /**< @brief Non-zero if object type is World. */
} m3g_group_data;

/** @brief Camera node. */
typedef struct m3g_camera_data {
    m3g_node_meta node;
    int projection_type;
    float fov_degrees;
    float aspect_ratio;
    float near_distance;
    float far_distance;
    float generic_values[16];
    int has_perspective; /**< @brief Perspective parameters valid. */
    int has_generic16;   /**< @brief @c generic_values holds a 4×4. */
} m3g_camera_data;

/** @brief Light node. */
typedef struct m3g_light_data {
    m3g_node_meta node;
    float attenuation_constant;
    float attenuation_linear;
    float attenuation_quadratic;
    m3g_rgb color;
    int mode;
    float intensity;
    float spot_angle;
    float spot_exponent;
} m3g_light_data;

/** @brief Background object. */
typedef struct m3g_background_data {
    m3g_object3d_meta object3d;
    m3g_rgba background_color;
    int background_image_id;
    int image_mode_x;
    int image_mode_y;
    int crop_x, crop_y, crop_width, crop_height;
    int depth_clear_enabled;
    int color_clear_enabled;
} m3g_background_data;

/** @brief Fog object. */
typedef struct m3g_fog_data {
    m3g_object3d_meta object3d;
    m3g_rgb color;
    int mode;
    float density;
    float near_distance;
    float far_distance;
} m3g_fog_data;

/** @brief PolygonMode object. */
typedef struct m3g_polygon_mode_data {
    m3g_object3d_meta object3d;
    int culling;
    int shading;
    int winding;
    int two_sided_lighting_enabled;
    int local_camera_lighting_enabled;
    int perspective_correction_enabled;
} m3g_polygon_mode_data;

/** @brief Material object. */
typedef struct m3g_material_data {
    m3g_object3d_meta object3d;
    m3g_rgb ambient_color;
    m3g_rgba diffuse_color;
    m3g_rgb emissive_color;
    m3g_rgb specular_color;
    float shininess;
    int vertex_color_tracking_enabled;
} m3g_material_data;

/** @brief VertexArray object. */
typedef struct m3g_vertex_array_data {
    m3g_object3d_meta object3d;
    int component_size;
    int component_count;
    int encoding;
    int vertex_count;
    int *components;     /**< @brief Flattened signed components. */
    int component_total; /**< @brief @c vertex_count * component_count. */
} m3g_vertex_array_data;

/** @brief VertexBuffer object. */
typedef struct m3g_vertex_buffer_data {
    m3g_object3d_meta object3d;
    m3g_rgba default_color;
    int positions_id;
    int normals_id;
    int colors_id;
    float position_bias[3];
    float position_scale;
    m3g_tex_coord_binding *tex_coord_bindings;
    int tex_coord_binding_count;
} m3g_vertex_buffer_data;

/** @brief TriangleStripArray index buffer. */
typedef struct m3g_triangle_strip_data {
    m3g_object3d_meta object3d;
    int encoding;
    int *indices;
    int index_count;
    int *strip_lengths;
    int strip_count;
} m3g_triangle_strip_data;

/** @brief Appearance object. */
typedef struct m3g_appearance_data {
    m3g_object3d_meta object3d;
    int layer;
    int compositing_mode_id;
    int fog_id;
    int polygon_mode_id;
    int material_id;
    int *texture_ids;
    int texture_count;
} m3g_appearance_data;

/** @brief Texture2D object. */
typedef struct m3g_texture2d_data {
    m3g_transformable_meta transformable;
    int image_id;
    m3g_rgb blend_color;
    int blending;
    int wrapping_s;
    int wrapping_t;
    int level_filter;
    int image_filter;
} m3g_texture2d_data;

/** @brief Image2D object. */
typedef struct m3g_image2d_data {
    m3g_object3d_meta object3d;
    int format;
    int is_mutable;
    int width;
    int height;
    uint8_t *palette;
    int palette_len;
    uint8_t *pixels;
    int pixels_len;
    int has_pixels;
} m3g_image2d_data;

/** @brief Mesh or SkinnedMesh node. */
typedef struct m3g_mesh_data {
    m3g_node_meta node;
    int vertex_buffer_id;
    m3g_submesh_ref *submeshes;
    int submesh_count;
    int skeleton_id; /**< @brief Skinned only. */
    m3g_bone_transform *bone_transforms;
    int bone_transform_count;
    int is_skinned;
} m3g_mesh_data;

/** @brief AnimationController object. */
typedef struct m3g_anim_controller_data {
    m3g_object3d_meta object3d;
    float speed;
    float weight;
    int active_interval_start;
    int active_interval_end;
    float reference_sequence_time;
    int reference_world_time;
} m3g_anim_controller_data;

/** @brief AnimationTrack object. */
typedef struct m3g_anim_track_data {
    m3g_object3d_meta object3d;
    int keyframe_sequence_id;
    int animation_controller_id;
    int property_id; /**< @brief @c M3G_ANIM_*. */
} m3g_anim_track_data;

/** @brief KeyframeSequence object. */
typedef struct m3g_keyframe_seq_data {
    m3g_object3d_meta object3d;
    int interpolation; /**< @brief @c M3G_KF_*. */
    int repeat_mode;
    int encoding;
    int duration;
    int valid_range_first;
    int valid_range_last;
    int component_count;
    m3g_keyframe *keyframes;
    int keyframe_count;
} m3g_keyframe_seq_data;

/** @brief Unrecognized or skipped object payload. */
typedef struct m3g_unknown_data {
    uint8_t *raw_data;
    int raw_len;
} m3g_unknown_data;

/**
 * @brief One parsed M3G object (tagged union).
 *
 * Discriminate with @c object_type (`M3G_OBJ_*`), then read the matching
 * member of @c u.
 */
typedef struct m3g_object {
    int object_id;   /**< @brief Sequential id assigned while parsing (1-based). */
    int object_type; /**< @brief @c M3G_OBJ_* value. */
    int raw_length;  /**< @brief Payload size in the section. */
    union {
        m3g_header_data header;
        m3g_external_ref_data external;
        m3g_group_data group;
        m3g_camera_data camera;
        m3g_light_data light;
        m3g_background_data background;
        m3g_fog_data fog;
        m3g_polygon_mode_data polygon_mode;
        m3g_material_data material;
        m3g_vertex_array_data vertex_array;
        m3g_vertex_buffer_data vertex_buffer;
        m3g_triangle_strip_data triangle_strip;
        m3g_appearance_data appearance;
        m3g_texture2d_data texture2d;
        m3g_image2d_data image2d;
        m3g_mesh_data mesh;
        m3g_anim_controller_data anim_controller;
        m3g_anim_track_data anim_track;
        m3g_keyframe_seq_data keyframe_seq;
        m3g_unknown_data unknown;
    } u;
} m3g_object;

/**
 * @brief Complete parsed M3G file (header, sections, object map).
 *
 * @c objects is indexed by @c object_id; slot `0` is unused.
 * @c object_count is the highest assigned id.
 */
typedef struct m3g_file {
    m3g_object *header; /**< @brief Points into @c objects, or NULL. */
    m3g_section_info *sections;
    int section_count;
    m3g_object **objects;
    int object_count;
    int object_capacity;
} m3g_file;

/** @brief Free a parsed file graph (NULL-safe fields). */
M3G_API void m3g_file_free(m3g_file *f);
/** @} */ /* m3g_c_model */

/**
 * @addtogroup m3g_c_scene
 * @{
 */

/** @brief Non-fatal conversion note (code + message). */
typedef struct m3g_warning {
    char *code;
    char *message;
} m3g_warning;

/** @brief One glTF image source (embedded RGBA8 and/or external path). */
typedef struct m3g_scene_image {
    char *name;
    int object_id;
    int is_embedded; /**< @brief Non-zero → @c pixels is RGBA8. */
    int width;
    int height;
    uint8_t *pixels;
    int pixels_len;
    char *source_path; /**< @brief External file when not embedded. */
} m3g_scene_image;

/** @brief glTF sampler parameters. */
typedef struct m3g_scene_sampler {
    int mag_filter; /**< @brief `-1` = unset. */
    int min_filter; /**< @brief `-1` = unset. */
    int wrap_s;
    int wrap_t;
} m3g_scene_sampler;

/** @brief glTF texture linking image + optional sampler. */
typedef struct m3g_scene_texture {
    char *name;
    int image_index;
    int sampler_index; /**< @brief `-1` = none. */
} m3g_scene_texture;

/** @brief glTF PBR metallic-roughness material fields. */
typedef struct m3g_scene_material {
    char *name;
    float base_color_factor[4];
    int base_color_texture_index; /**< @brief `-1` = none. */
    float emissive_factor[3];
    float roughness_factor;
    float metallic_factor;
    int double_sided;
    char *alpha_mode; /**< @brief NULL or e.g. `"BLEND"`. */
} m3g_scene_material;

/** @brief Triangle mesh primitive (interleaved-friendly arrays). */
typedef struct m3g_scene_primitive {
    char *name;
    float *positions;
    int position_count; /**< @brief Float count (`verts * 3`). */
    float *normals;
    int normal_count;
    float *tex_coords0;
    int tex_coord_count;
    float *vertex_colors;
    int vertex_color_count;
    int *indices;
    int index_count;
    int material_index; /**< @brief `-1` = none. */
} m3g_scene_primitive;

/** @brief Mesh with one or more primitives. */
typedef struct m3g_scene_mesh {
    char *name;
    m3g_scene_primitive *primitives;
    int primitive_count;
} m3g_scene_mesh;

/** @brief Named camera (perspective optional). */
typedef struct m3g_scene_camera {
    char *name;
    int has_perspective;
    float yfov_radians;
    float aspect_ratio; /**< @brief `0` = unset. */
    float znear;
    float zfar; /**< @brief `0` = unset. */
} m3g_scene_camera;

/**
 * @brief Scene graph node; TRS and/or matrix, mesh/camera refs, children.
 * Indices refer to parallel arrays in @ref m3g_scene_ir.
 */
typedef struct m3g_scene_node {
    char *name;
    float matrix[16];
    int has_matrix;
    float translation[3];
    float rotation[4]; /**< @brief xyzw quaternion when @c has_trs is set. */
    float scale[3];
    int has_trs;
    int mesh_index;   /**< @brief `-1` = none. */
    int camera_index; /**< @brief `-1` = none. */
    int *children;
    int child_count;
} m3g_scene_node;

/** @brief Animation sampler input/output floats. */
typedef struct m3g_scene_anim_sampler {
    float *times;
    int time_count;
    float *values;
    int value_count;
    char interpolation[16]; /**< @brief e.g. `"LINEAR"`, `"STEP"`. */
    int component_count;
} m3g_scene_anim_sampler;

/** @brief Channel binding sampler → node path. */
typedef struct m3g_scene_anim_channel {
    int sampler_index;
    int node_index;
    char path[32]; /**< @brief e.g. `"translation"`, `"rotation"`, `"scale"`. */
} m3g_scene_anim_channel;

/** @brief Named animation clip. */
typedef struct m3g_scene_animation {
    char *name;
    m3g_scene_anim_sampler *samplers;
    int sampler_count;
    m3g_scene_anim_channel *channels;
    int channel_count;
} m3g_scene_animation;

/**
 * @brief Full intermediate representation ready for glTF export.
 *
 * Indices in nodes/meshes/materials/… refer to positions in the parallel arrays.
 */
typedef struct m3g_scene_ir {
    m3g_scene_node *nodes;
    int node_count;
    int *root_node_indices; /**< @brief Indices into @c nodes for the default scene. */
    int root_count;
    m3g_scene_mesh *meshes;
    int mesh_count;
    m3g_scene_material *materials;
    int material_count;
    m3g_scene_texture *textures;
    int texture_count;
    m3g_scene_image *images;
    int image_count;
    m3g_scene_sampler *samplers;
    int sampler_count;
    m3g_scene_camera *cameras;
    int camera_count;
    m3g_scene_animation *animations;
    int animation_count;
    m3g_warning *warnings;
    int warning_count;
} m3g_scene_ir;

/** @brief Free scene IR heap ownership (NULL-safe). */
M3G_API void m3g_scene_free(m3g_scene_ir *s);
/** @} */ /* m3g_c_scene */

/**
 * @addtogroup m3g_c_decode
 * @{
 */

/**
 * @brief Optional decode knobs.
 */
typedef struct m3g_decode_options {
    /**
     * @brief Optional external pattern image applied to untextured materials.
     * NULL = disabled.
     */
    char const *pattern_path;
} m3g_decode_options;

/**
 * @brief Result of decoding one M3G asset.
 *
 * Owns @c file and @c scene; free with @ref m3g_decoded_free.
 */
typedef struct m3g_decoded {
    char *source_path; /**< @brief Normalized path or caller-supplied label. */
    m3g_file file;     /**< @brief Parsed object graph. */
    m3g_scene_ir scene; /**< @brief Export-ready intermediate scene. */
} m3g_decoded;

/**
 * @brief Decode an `.m3g` file from disk.
 * @param path Filesystem path.
 * @param options Optional; NULL = defaults.
 * @param[out] out Filled on success; must be freed with @ref m3g_decoded_free.
 * @return @ref M3G_OK or a negative error code.
 *
 * Requires @ref m3g_deflate_io when the file uses compressed sections, and
 * @ref m3g_image_io when decoding embedded PNG/JPEG images.
 */
M3G_API m3g_result m3g_decode_file(char const *path, m3g_decode_options const *options, m3g_decoded *out);

/**
 * @brief Decode M3G bytes already in memory.
 * @param bytes File contents.
 * @param len Byte length of @p bytes.
 * @param source_path Label for diagnostics / relative resolves (may be empty).
 * @param options Optional; NULL = defaults.
 * @param[out] out Filled on success.
 */
M3G_API m3g_result m3g_decode_bytes(uint8_t const *bytes, size_t len, char const *source_path,
                                    m3g_decode_options const *options, m3g_decoded *out);

/**
 * @brief Release all heap ownership in a @ref m3g_decoded (NULL-safe).
 */
M3G_API void m3g_decoded_free(m3g_decoded *d);
/** @} */ /* m3g_c_decode */

/**
 * @addtogroup m3g_c_math
 * @{
 * Row-major @c float[16] helpers; forward to @ref m3g_math_io.
 */

M3G_API void m3g_mat4_identity(float m[16]);
M3G_API void m3g_mat4_mul(float const a[16], float const b[16], float out[16]);
M3G_API int m3g_mat4_is_identity(float const m[16], float epsilon);
M3G_API void m3g_mat4_translation(float x, float y, float z, float out[16]);
M3G_API void m3g_mat4_scale(float x, float y, float z, float out[16]);
M3G_API void m3g_mat4_rotation_axis(float angle_rad, float ax, float ay, float az, float out[16]);
M3G_API float m3g_vec3_length(float x, float y, float z);
M3G_API void m3g_quat_from_axis_angle_deg(float angle_deg, float ax, float ay, float az, float q_xyzw[4]);
/**
 * @brief Local matrix for a node (general transform, component TRS, or identity).
 */
M3G_API void m3g_node_matrix(m3g_node_meta const *node, float out[16]);
/**
 * @brief Decompose row-major matrix into translation, xyzw quaternion, scale.
 */
M3G_API void m3g_decompose_trs(float const m[16], float t[3], float q_xyzw[4], float s[3]);
/** @} */ /* m3g_c_math */

#ifdef __cplusplus
}
#endif

#endif /* M3G_H_INCLUDED */


/* ============================ IMPLEMENTATION ============================ */
#ifndef DOXYGEN
#ifdef M3G_IMPLEMENTATION
#ifndef M3G_IMPLEMENTATION_INCLUDED
#define M3G_IMPLEMENTATION_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>

#ifndef M3G_MALLOC
#define M3G_MALLOC(sz) malloc(sz)
#endif
#ifndef M3G_REALLOC
#define M3G_REALLOC(p, sz) realloc(p, sz)
#endif
#ifndef M3G_FREE
#define M3G_FREE(p) free(p)
#endif
#ifndef M3G_ASSERT
#include <assert.h>
#define M3G_ASSERT(c) assert(c)
#endif

#ifndef M3G_PI
#define M3G_PI 3.14159265358979323846
#endif

/* ---- error string ---- */
static char m3g__errbuf[1024];
static void m3g__set_err(char const *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(m3g__errbuf, sizeof(m3g__errbuf), fmt, ap);
    va_end(ap);
}
M3G_API char const *m3g_last_error(void) { return m3g__errbuf[0] ? m3g__errbuf : ""; }

static void *m3g__xmalloc(size_t n) {
    void *p = M3G_MALLOC(n ? n : 1);
    if (!p) m3g__set_err("out of memory");
    return p;
}
static void *m3g__xrealloc(void *p, size_t n) {
    void *q = M3G_REALLOC(p, n ? n : 1);
    if (!q) m3g__set_err("out of memory");
    return q;
}
static char *m3g__strdup(char const *s) {
    size_t n;
    char *d;
    if (!s) s = "";
    n = strlen(s) + 1;
    d = (char *)m3g__xmalloc(n);
    if (d) memcpy(d, s, n);
    return d;
}

/* dynamic array helpers */
#define M3G__DA_PUSH(arr, count, cap, val, T) do { \
    if ((count) >= (cap)) { \
        int ncap = (cap) ? (cap) * 2 : 8; \
        T *na = (T *)m3g__xrealloc((arr), (size_t)ncap * sizeof(T)); \
        if (!na) return M3G_ERR_NOMEM; \
        (arr) = na; (cap) = ncap; \
    } \
    (arr)[(count)++] = (val); \
} while (0)

/* ---- backends ---- */
static m3g_deflate_io m3g__deflate_io;
static m3g_image_io m3g__image_io;
static m3g_math_io m3g__math_io;
static int m3g__math_io_ready;

/* ---- portable math backend (math.h only; default) ---- */
static float m3g__port_vec3_length(float x, float y, float z, void *user) {
    (void)user; return sqrtf(x * x + y * y + z * z);
}
static void m3g__port_mat4_identity(float out[16], void *user) {
    (void)user;
    memset(out, 0, 16 * sizeof(float));
    out[0] = out[5] = out[10] = out[15] = 1.f;
}
static void m3g__port_mat4_mul(float const a[16], float const b[16], float out[16], void *user) {
    int row, col, k; float r[16];
    (void)user;
    for (row = 0; row < 4; ++row)
        for (col = 0; col < 4; ++col) {
            float sum = 0.f;
            for (k = 0; k < 4; ++k) sum += a[row * 4 + k] * b[k * 4 + col];
            r[row * 4 + col] = sum;
        }
    memcpy(out, r, sizeof(r));
}
static int m3g__port_mat4_is_identity(float const m[16], float epsilon, void *user) {
    static float const id[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    int i; (void)user;
    for (i = 0; i < 16; ++i) if (fabsf(m[i] - id[i]) > epsilon) return 0;
    return 1;
}
static void m3g__port_mat4_translation(float x, float y, float z, float out[16], void *user) {
    m3g__port_mat4_identity(out, user);
    out[3] = x; out[7] = y; out[11] = z;
}
static void m3g__port_mat4_scale(float x, float y, float z, float out[16], void *user) {
    (void)user;
    memset(out, 0, 16 * sizeof(float));
    out[0] = x; out[5] = y; out[10] = z; out[15] = 1.f;
}
static void m3g__port_mat4_rotation_axis(float angle_rad, float ax, float ay, float az, float out[16], void *user) {
    float len, x, y, z, c, s, t;
    (void)user;
    len = sqrtf(ax * ax + ay * ay + az * az);
    if (len < 1e-6f || angle_rad == 0.f) { m3g__port_mat4_identity(out, user); return; }
    x = ax / len; y = ay / len; z = az / len;
    c = cosf(angle_rad); s = sinf(angle_rad); t = 1.f - c;
    /* row-major, column-vector (M3G): same as former C++ axis_angle_matrix_row_major */
    out[0]  = t*x*x + c;   out[1]  = t*x*y - s*z; out[2]  = t*x*z + s*y; out[3]  = 0.f;
    out[4]  = t*x*y + s*z; out[5]  = t*y*y + c;   out[6]  = t*y*z - s*x; out[7]  = 0.f;
    out[8]  = t*x*z - s*y; out[9]  = t*y*z + s*x; out[10] = t*z*z + c;   out[11] = 0.f;
    out[12] = 0.f;         out[13] = 0.f;         out[14] = 0.f;         out[15] = 1.f;
}
static void m3g__port_quat_from_rm_rot(float const r00, float r01, float r02,
                                       float r10, float r11, float r12,
                                       float r20, float r21, float r22,
                                       float q_xyzw[4]) {
    float trace = r00 + r11 + r22;
    float x, y, z, w, s, qlen;
    if (trace > 0.f) {
        s = sqrtf(trace + 1.f) * 2.f;
        w = 0.25f * s; x = (r21 - r12) / s; y = (r02 - r20) / s; z = (r10 - r01) / s;
    } else if (r00 > r11 && r00 > r22) {
        s = sqrtf(1.f + r00 - r11 - r22) * 2.f;
        w = (r21 - r12) / s; x = 0.25f * s; y = (r01 + r10) / s; z = (r02 + r20) / s;
    } else if (r11 > r22) {
        s = sqrtf(1.f + r11 - r00 - r22) * 2.f;
        w = (r02 - r20) / s; x = (r01 + r10) / s; y = 0.25f * s; z = (r12 + r21) / s;
    } else {
        s = sqrtf(1.f + r22 - r00 - r11) * 2.f;
        w = (r10 - r01) / s; x = (r02 + r20) / s; y = (r12 + r21) / s; z = 0.25f * s;
    }
    qlen = sqrtf(x*x + y*y + z*z + w*w);
    if (qlen < 1e-8f) { q_xyzw[0]=0; q_xyzw[1]=0; q_xyzw[2]=0; q_xyzw[3]=1; return; }
    q_xyzw[0]=x/qlen; q_xyzw[1]=y/qlen; q_xyzw[2]=z/qlen; q_xyzw[3]=w/qlen;
}
static void m3g__port_decompose_trs(float const matrix[16], float t[3], float q_xyzw[4], float s[3], void *user) {
    float sx, sy, sz, det;
    (void)user;
    t[0] = matrix[3]; t[1] = matrix[7]; t[2] = matrix[11];
    sx = sqrtf(matrix[0]*matrix[0] + matrix[4]*matrix[4] + matrix[8]*matrix[8]);
    sy = sqrtf(matrix[1]*matrix[1] + matrix[5]*matrix[5] + matrix[9]*matrix[9]);
    sz = sqrtf(matrix[2]*matrix[2] + matrix[6]*matrix[6] + matrix[10]*matrix[10]);
    det = matrix[0]*(matrix[5]*matrix[10]-matrix[6]*matrix[9])
        - matrix[1]*(matrix[4]*matrix[10]-matrix[6]*matrix[8])
        + matrix[2]*(matrix[4]*matrix[9]-matrix[5]*matrix[8]);
    if (det < 0.f) sx = -sx;
    if (sx == 0.f) sx = 1e-8f;
    if (sy == 0.f) sy = 1e-8f;
    if (sz == 0.f) sz = 1e-8f;
    s[0] = sx; s[1] = sy; s[2] = sz;
    m3g__port_quat_from_rm_rot(
        matrix[0]/sx, matrix[1]/sy, matrix[2]/sz,
        matrix[4]/sx, matrix[5]/sy, matrix[6]/sz,
        matrix[8]/sx, matrix[9]/sy, matrix[10]/sz, q_xyzw);
}
static void m3g__port_quat_aa_deg(float angle_deg, float ax, float ay, float az, float q_xyzw[4], void *user) {
    float len, half, sn, cs;
    (void)user;
    len = sqrtf(ax*ax + ay*ay + az*az);
    if (len < 1e-8f || fabsf(angle_deg) < 1e-8f) {
        q_xyzw[0]=0; q_xyzw[1]=0; q_xyzw[2]=0; q_xyzw[3]=1; return;
    }
    half = (angle_deg * (float)M3G_PI / 180.f) * 0.5f;
    sn = sinf(half); cs = cosf(half);
    q_xyzw[0] = ax/len*sn; q_xyzw[1] = ay/len*sn; q_xyzw[2] = az/len*sn; q_xyzw[3] = cs;
}

M3G_API void m3g_install_portable_math_io(void) {
    m3g_math_io io;
    memset(&io, 0, sizeof(io));
    io.vec3_length = m3g__port_vec3_length;
    io.mat4_identity = m3g__port_mat4_identity;
    io.mat4_mul = m3g__port_mat4_mul;
    io.mat4_is_identity = m3g__port_mat4_is_identity;
    io.mat4_translation = m3g__port_mat4_translation;
    io.mat4_scale = m3g__port_mat4_scale;
    io.mat4_rotation_axis = m3g__port_mat4_rotation_axis;
    io.decompose_trs = m3g__port_decompose_trs;
    io.quat_from_axis_angle_deg = m3g__port_quat_aa_deg;
    io.user = NULL;
    m3g__math_io = io;
    m3g__math_io_ready = 1;
}

static m3g_math_io const *m3g__require_math_io(void) {
    if (!m3g__math_io_ready) m3g_install_portable_math_io();
    return &m3g__math_io;
}

M3G_API void m3g_set_deflate_io(m3g_deflate_io const *io) {
    if (!io) { memset(&m3g__deflate_io, 0, sizeof(m3g__deflate_io)); return; }
    m3g__deflate_io = *io;
}
M3G_API m3g_deflate_io const *m3g_get_deflate_io(void) {
    if (!m3g__deflate_io.adler32 && !m3g__deflate_io.uncompress && !m3g__deflate_io.uncompress_to_heap)
        return NULL;
    return &m3g__deflate_io;
}
M3G_API void m3g_set_image_io(m3g_image_io const *io) {
    if (!io) { memset(&m3g__image_io, 0, sizeof(m3g__image_io)); return; }
    m3g__image_io = *io;
}
M3G_API m3g_image_io const *m3g_get_image_io(void) {
    if (!m3g__image_io.load_file && !m3g__image_io.load_memory) return NULL;
    return &m3g__image_io;
}
M3G_API void m3g_set_math_io(m3g_math_io const *io) {
    if (!io) { m3g_install_portable_math_io(); return; }
    m3g__math_io = *io;
    m3g__math_io_ready = 1;
}
M3G_API m3g_math_io const *m3g_get_math_io(void) {
    return m3g__require_math_io();
}

M3G_API uint32_t m3g_deflate_adler32(uint32_t adler, unsigned char const *ptr, size_t buf_len) {
    m3g_deflate_io const *io = m3g_get_deflate_io();
    if (!io || !io->adler32) { m3g__set_err("deflate adler32 not set"); return 0; }
    return io->adler32(adler, ptr, buf_len, io->user);
}
M3G_API int m3g_deflate_uncompress(unsigned char *dest, size_t *dest_len, unsigned char const *source, size_t source_len) {
    m3g_deflate_io const *io = m3g_get_deflate_io();
    if (!io || !io->uncompress) { m3g__set_err("deflate uncompress not set"); return -1; }
    return io->uncompress(dest, dest_len, source, source_len, io->user);
}
M3G_API void *m3g_deflate_uncompress_to_heap(unsigned char const *source, size_t source_len, size_t *out_len, int zlib_header) {
    m3g_deflate_io const *io = m3g_get_deflate_io();
    if (!io || !io->uncompress_to_heap) { m3g__set_err("deflate uncompress_to_heap not set"); return NULL; }
    return io->uncompress_to_heap(source, source_len, out_len, zlib_header, io->user);
}
M3G_API void m3g_deflate_free(void *p) {
    m3g_deflate_io const *io;
    if (!p) return;
    io = m3g_get_deflate_io();
    if (io && io->free_mem) io->free_mem(p, io->user);
}
M3G_API unsigned char *m3g_image_load_memory(unsigned char const *buffer, int len, int *width, int *height, int *channels_in_file, int req_comp) {
    m3g_image_io const *io = m3g_get_image_io();
    if (!io || !io->load_memory) { m3g__set_err("image load_memory not set"); return NULL; }
    return io->load_memory(buffer, len, width, height, channels_in_file, req_comp, io->user);
}
M3G_API void m3g_image_free_pixels(void *pixels) {
    m3g_image_io const *io;
    if (!pixels) return;
    io = m3g_get_image_io();
    if (io && io->free_pixels) io->free_pixels(pixels, io->user);
}

M3G_API char const *m3g_type_name(int object_type) {
    switch (object_type) {
    case M3G_OBJ_HEADER: return "Header";
    case M3G_OBJ_ANIMATION_CONTROLLER: return "AnimationController";
    case M3G_OBJ_ANIMATION_TRACK: return "AnimationTrack";
    case M3G_OBJ_APPEARANCE: return "Appearance";
    case M3G_OBJ_BACKGROUND: return "Background";
    case M3G_OBJ_CAMERA: return "Camera";
    case M3G_OBJ_COMPOSITING_MODE: return "CompositingMode";
    case M3G_OBJ_FOG: return "Fog";
    case M3G_OBJ_POLYGON_MODE: return "PolygonMode";
    case M3G_OBJ_GROUP: return "Group";
    case M3G_OBJ_IMAGE_2D: return "Image2D";
    case M3G_OBJ_TRIANGLE_STRIP_ARRAY: return "TriangleStripArray";
    case M3G_OBJ_LIGHT: return "Light";
    case M3G_OBJ_MATERIAL: return "Material";
    case M3G_OBJ_MESH: return "Mesh";
    case M3G_OBJ_MORPHING_MESH: return "MorphingMesh";
    case M3G_OBJ_SKINNED_MESH: return "SkinnedMesh";
    case M3G_OBJ_TEXTURE_2D: return "Texture2D";
    case M3G_OBJ_SPRITE_3D: return "Sprite3D";
    case M3G_OBJ_KEYFRAME_SEQUENCE: return "KeyframeSequence";
    case M3G_OBJ_VERTEX_ARRAY: return "VertexArray";
    case M3G_OBJ_VERTEX_BUFFER: return "VertexBuffer";
    case M3G_OBJ_WORLD: return "World";
    case M3G_OBJ_EXTERNAL_REFERENCE: return "ExternalReference";
    default: return "Unknown";
    }
}

/* ---- matrix helpers: forward to m3g_math_io ---- */
M3G_API void m3g_mat4_identity(float m[16]) {
    m3g_math_io const *io = m3g__require_math_io();
    io->mat4_identity(m, io->user);
}
M3G_API void m3g_mat4_mul(float const a[16], float const b[16], float out[16]) {
    m3g_math_io const *io = m3g__require_math_io();
    io->mat4_mul(a, b, out, io->user);
}
M3G_API int m3g_mat4_is_identity(float const m[16], float epsilon) {
    m3g_math_io const *io = m3g__require_math_io();
    return io->mat4_is_identity(m, epsilon, io->user);
}
M3G_API void m3g_mat4_translation(float x, float y, float z, float out[16]) {
    m3g_math_io const *io = m3g__require_math_io();
    io->mat4_translation(x, y, z, out, io->user);
}
M3G_API void m3g_mat4_scale(float x, float y, float z, float out[16]) {
    m3g_math_io const *io = m3g__require_math_io();
    io->mat4_scale(x, y, z, out, io->user);
}
M3G_API void m3g_mat4_rotation_axis(float angle_rad, float ax, float ay, float az, float out[16]) {
    m3g_math_io const *io = m3g__require_math_io();
    io->mat4_rotation_axis(angle_rad, ax, ay, az, out, io->user);
}
M3G_API float m3g_vec3_length(float x, float y, float z) {
    m3g_math_io const *io = m3g__require_math_io();
    return io->vec3_length(x, y, z, io->user);
}
M3G_API void m3g_quat_from_axis_angle_deg(float angle_deg, float ax, float ay, float az, float q_xyzw[4]) {
    m3g_math_io const *io = m3g__require_math_io();
    io->quat_from_axis_angle_deg(angle_deg, ax, ay, az, q_xyzw, io->user);
}
M3G_API void m3g_decompose_trs(float const matrix[16], float t[3], float q_xyzw[4], float s[3]) {
    m3g_math_io const *io = m3g__require_math_io();
    io->decompose_trs(matrix, t, q_xyzw, s, io->user);
}

static void m3g__component_to_rm(m3g_component_transform const *c, float out[16]) {
    float T[16], R[16], S[16], RS[16];
    m3g_mat4_translation(c->translation[0], c->translation[1], c->translation[2], T);
    m3g_mat4_rotation_axis(c->orientation_angle, c->orientation_axis[0], c->orientation_axis[1], c->orientation_axis[2], R);
    m3g_mat4_scale(c->scale[0], c->scale[1], c->scale[2], S);
    m3g_mat4_mul(R, S, RS);
    m3g_mat4_mul(T, RS, out);
}
M3G_API void m3g_node_matrix(m3g_node_meta const *node, float out[16]) {
    if (node->transformable.has_general_transform) {
        memcpy(out, node->transformable.general_transform, 16 * sizeof(float));
        return;
    }
    if (node->transformable.component.present) {
        m3g__component_to_rm(&node->transformable.component, out);
        return;
    }
    m3g_mat4_identity(out);
}

static void m3g__quat_from_axis_angle_deg(float angle_deg, float ax, float ay, float az, float q_out[4]) {
    m3g_quat_from_axis_angle_deg(angle_deg, ax, ay, az, q_out);
}

/* ---- binary reader ---- */
typedef struct m3g__br {
    uint8_t const *data;
    size_t size;
    size_t pos;
    char label[64];
} m3g__br;

static void m3g__br_init(m3g__br *r, uint8_t const *data, size_t size, char const *label) {
    memset(r, 0, sizeof(*r));
    r->data = data; r->size = size; r->pos = 0;
    if (label) {
        strncpy(r->label, label, sizeof(r->label)-1);
    }
}
static size_t m3g__br_remain(m3g__br const *r) { return r->size - r->pos; }
static int m3g__br_eof(m3g__br const *r) { return r->pos >= r->size; }
static m3g_result m3g__br_need(m3g__br *r, size_t n) {
    if (m3g__br_remain(r) < n) {
        m3g__set_err("%s @ %zu: need %zu bytes, have %zu", r->label, r->pos, n, m3g__br_remain(r));
        return M3G_ERR_FORMAT;
    }
    return M3G_OK;
}
static m3g_result m3g__read_u8(m3g__br *r, uint8_t *o) {
    if (m3g__br_need(r, 1)) return M3G_ERR_FORMAT;
    *o = r->data[r->pos++]; return M3G_OK;
}
static m3g_result m3g__read_i8(m3g__br *r, int *o) {
    uint8_t v; m3g_result rc = m3g__read_u8(r, &v); if (rc) return rc;
    *o = (int8_t)v; return M3G_OK;
}
static m3g_result m3g__read_bool(m3g__br *r, int *o) {
    uint8_t v; m3g_result rc = m3g__read_u8(r, &v); if (rc) return rc;
    *o = v != 0; return M3G_OK;
}
static m3g_result m3g__read_u16(m3g__br *r, uint16_t *o) {
    if (m3g__br_need(r, 2)) return M3G_ERR_FORMAT;
    *o = (uint16_t)(r->data[r->pos] | ((uint16_t)r->data[r->pos+1] << 8));
    r->pos += 2; return M3G_OK;
}
static m3g_result m3g__read_i16(m3g__br *r, int *o) {
    uint16_t v; m3g_result rc = m3g__read_u16(r, &v); if (rc) return rc;
    *o = (int16_t)v; return M3G_OK;
}
static m3g_result m3g__read_u32(m3g__br *r, uint32_t *o) {
    if (m3g__br_need(r, 4)) return M3G_ERR_FORMAT;
    *o = (uint32_t)r->data[r->pos]
       | ((uint32_t)r->data[r->pos+1] << 8)
       | ((uint32_t)r->data[r->pos+2] << 16)
       | ((uint32_t)r->data[r->pos+3] << 24);
    r->pos += 4; return M3G_OK;
}
static m3g_result m3g__read_i32(m3g__br *r, int *o) {
    uint32_t v; m3g_result rc = m3g__read_u32(r, &v); if (rc) return rc;
    *o = (int32_t)v; return M3G_OK;
}
static m3g_result m3g__read_f32(m3g__br *r, float *o) {
    uint32_t bits; m3g_result rc = m3g__read_u32(r, &bits); if (rc) return rc;
    memcpy(o, &bits, 4); return M3G_OK;
}
static m3g_result m3g__read_bytes(m3g__br *r, size_t n, uint8_t **out) {
    uint8_t *p;
    if (m3g__br_need(r, n)) return M3G_ERR_FORMAT;
    p = (uint8_t *)m3g__xmalloc(n ? n : 1);
    if (!p) return M3G_ERR_NOMEM;
    memcpy(p, r->data + r->pos, n);
    r->pos += n;
    *out = p;
    return M3G_OK;
}
static m3g_result m3g__read_cstring(m3g__br *r, char **out) {
    size_t i, start = r->pos;
    char *s;
    for (i = r->pos; i < r->size; ++i) {
        if (r->data[i] == 0) {
            s = (char *)m3g__xmalloc(i - start + 1);
            if (!s) return M3G_ERR_NOMEM;
            memcpy(s, r->data + start, i - start);
            s[i - start] = 0;
            r->pos = i + 1;
            *out = s;
            return M3G_OK;
        }
    }
    m3g__set_err("%s: missing cstring terminator", r->label);
    return M3G_ERR_FORMAT;
}
static m3g_result m3g__checked_int(uint32_t v, int *o, char const *lab) {
    if (v > 0x7FFFFFFFu) { m3g__set_err("%s out of range", lab); return M3G_ERR_RANGE; }
    *o = (int)v; return M3G_OK;
}
static void m3g__write_u32_le(uint8_t *t, int off, int value) {
    t[off] = (uint8_t)(value & 0xFF);
    t[off+1] = (uint8_t)((value >> 8) & 0xFF);
    t[off+2] = (uint8_t)((value >> 16) & 0xFF);
    t[off+3] = (uint8_t)((value >> 24) & 0xFF);
}
static m3g_result m3g__read_floats(m3g__br *r, int n, float *o) {
    int i; for (i = 0; i < n; ++i) { m3g_result rc = m3g__read_f32(r, &o[i]); if (rc) return rc; }
    return M3G_OK;
}
static m3g_result m3g__read_rgb(m3g__br *r, m3g_rgb *c) {
    uint8_t a,b,d; m3g_result rc;
    rc = m3g__read_u8(r,&a); if(rc) return rc;
    rc = m3g__read_u8(r,&b); if(rc) return rc;
    rc = m3g__read_u8(r,&d); if(rc) return rc;
    c->red=a; c->green=b; c->blue=d; return M3G_OK;
}
static m3g_result m3g__read_rgba(m3g__br *r, m3g_rgba *c) {
    uint8_t a,b,d,e; m3g_result rc;
    rc = m3g__read_u8(r,&a); if(rc) return rc;
    rc = m3g__read_u8(r,&b); if(rc) return rc;
    rc = m3g__read_u8(r,&d); if(rc) return rc;
    rc = m3g__read_u8(r,&e); if(rc) return rc;
    c->red=a; c->green=b; c->blue=d; c->alpha=e; return M3G_OK;
}
static m3g_result m3g__read_objref(m3g__br *r, int *o) {
    uint32_t v; m3g_result rc = m3g__read_u32(r, &v); if (rc) return rc;
    return m3g__checked_int(v, o, "object ref");
}

static m3g_result m3g__read_object3d(m3g__br *r, m3g_object3d_meta *m) {
    /* Spec §11.19 Object3D (j2me_mobile_3d-1_1-mrel-spec.pdf) */
    uint32_t uid, atc, upc; m3g_result rc; int i;
    memset(m, 0, sizeof(*m));
    rc = m3g__read_u32(r, &uid); if (rc) return rc;
    rc = m3g__checked_int(uid, &m->user_id, "user id"); if (rc) return rc;
    rc = m3g__read_u32(r, &atc); if (rc) return rc;
    rc = m3g__checked_int(atc, &m->animation_track_count, "anim tracks"); if (rc) return rc;
    if (m->animation_track_count > 0) {
        m->animation_track_ids = (int *)m3g__xmalloc((size_t)m->animation_track_count * sizeof(int));
        if (!m->animation_track_ids) return M3G_ERR_NOMEM;
        for (i = 0; i < m->animation_track_count; ++i) {
            rc = m3g__read_objref(r, &m->animation_track_ids[i]); if (rc) return rc;
        }
    }
    rc = m3g__read_u32(r, &upc); if (rc) return rc;
    rc = m3g__checked_int(upc, &m->user_parameter_count, "user params"); if (rc) return rc;
    if (m->user_parameter_count > 0) {
        m->user_parameters = (m3g_user_parameter *)m3g__xmalloc((size_t)m->user_parameter_count * sizeof(m3g_user_parameter));
        if (!m->user_parameters) return M3G_ERR_NOMEM;
        memset(m->user_parameters, 0, (size_t)m->user_parameter_count * sizeof(m3g_user_parameter));
        for (i = 0; i < m->user_parameter_count; ++i) {
            uint32_t pid, vlen;
            rc = m3g__read_u32(r, &pid); if (rc) return rc;
            rc = m3g__checked_int(pid, &m->user_parameters[i].parameter_id, "param id"); if (rc) return rc;
            rc = m3g__read_u32(r, &vlen); if (rc) return rc;
            rc = m3g__checked_int(vlen, &m->user_parameters[i].value_len, "param len"); if (rc) return rc;
            if (m->user_parameters[i].value_len > 0) {
                rc = m3g__read_bytes(r, (size_t)m->user_parameters[i].value_len, &m->user_parameters[i].value);
                if (rc) return rc;
            }
        }
    }
    return M3G_OK;
}
static m3g_result m3g__read_transformable(m3g__br *r, m3g_transformable_meta *m) {
    int has_c = 0, has_g = 0; m3g_result rc;
    memset(m, 0, sizeof(*m));
    m->component.scale[0]=m->component.scale[1]=m->component.scale[2]=1.f;
    m->component.orientation_axis[2]=1.f;
    rc = m3g__read_object3d(r, &m->object3d); if (rc) return rc;
    rc = m3g__read_bool(r, &has_c); if (rc) return rc;
    if (has_c) {
        m->component.present = 1;
        rc = m3g__read_floats(r, 3, m->component.translation); if (rc) return rc;
        rc = m3g__read_floats(r, 3, m->component.scale); if (rc) return rc;
        rc = m3g__read_f32(r, &m->component.orientation_angle); if (rc) return rc;
        rc = m3g__read_floats(r, 3, m->component.orientation_axis); if (rc) return rc;
    }
    rc = m3g__read_bool(r, &has_g); if (rc) return rc;
    if (has_g) {
        m->has_general_transform = 1;
        rc = m3g__read_floats(r, 16, m->general_transform); if (rc) return rc;
    }
    return M3G_OK;
}
static m3g_result m3g__read_node(m3g__br *r, m3g_node_meta *m) {
    int has_a = 0; uint8_t af; uint32_t sc; m3g_result rc;
    memset(m, 0, sizeof(*m));
    m->enable_rendering = 1; m->enable_picking = 1; m->alpha_factor = 255;
    rc = m3g__read_transformable(r, &m->transformable); if (rc) return rc;
    rc = m3g__read_bool(r, &m->enable_rendering); if (rc) return rc;
    rc = m3g__read_bool(r, &m->enable_picking); if (rc) return rc;
    rc = m3g__read_u8(r, &af); if (rc) return rc; m->alpha_factor = af;
    rc = m3g__read_u32(r, &sc); if (rc) return rc; m->scope = sc;
    rc = m3g__read_bool(r, &has_a); if (rc) return rc;
    if (has_a) {
        uint8_t zt, yt;
        m->alignment.present = 1;
        rc = m3g__read_u8(r, &zt); if (rc) return rc; m->alignment.z_target = zt;
        rc = m3g__read_u8(r, &yt); if (rc) return rc; m->alignment.y_target = yt;
        rc = m3g__read_objref(r, &m->alignment.z_reference_id); if (rc) return rc;
        rc = m3g__read_objref(r, &m->alignment.y_reference_id); if (rc) return rc;
    }
    return M3G_OK;
}

/* free helpers */
static void m3g__free_object3d(m3g_object3d_meta *m) {
    int i;
    M3G_FREE(m->animation_track_ids); m->animation_track_ids = NULL;
    if (m->user_parameters) {
        for (i = 0; i < m->user_parameter_count; ++i) M3G_FREE(m->user_parameters[i].value);
        M3G_FREE(m->user_parameters); m->user_parameters = NULL;
    }
    m->user_parameter_count = 0;
}
static void m3g__free_transformable(m3g_transformable_meta *m) {
    m3g__free_object3d(&m->object3d);
}
static void m3g__free_node(m3g_node_meta *m) {
    m3g__free_transformable(&m->transformable);
}

static void m3g__object_free_contents(m3g_object *o) {
    int i;
    if (!o) return;
    switch (o->object_type) {
    case M3G_OBJ_HEADER:
        M3G_FREE(o->u.header.authoring_field); break;
    case M3G_OBJ_EXTERNAL_REFERENCE:
        M3G_FREE(o->u.external.uri); break;
    case M3G_OBJ_GROUP:
    case M3G_OBJ_WORLD:
        m3g__free_node(&o->u.group.node);
        M3G_FREE(o->u.group.child_ids); break;
    case M3G_OBJ_CAMERA:
        m3g__free_node(&o->u.camera.node); break;
    case M3G_OBJ_LIGHT:
        m3g__free_node(&o->u.light.node); break;
    case M3G_OBJ_BACKGROUND:
        m3g__free_object3d(&o->u.background.object3d); break;
    case M3G_OBJ_FOG:
        m3g__free_object3d(&o->u.fog.object3d); break;
    case M3G_OBJ_POLYGON_MODE:
        m3g__free_object3d(&o->u.polygon_mode.object3d); break;
    case M3G_OBJ_MATERIAL:
        m3g__free_object3d(&o->u.material.object3d); break;
    case M3G_OBJ_VERTEX_ARRAY:
        m3g__free_object3d(&o->u.vertex_array.object3d);
        M3G_FREE(o->u.vertex_array.components); break;
    case M3G_OBJ_VERTEX_BUFFER:
        m3g__free_object3d(&o->u.vertex_buffer.object3d);
        M3G_FREE(o->u.vertex_buffer.tex_coord_bindings); break;
    case M3G_OBJ_TRIANGLE_STRIP_ARRAY:
        m3g__free_object3d(&o->u.triangle_strip.object3d);
        M3G_FREE(o->u.triangle_strip.indices);
        M3G_FREE(o->u.triangle_strip.strip_lengths); break;
    case M3G_OBJ_APPEARANCE:
        m3g__free_object3d(&o->u.appearance.object3d);
        M3G_FREE(o->u.appearance.texture_ids); break;
    case M3G_OBJ_TEXTURE_2D:
        m3g__free_transformable(&o->u.texture2d.transformable); break;
    case M3G_OBJ_IMAGE_2D:
        m3g__free_object3d(&o->u.image2d.object3d);
        M3G_FREE(o->u.image2d.palette);
        M3G_FREE(o->u.image2d.pixels); break;
    case M3G_OBJ_MESH:
    case M3G_OBJ_SKINNED_MESH:
        m3g__free_node(&o->u.mesh.node);
        M3G_FREE(o->u.mesh.submeshes);
        M3G_FREE(o->u.mesh.bone_transforms); break;
    case M3G_OBJ_ANIMATION_CONTROLLER:
        m3g__free_object3d(&o->u.anim_controller.object3d); break;
    case M3G_OBJ_ANIMATION_TRACK:
        m3g__free_object3d(&o->u.anim_track.object3d); break;
    case M3G_OBJ_KEYFRAME_SEQUENCE:
        m3g__free_object3d(&o->u.keyframe_seq.object3d);
        for (i = 0; i < o->u.keyframe_seq.keyframe_count; ++i)
            M3G_FREE(o->u.keyframe_seq.keyframes[i].values);
        M3G_FREE(o->u.keyframe_seq.keyframes); break;
    default:
        M3G_FREE(o->u.unknown.raw_data); break;
    }
}

M3G_API void m3g_file_free(m3g_file *f) {
    int i;
    if (!f) return;
    for (i = 1; i <= f->object_count; ++i) {
        if (f->objects && f->objects[i]) {
            m3g__object_free_contents(f->objects[i]);
            M3G_FREE(f->objects[i]);
            f->objects[i] = NULL;
        }
    }
    M3G_FREE(f->objects);
    M3G_FREE(f->sections);
    memset(f, 0, sizeof(*f));
}

static m3g_result m3g__file_add_object(m3g_file *f, m3g_object *o) {
    int id = o->object_id;
    if (id + 1 > f->object_capacity) {
        int ncap = f->object_capacity ? f->object_capacity * 2 : 32;
        m3g_object **na;
        while (ncap <= id) ncap *= 2;
        na = (m3g_object **)m3g__xrealloc(f->objects, (size_t)ncap * sizeof(m3g_object *));
        if (!na) return M3G_ERR_NOMEM;
        if (ncap > f->object_capacity)
            memset(na + f->object_capacity, 0, (size_t)(ncap - f->object_capacity) * sizeof(m3g_object *));
        f->objects = na;
        f->object_capacity = ncap;
    }
    f->objects[id] = o;
    if (id > f->object_count) f->object_count = id;
    return M3G_OK;
}

static m3g_object *m3g__obj(m3g_file const *f, int id) {
    if (!f || id <= 0 || id > f->object_count || !f->objects) return NULL;
    return f->objects[id];
}

/* ---- object parsers ---- */
static m3g_result m3g__parse_header(m3g_object *o, m3g__br *r) {
    uint8_t maj, min; m3g_result rc;
    o->object_type = M3G_OBJ_HEADER;
    rc = m3g__read_u8(r, &maj); if (rc) return rc; o->u.header.version_major = maj;
    rc = m3g__read_u8(r, &min); if (rc) return rc; o->u.header.version_minor = min;
    rc = m3g__read_bool(r, &o->u.header.has_external_references); if (rc) return rc;
    rc = m3g__read_u32(r, &o->u.header.total_file_size); if (rc) return rc;
    rc = m3g__read_u32(r, &o->u.header.approximate_content_size); if (rc) return rc;
    return m3g__read_cstring(r, &o->u.header.authoring_field);
}
static m3g_result m3g__parse_group_like(m3g_object *o, m3g__br *r, int is_world) {
    uint32_t cc; int i; m3g_result rc;
    o->object_type = is_world ? M3G_OBJ_WORLD : M3G_OBJ_GROUP;
    o->u.group.is_world = is_world;
    rc = m3g__read_node(r, &o->u.group.node); if (rc) return rc;
    rc = m3g__read_u32(r, &cc); if (rc) return rc;
    rc = m3g__checked_int(cc, &o->u.group.child_count, "child count"); if (rc) return rc;
    if (o->u.group.child_count > 0) {
        o->u.group.child_ids = (int *)m3g__xmalloc((size_t)o->u.group.child_count * sizeof(int));
        if (!o->u.group.child_ids) return M3G_ERR_NOMEM;
        for (i = 0; i < o->u.group.child_count; ++i) {
            rc = m3g__read_objref(r, &o->u.group.child_ids[i]); if (rc) return rc;
        }
    }
    if (is_world) {
        rc = m3g__read_objref(r, &o->u.group.active_camera_id); if (rc) return rc;
        rc = m3g__read_objref(r, &o->u.group.background_id); if (rc) return rc;
    }
    return M3G_OK;
}
static m3g_result m3g__parse_camera(m3g_object *o, m3g__br *r) {
    uint8_t pt; m3g_result rc; float vals[16];
    o->object_type = M3G_OBJ_CAMERA;
    rc = m3g__read_node(r, &o->u.camera.node); if (rc) return rc;
    rc = m3g__read_u8(r, &pt); if (rc) return rc; o->u.camera.projection_type = pt;
    if (pt == 48) {
        o->u.camera.has_generic16 = 1;
        return m3g__read_floats(r, 16, o->u.camera.generic_values);
    }
    rc = m3g__read_floats(r, 4, vals); if (rc) return rc;
    if (pt == 50) {
        o->u.camera.has_perspective = 1;
        o->u.camera.fov_degrees = vals[0];
        o->u.camera.aspect_ratio = vals[1];
        o->u.camera.near_distance = vals[2];
        o->u.camera.far_distance = vals[3];
    } else {
        o->u.camera.has_generic16 = 0;
        memcpy(o->u.camera.generic_values, vals, 4 * sizeof(float));
    }
    return M3G_OK;
}
static m3g_result m3g__parse_light(m3g_object *o, m3g__br *r) {
    uint8_t mode; m3g_result rc;
    o->object_type = M3G_OBJ_LIGHT;
    rc = m3g__read_node(r, &o->u.light.node); if (rc) return rc;
    rc = m3g__read_f32(r, &o->u.light.attenuation_constant); if (rc) return rc;
    rc = m3g__read_f32(r, &o->u.light.attenuation_linear); if (rc) return rc;
    rc = m3g__read_f32(r, &o->u.light.attenuation_quadratic); if (rc) return rc;
    rc = m3g__read_rgb(r, &o->u.light.color); if (rc) return rc;
    rc = m3g__read_u8(r, &mode); if (rc) return rc; o->u.light.mode = mode;
    rc = m3g__read_f32(r, &o->u.light.intensity); if (rc) return rc;
    rc = m3g__read_f32(r, &o->u.light.spot_angle); if (rc) return rc;
    return m3g__read_f32(r, &o->u.light.spot_exponent);
}
static m3g_result m3g__parse_background(m3g_object *o, m3g__br *r) {
    uint8_t mx, my; m3g_result rc;
    o->object_type = M3G_OBJ_BACKGROUND;
    rc = m3g__read_object3d(r, &o->u.background.object3d); if (rc) return rc;
    rc = m3g__read_rgba(r, &o->u.background.background_color); if (rc) return rc;
    rc = m3g__read_objref(r, &o->u.background.background_image_id); if (rc) return rc;
    rc = m3g__read_u8(r, &mx); if (rc) return rc; o->u.background.image_mode_x = mx;
    rc = m3g__read_u8(r, &my); if (rc) return rc; o->u.background.image_mode_y = my;
    rc = m3g__read_i32(r, &o->u.background.crop_x); if (rc) return rc;
    rc = m3g__read_i32(r, &o->u.background.crop_y); if (rc) return rc;
    rc = m3g__read_i32(r, &o->u.background.crop_width); if (rc) return rc;
    rc = m3g__read_i32(r, &o->u.background.crop_height); if (rc) return rc;
    rc = m3g__read_bool(r, &o->u.background.depth_clear_enabled); if (rc) return rc;
    return m3g__read_bool(r, &o->u.background.color_clear_enabled);
}
static m3g_result m3g__parse_fog(m3g_object *o, m3g__br *r) {
    /* Spec §11.7 Fog */
    uint8_t mode; m3g_result rc;
    o->object_type = M3G_OBJ_FOG;
    rc = m3g__read_object3d(r, &o->u.fog.object3d); if (rc) return rc;
    rc = m3g__read_rgb(r, &o->u.fog.color); if (rc) return rc;
    rc = m3g__read_u8(r, &mode); if (rc) return rc; o->u.fog.mode = mode;
    if (mode == 80) { /* EXPONENTIAL */
        return m3g__read_f32(r, &o->u.fog.density);
    }
    if (mode == 81) { /* LINEAR */
        rc = m3g__read_f32(r, &o->u.fog.near_distance); if (rc) return rc;
        return m3g__read_f32(r, &o->u.fog.far_distance);
    }
    m3g__set_err("bad fog mode");
    return M3G_ERR_FORMAT;
}
static m3g_result m3g__parse_polygon_mode(m3g_object *o, m3g__br *r) {
    uint8_t c,s,w; m3g_result rc;
    o->object_type = M3G_OBJ_POLYGON_MODE;
    rc = m3g__read_object3d(r, &o->u.polygon_mode.object3d); if (rc) return rc;
    rc = m3g__read_u8(r, &c); if (rc) return rc; o->u.polygon_mode.culling = c;
    rc = m3g__read_u8(r, &s); if (rc) return rc; o->u.polygon_mode.shading = s;
    rc = m3g__read_u8(r, &w); if (rc) return rc; o->u.polygon_mode.winding = w;
    rc = m3g__read_bool(r, &o->u.polygon_mode.two_sided_lighting_enabled); if (rc) return rc;
    rc = m3g__read_bool(r, &o->u.polygon_mode.local_camera_lighting_enabled); if (rc) return rc;
    return m3g__read_bool(r, &o->u.polygon_mode.perspective_correction_enabled);
}
static m3g_result m3g__parse_material(m3g_object *o, m3g__br *r) {
    m3g_result rc;
    o->object_type = M3G_OBJ_MATERIAL;
    rc = m3g__read_object3d(r, &o->u.material.object3d); if (rc) return rc;
    rc = m3g__read_rgb(r, &o->u.material.ambient_color); if (rc) return rc;
    rc = m3g__read_rgba(r, &o->u.material.diffuse_color); if (rc) return rc;
    rc = m3g__read_rgb(r, &o->u.material.emissive_color); if (rc) return rc;
    rc = m3g__read_rgb(r, &o->u.material.specular_color); if (rc) return rc;
    rc = m3g__read_f32(r, &o->u.material.shininess); if (rc) return rc;
    return m3g__read_bool(r, &o->u.material.vertex_color_tracking_enabled);
}
static m3g_result m3g__parse_vertex_array(m3g_object *o, m3g__br *r) {
    uint8_t cs, cc, enc; uint16_t vc; int total, i; m3g_result rc;
    o->object_type = M3G_OBJ_VERTEX_ARRAY;
    rc = m3g__read_object3d(r, &o->u.vertex_array.object3d); if (rc) return rc;
    rc = m3g__read_u8(r, &cs); if (rc) return rc; o->u.vertex_array.component_size = cs;
    rc = m3g__read_u8(r, &cc); if (rc) return rc; o->u.vertex_array.component_count = cc;
    rc = m3g__read_u8(r, &enc); if (rc) return rc; o->u.vertex_array.encoding = enc;
    rc = m3g__read_u16(r, &vc); if (rc) return rc; o->u.vertex_array.vertex_count = vc;
    total = o->u.vertex_array.vertex_count * o->u.vertex_array.component_count;
    o->u.vertex_array.component_total = total;
    o->u.vertex_array.components = (int *)m3g__xmalloc((size_t)(total > 0 ? total : 1) * sizeof(int));
    if (!o->u.vertex_array.components) return M3G_ERR_NOMEM;
    for (i = 0; i < total; ++i) {
        if (cs == 1) { rc = m3g__read_i8(r, &o->u.vertex_array.components[i]); if (rc) return rc; }
        else if (cs == 2) { rc = m3g__read_i16(r, &o->u.vertex_array.components[i]); if (rc) return rc; }
        else { m3g__set_err("unsupported vertex component size %d", cs); return M3G_ERR_FORMAT; }
    }
    return M3G_OK;
}
static m3g_result m3g__parse_vertex_buffer(m3g_object *o, m3g__br *r) {
    uint32_t tc; int i; m3g_result rc;
    o->object_type = M3G_OBJ_VERTEX_BUFFER;
    rc = m3g__read_object3d(r, &o->u.vertex_buffer.object3d); if (rc) return rc;
    rc = m3g__read_rgba(r, &o->u.vertex_buffer.default_color); if (rc) return rc;
    rc = m3g__read_objref(r, &o->u.vertex_buffer.positions_id); if (rc) return rc;
    rc = m3g__read_floats(r, 3, o->u.vertex_buffer.position_bias); if (rc) return rc;
    rc = m3g__read_f32(r, &o->u.vertex_buffer.position_scale); if (rc) return rc;
    rc = m3g__read_objref(r, &o->u.vertex_buffer.normals_id); if (rc) return rc;
    rc = m3g__read_objref(r, &o->u.vertex_buffer.colors_id); if (rc) return rc;
    rc = m3g__read_u32(r, &tc); if (rc) return rc;
    rc = m3g__checked_int(tc, &o->u.vertex_buffer.tex_coord_binding_count, "tex coords"); if (rc) return rc;
    if (o->u.vertex_buffer.tex_coord_binding_count > 0) {
        o->u.vertex_buffer.tex_coord_bindings = (m3g_tex_coord_binding *)m3g__xmalloc(
            (size_t)o->u.vertex_buffer.tex_coord_binding_count * sizeof(m3g_tex_coord_binding));
        if (!o->u.vertex_buffer.tex_coord_bindings) return M3G_ERR_NOMEM;
        memset(o->u.vertex_buffer.tex_coord_bindings, 0,
               (size_t)o->u.vertex_buffer.tex_coord_binding_count * sizeof(m3g_tex_coord_binding));
        for (i = 0; i < o->u.vertex_buffer.tex_coord_binding_count; ++i) {
            m3g_tex_coord_binding *b = &o->u.vertex_buffer.tex_coord_bindings[i];
            b->scale = 1.f;
            rc = m3g__read_objref(r, &b->vertex_array_id); if (rc) return rc;
            rc = m3g__read_floats(r, 3, b->bias); if (rc) return rc;
            rc = m3g__read_f32(r, &b->scale); if (rc) return rc;
        }
    }
    return M3G_OK;
}
static m3g_result m3g__build_implicit_strips(int start, int const *lens, int nlen, int **out_idx, int *out_n) {
    int total = 0, i, k = 0, next;
    for (i = 0; i < nlen; ++i) total += lens[i];
    *out_idx = (int *)m3g__xmalloc((size_t)(total > 0 ? total : 1) * sizeof(int));
    if (!*out_idx) return M3G_ERR_NOMEM;
    next = start;
    for (i = 0; i < total; ++i) (*out_idx)[k++] = next++;
    *out_n = total;
    return M3G_OK;
}
static m3g_result m3g__parse_triangle_strip(m3g_object *o, m3g__br *r) {
    uint8_t enc; uint32_t n; int i, start; m3g_result rc;
    o->object_type = M3G_OBJ_TRIANGLE_STRIP_ARRAY;
    rc = m3g__read_object3d(r, &o->u.triangle_strip.object3d); if (rc) return rc;
    rc = m3g__read_u8(r, &enc); if (rc) return rc; o->u.triangle_strip.encoding = enc;
    if (enc == 0 || enc == 1 || enc == 2) {
        if (enc == 0) { uint32_t s; rc = m3g__read_u32(r, &s); if (rc) return rc; rc = m3g__checked_int(s, &start, "start"); if (rc) return rc; }
        else if (enc == 1) { uint8_t s; rc = m3g__read_u8(r, &s); if (rc) return rc; start = s; }
        else { uint16_t s; rc = m3g__read_u16(r, &s); if (rc) return rc; start = s; }
        rc = m3g__read_u32(r, &n); if (rc) return rc;
        rc = m3g__checked_int(n, &o->u.triangle_strip.strip_count, "strips"); if (rc) return rc;
        o->u.triangle_strip.strip_lengths = (int *)m3g__xmalloc((size_t)(o->u.triangle_strip.strip_count > 0 ? o->u.triangle_strip.strip_count : 1) * sizeof(int));
        if (!o->u.triangle_strip.strip_lengths) return M3G_ERR_NOMEM;
        for (i = 0; i < o->u.triangle_strip.strip_count; ++i) {
            uint32_t L; rc = m3g__read_u32(r, &L); if (rc) return rc;
            rc = m3g__checked_int(L, &o->u.triangle_strip.strip_lengths[i], "strip len"); if (rc) return rc;
        }
        return m3g__build_implicit_strips(start, o->u.triangle_strip.strip_lengths, o->u.triangle_strip.strip_count,
                                         &o->u.triangle_strip.indices, &o->u.triangle_strip.index_count);
    }
    if (enc == 128 || enc == 129 || enc == 130) {
        rc = m3g__read_u32(r, &n); if (rc) return rc;
        rc = m3g__checked_int(n, &o->u.triangle_strip.index_count, "index count"); if (rc) return rc;
        o->u.triangle_strip.indices = (int *)m3g__xmalloc((size_t)(o->u.triangle_strip.index_count > 0 ? o->u.triangle_strip.index_count : 1) * sizeof(int));
        if (!o->u.triangle_strip.indices) return M3G_ERR_NOMEM;
        for (i = 0; i < o->u.triangle_strip.index_count; ++i) {
            if (enc == 128) { uint32_t v; rc = m3g__read_u32(r, &v); if (rc) return rc; rc = m3g__checked_int(v, &o->u.triangle_strip.indices[i], "idx"); if (rc) return rc; }
            else if (enc == 129) { uint8_t v; rc = m3g__read_u8(r, &v); if (rc) return rc; o->u.triangle_strip.indices[i] = v; }
            else { uint16_t v; rc = m3g__read_u16(r, &v); if (rc) return rc; o->u.triangle_strip.indices[i] = v; }
        }
        rc = m3g__read_u32(r, &n); if (rc) return rc;
        rc = m3g__checked_int(n, &o->u.triangle_strip.strip_count, "strips"); if (rc) return rc;
        o->u.triangle_strip.strip_lengths = (int *)m3g__xmalloc((size_t)(o->u.triangle_strip.strip_count > 0 ? o->u.triangle_strip.strip_count : 1) * sizeof(int));
        if (!o->u.triangle_strip.strip_lengths) return M3G_ERR_NOMEM;
        for (i = 0; i < o->u.triangle_strip.strip_count; ++i) {
            uint32_t L; rc = m3g__read_u32(r, &L); if (rc) return rc;
            rc = m3g__checked_int(L, &o->u.triangle_strip.strip_lengths[i], "strip len"); if (rc) return rc;
        }
        return M3G_OK;
    }
    m3g__set_err("unsupported TriangleStripArray encoding %d", enc);
    return M3G_ERR_FORMAT;
}
static m3g_result m3g__parse_appearance(m3g_object *o, m3g__br *r) {
    uint8_t layer; uint32_t tc; int i; m3g_result rc;
    o->object_type = M3G_OBJ_APPEARANCE;
    rc = m3g__read_object3d(r, &o->u.appearance.object3d); if (rc) return rc;
    rc = m3g__read_u8(r, &layer); if (rc) return rc; o->u.appearance.layer = layer;
    rc = m3g__read_objref(r, &o->u.appearance.compositing_mode_id); if (rc) return rc;
    rc = m3g__read_objref(r, &o->u.appearance.fog_id); if (rc) return rc;
    rc = m3g__read_objref(r, &o->u.appearance.polygon_mode_id); if (rc) return rc;
    rc = m3g__read_objref(r, &o->u.appearance.material_id); if (rc) return rc;
    rc = m3g__read_u32(r, &tc); if (rc) return rc;
    rc = m3g__checked_int(tc, &o->u.appearance.texture_count, "textures"); if (rc) return rc;
    if (o->u.appearance.texture_count > 0) {
        o->u.appearance.texture_ids = (int *)m3g__xmalloc((size_t)o->u.appearance.texture_count * sizeof(int));
        if (!o->u.appearance.texture_ids) return M3G_ERR_NOMEM;
        for (i = 0; i < o->u.appearance.texture_count; ++i) {
            rc = m3g__read_objref(r, &o->u.appearance.texture_ids[i]); if (rc) return rc;
        }
    }
    return M3G_OK;
}
static m3g_result m3g__parse_texture2d(m3g_object *o, m3g__br *r) {
    uint8_t a,b,c,d,e; m3g_result rc;
    o->object_type = M3G_OBJ_TEXTURE_2D;
    rc = m3g__read_transformable(r, &o->u.texture2d.transformable); if (rc) return rc;
    rc = m3g__read_objref(r, &o->u.texture2d.image_id); if (rc) return rc;
    rc = m3g__read_rgb(r, &o->u.texture2d.blend_color); if (rc) return rc;
    rc = m3g__read_u8(r, &a); if (rc) return rc; o->u.texture2d.blending = a;
    rc = m3g__read_u8(r, &b); if (rc) return rc; o->u.texture2d.wrapping_s = b;
    rc = m3g__read_u8(r, &c); if (rc) return rc; o->u.texture2d.wrapping_t = c;
    rc = m3g__read_u8(r, &d); if (rc) return rc; o->u.texture2d.level_filter = d;
    rc = m3g__read_u8(r, &e); if (rc) return rc; o->u.texture2d.image_filter = e;
    return M3G_OK;
}
static m3g_result m3g__parse_image2d(m3g_object *o, m3g__br *r) {
    uint8_t fmt; uint32_t w,h,pl; m3g_result rc;
    o->object_type = M3G_OBJ_IMAGE_2D;
    rc = m3g__read_object3d(r, &o->u.image2d.object3d); if (rc) return rc;
    rc = m3g__read_u8(r, &fmt); if (rc) return rc; o->u.image2d.format = fmt;
    rc = m3g__read_bool(r, &o->u.image2d.is_mutable); if (rc) return rc;
    rc = m3g__read_u32(r, &w); if (rc) return rc; rc = m3g__checked_int(w, &o->u.image2d.width, "w"); if (rc) return rc;
    rc = m3g__read_u32(r, &h); if (rc) return rc; rc = m3g__checked_int(h, &o->u.image2d.height, "h"); if (rc) return rc;
    if (!o->u.image2d.is_mutable) {
        rc = m3g__read_u32(r, &pl); if (rc) return rc;
        rc = m3g__checked_int(pl, &o->u.image2d.palette_len, "palette"); if (rc) return rc;
        if (o->u.image2d.palette_len > 0) {
            rc = m3g__read_bytes(r, (size_t)o->u.image2d.palette_len, &o->u.image2d.palette); if (rc) return rc;
        }
        if (m3g__br_remain(r) >= 4) {
            rc = m3g__read_u32(r, &pl); if (rc) return rc;
            rc = m3g__checked_int(pl, &o->u.image2d.pixels_len, "pixels"); if (rc) return rc;
            if (o->u.image2d.pixels_len > 0) {
                rc = m3g__read_bytes(r, (size_t)o->u.image2d.pixels_len, &o->u.image2d.pixels); if (rc) return rc;
                o->u.image2d.has_pixels = 1;
            }
        } else if (m3g__br_remain(r) > 0) {
            size_t rem = m3g__br_remain(r);
            rc = m3g__read_bytes(r, rem, &o->u.image2d.pixels); if (rc) return rc;
            o->u.image2d.pixels_len = (int)rem;
            o->u.image2d.has_pixels = 1;
        }
    }
    return M3G_OK;
}
static m3g_result m3g__parse_mesh(m3g_object *o, m3g__br *r, int skinned) {
    uint32_t sc, bc; int i; m3g_result rc;
    o->object_type = skinned ? M3G_OBJ_SKINNED_MESH : M3G_OBJ_MESH;
    o->u.mesh.is_skinned = skinned;
    rc = m3g__read_node(r, &o->u.mesh.node); if (rc) return rc;
    rc = m3g__read_objref(r, &o->u.mesh.vertex_buffer_id); if (rc) return rc;
    rc = m3g__read_u32(r, &sc); if (rc) return rc;
    rc = m3g__checked_int(sc, &o->u.mesh.submesh_count, "submeshes"); if (rc) return rc;
    if (o->u.mesh.submesh_count > 0) {
        o->u.mesh.submeshes = (m3g_submesh_ref *)m3g__xmalloc((size_t)o->u.mesh.submesh_count * sizeof(m3g_submesh_ref));
        if (!o->u.mesh.submeshes) return M3G_ERR_NOMEM;
        for (i = 0; i < o->u.mesh.submesh_count; ++i) {
            rc = m3g__read_objref(r, &o->u.mesh.submeshes[i].index_buffer_id); if (rc) return rc;
            rc = m3g__read_objref(r, &o->u.mesh.submeshes[i].appearance_id); if (rc) return rc;
        }
    }
    if (skinned) {
        rc = m3g__read_objref(r, &o->u.mesh.skeleton_id); if (rc) return rc;
        rc = m3g__read_u32(r, &bc); if (rc) return rc;
        rc = m3g__checked_int(bc, &o->u.mesh.bone_transform_count, "bones"); if (rc) return rc;
        if (o->u.mesh.bone_transform_count > 0) {
            o->u.mesh.bone_transforms = (m3g_bone_transform *)m3g__xmalloc((size_t)o->u.mesh.bone_transform_count * sizeof(m3g_bone_transform));
            if (!o->u.mesh.bone_transforms) return M3G_ERR_NOMEM;
            for (i = 0; i < o->u.mesh.bone_transform_count; ++i) {
                m3g_bone_transform *b = &o->u.mesh.bone_transforms[i];
                uint32_t fv, vc;
                rc = m3g__read_objref(r, &b->transform_node_id); if (rc) return rc;
                rc = m3g__read_u32(r, &fv); if (rc) return rc; rc = m3g__checked_int(fv, &b->first_vertex, "fv"); if (rc) return rc;
                rc = m3g__read_u32(r, &vc); if (rc) return rc; rc = m3g__checked_int(vc, &b->vertex_count, "vc"); if (rc) return rc;
                rc = m3g__read_i32(r, &b->weight); if (rc) return rc;
            }
        }
    }
    return M3G_OK;
}
static m3g_result m3g__parse_anim_controller(m3g_object *o, m3g__br *r) {
    m3g_result rc;
    o->object_type = M3G_OBJ_ANIMATION_CONTROLLER;
    rc = m3g__read_object3d(r, &o->u.anim_controller.object3d); if (rc) return rc;
    rc = m3g__read_f32(r, &o->u.anim_controller.speed); if (rc) return rc;
    rc = m3g__read_f32(r, &o->u.anim_controller.weight); if (rc) return rc;
    rc = m3g__read_i32(r, &o->u.anim_controller.active_interval_start); if (rc) return rc;
    rc = m3g__read_i32(r, &o->u.anim_controller.active_interval_end); if (rc) return rc;
    rc = m3g__read_f32(r, &o->u.anim_controller.reference_sequence_time); if (rc) return rc;
    return m3g__read_i32(r, &o->u.anim_controller.reference_world_time);
}
static m3g_result m3g__parse_anim_track(m3g_object *o, m3g__br *r) {
    uint32_t pid; m3g_result rc;
    o->object_type = M3G_OBJ_ANIMATION_TRACK;
    rc = m3g__read_object3d(r, &o->u.anim_track.object3d); if (rc) return rc;
    rc = m3g__read_objref(r, &o->u.anim_track.keyframe_sequence_id); if (rc) return rc;
    rc = m3g__read_objref(r, &o->u.anim_track.animation_controller_id); if (rc) return rc;
    rc = m3g__read_u32(r, &pid); if (rc) return rc;
    return m3g__checked_int(pid, &o->u.anim_track.property_id, "property");
}
static m3g_result m3g__parse_keyframe_seq(m3g_object *o, m3g__br *r) {
    /* Spec §11.12 KeyframeSequence */
    uint8_t a,b,c; uint32_t d,vf,vl,cc,kc, t; int i, j; m3g_result rc;
    float *bias = NULL; float *scalev = NULL; float quant_max = 1.f;
    o->object_type = M3G_OBJ_KEYFRAME_SEQUENCE;
    rc = m3g__read_object3d(r, &o->u.keyframe_seq.object3d); if (rc) return rc;
    rc = m3g__read_u8(r, &a); if (rc) return rc; o->u.keyframe_seq.interpolation = a;
    rc = m3g__read_u8(r, &b); if (rc) return rc; o->u.keyframe_seq.repeat_mode = b;
    rc = m3g__read_u8(r, &c); if (rc) return rc; o->u.keyframe_seq.encoding = c;
    rc = m3g__read_u32(r, &d); if (rc) return rc; rc = m3g__checked_int(d, &o->u.keyframe_seq.duration, "dur"); if (rc) return rc;
    rc = m3g__read_u32(r, &vf); if (rc) return rc; rc = m3g__checked_int(vf, &o->u.keyframe_seq.valid_range_first, "vf"); if (rc) return rc;
    rc = m3g__read_u32(r, &vl); if (rc) return rc; rc = m3g__checked_int(vl, &o->u.keyframe_seq.valid_range_last, "vl"); if (rc) return rc;
    rc = m3g__read_u32(r, &cc); if (rc) return rc; rc = m3g__checked_int(cc, &o->u.keyframe_seq.component_count, "cc"); if (rc) return rc;
    rc = m3g__read_u32(r, &kc); if (rc) return rc; rc = m3g__checked_int(kc, &o->u.keyframe_seq.keyframe_count, "kc"); if (rc) return rc;
    o->u.keyframe_seq.keyframes = (m3g_keyframe *)m3g__xmalloc((size_t)(o->u.keyframe_seq.keyframe_count > 0 ? o->u.keyframe_seq.keyframe_count : 1) * sizeof(m3g_keyframe));
    if (!o->u.keyframe_seq.keyframes) return M3G_ERR_NOMEM;
    memset(o->u.keyframe_seq.keyframes, 0, (size_t)o->u.keyframe_seq.keyframe_count * sizeof(m3g_keyframe));
    if (c == 1 || c == 2) {
        size_t n = (size_t)o->u.keyframe_seq.component_count * sizeof(float);
        bias = (float *)m3g__xmalloc(n ? n : sizeof(float));
        scalev = (float *)m3g__xmalloc(n ? n : sizeof(float));
        if (!bias || !scalev) { M3G_FREE(bias); M3G_FREE(scalev); return M3G_ERR_NOMEM; }
        rc = m3g__read_floats(r, o->u.keyframe_seq.component_count, bias); if (rc) { M3G_FREE(bias); M3G_FREE(scalev); return rc; }
        rc = m3g__read_floats(r, o->u.keyframe_seq.component_count, scalev); if (rc) { M3G_FREE(bias); M3G_FREE(scalev); return rc; }
        quant_max = (c == 1) ? 255.f : 65535.f;
    } else if (c != 0) {
        m3g__set_err("bad keyframe encoding"); return M3G_ERR_FORMAT;
    }
    for (i = 0; i < o->u.keyframe_seq.keyframe_count; ++i) {
        m3g_keyframe *kf = &o->u.keyframe_seq.keyframes[i];
        kf->value_count = o->u.keyframe_seq.component_count;
        kf->values = (float *)m3g__xmalloc((size_t)(kf->value_count > 0 ? kf->value_count : 1) * sizeof(float));
        if (!kf->values) { M3G_FREE(bias); M3G_FREE(scalev); return M3G_ERR_NOMEM; }
        rc = m3g__read_u32(r, &t); if (rc) { M3G_FREE(bias); M3G_FREE(scalev); return rc; }
        rc = m3g__checked_int(t, &kf->time, "kf time"); if (rc) { M3G_FREE(bias); M3G_FREE(scalev); return rc; }
        if (c == 0) {
            rc = m3g__read_floats(r, kf->value_count, kf->values); if (rc) { M3G_FREE(bias); M3G_FREE(scalev); return rc; }
        } else {
            for (j = 0; j < kf->value_count; ++j) {
                float q;
                if (c == 1) { uint8_t v; rc = m3g__read_u8(r, &v); if (rc) { M3G_FREE(bias); M3G_FREE(scalev); return rc; } q = (float)v; }
                else { uint16_t v; rc = m3g__read_u16(r, &v); if (rc) { M3G_FREE(bias); M3G_FREE(scalev); return rc; } q = (float)v; }
                kf->values[j] = (q / quant_max) * scalev[j] + bias[j];
            }
        }
    }
    M3G_FREE(bias); M3G_FREE(scalev);
    return M3G_OK;
}

static m3g_result m3g__parse_object(m3g_object *o, int type, uint8_t const *payload, size_t plen) {
    m3g__br r; m3g_result rc;
    char lab[64];
    snprintf(lab, sizeof(lab), "object-%d/%s", o->object_id, m3g_type_name(type));
    m3g__br_init(&r, payload, plen, lab);
    memset(&o->u, 0, sizeof(o->u));
    o->object_type = type;
    o->raw_length = (int)plen;
    switch (type) {
    case M3G_OBJ_HEADER: rc = m3g__parse_header(o, &r); break;
    case M3G_OBJ_EXTERNAL_REFERENCE:
        o->object_type = M3G_OBJ_EXTERNAL_REFERENCE;
        rc = m3g__read_cstring(&r, &o->u.external.uri); break;
    case M3G_OBJ_WORLD: rc = m3g__parse_group_like(o, &r, 1); break;
    case M3G_OBJ_GROUP: rc = m3g__parse_group_like(o, &r, 0); break;
    case M3G_OBJ_CAMERA: rc = m3g__parse_camera(o, &r); break;
    case M3G_OBJ_LIGHT: rc = m3g__parse_light(o, &r); break;
    case M3G_OBJ_BACKGROUND: rc = m3g__parse_background(o, &r); break;
    case M3G_OBJ_FOG: rc = m3g__parse_fog(o, &r); break;
    case M3G_OBJ_POLYGON_MODE: rc = m3g__parse_polygon_mode(o, &r); break;
    case M3G_OBJ_MATERIAL: rc = m3g__parse_material(o, &r); break;
    case M3G_OBJ_VERTEX_ARRAY: rc = m3g__parse_vertex_array(o, &r); break;
    case M3G_OBJ_VERTEX_BUFFER: rc = m3g__parse_vertex_buffer(o, &r); break;
    case M3G_OBJ_TRIANGLE_STRIP_ARRAY: rc = m3g__parse_triangle_strip(o, &r); break;
    case M3G_OBJ_APPEARANCE: rc = m3g__parse_appearance(o, &r); break;
    case M3G_OBJ_TEXTURE_2D: rc = m3g__parse_texture2d(o, &r); break;
    case M3G_OBJ_IMAGE_2D: rc = m3g__parse_image2d(o, &r); break;
    case M3G_OBJ_MESH: rc = m3g__parse_mesh(o, &r, 0); break;
    case M3G_OBJ_SKINNED_MESH: rc = m3g__parse_mesh(o, &r, 1); break;
    case M3G_OBJ_ANIMATION_CONTROLLER: rc = m3g__parse_anim_controller(o, &r); break;
    case M3G_OBJ_ANIMATION_TRACK: rc = m3g__parse_anim_track(o, &r); break;
    case M3G_OBJ_KEYFRAME_SEQUENCE: rc = m3g__parse_keyframe_seq(o, &r); break;
    default: {
        o->object_type = type;
        o->u.unknown.raw_len = (int)plen;
        if (plen) {
            o->u.unknown.raw_data = (uint8_t *)m3g__xmalloc(plen);
            if (!o->u.unknown.raw_data) return M3G_ERR_NOMEM;
            memcpy(o->u.unknown.raw_data, payload, plen);
        }
        return M3G_OK;
    }
    }
    if (rc) return rc;
    if (!m3g__br_eof(&r)) {
        m3g__set_err("%s left %zu unread", lab, m3g__br_remain(&r));
        return M3G_ERR_FORMAT;
    }
    return M3G_OK;
}

static m3g_result m3g__parse_bytes(uint8_t const *bytes, size_t len, m3g_file *file) {
    static uint8_t const ident[12] = {0xAB,0x4A,0x53,0x52,0x31,0x38,0x34,0xBB,0x0D,0x0A,0x1A,0x0A};
    m3g__br reader; m3g_result rc; int next_id = 1;
    memset(file, 0, sizeof(*file));
    m3g__br_init(&reader, bytes, len, "m3g-file");
    if (m3g__br_need(&reader, 12)) return M3G_ERR_FORMAT;
    if (memcmp(reader.data, ident, 12) != 0) { m3g__set_err("invalid M3G identifier"); return M3G_ERR_FORMAT; }
    reader.pos = 12;

    while (!m3g__br_eof(&reader)) {
        uint8_t scheme; uint32_t tsl, ucl, expected_cs, checksum;
        int total_section_length, uncompressed_length, payload_length, section_index;
        uint8_t *payload = NULL; uint8_t *object_bytes = NULL; size_t object_len = 0;
        uint8_t *checksum_input = NULL; size_t cin_len;
        m3g__br section_reader;
        section_index = file->section_count;
        rc = m3g__read_u8(&reader, &scheme); if (rc) goto fail;
        rc = m3g__read_u32(&reader, &tsl); if (rc) goto fail;
        rc = m3g__checked_int(tsl, &total_section_length, "section total"); if (rc) goto fail;
        rc = m3g__read_u32(&reader, &ucl); if (rc) goto fail;
        rc = m3g__checked_int(ucl, &uncompressed_length, "section uncomp"); if (rc) goto fail;
        if (total_section_length < 13) { m3g__set_err("invalid section length"); rc = M3G_ERR_FORMAT; goto fail; }
        payload_length = total_section_length - 13;
        if (scheme == 0 && payload_length != uncompressed_length) { m3g__set_err("section length mismatch"); rc = M3G_ERR_FORMAT; goto fail; }
        if (scheme != 0 && scheme != 1) { m3g__set_err("unsupported compression %d", scheme); rc = M3G_ERR_FORMAT; goto fail; }
        if (payload_length > 0) {
            rc = m3g__read_bytes(&reader, (size_t)payload_length, &payload); if (rc) goto fail;
        }
        rc = m3g__read_u32(&reader, &expected_cs); if (rc) goto fail;

        cin_len = 1 + 4 + 4 + (size_t)payload_length;
        checksum_input = (uint8_t *)m3g__xmalloc(cin_len);
        if (!checksum_input) { rc = M3G_ERR_NOMEM; goto fail; }
        checksum_input[0] = scheme;
        m3g__write_u32_le(checksum_input, 1, total_section_length);
        m3g__write_u32_le(checksum_input, 5, uncompressed_length);
        if (payload_length > 0) memcpy(checksum_input + 9, payload, (size_t)payload_length);
        checksum = m3g_deflate_adler32((uint32_t)M3G_ADLER32_INIT, checksum_input, cin_len);
        M3G_FREE(checksum_input); checksum_input = NULL;
        if (checksum != expected_cs) { m3g__set_err("section checksum mismatch"); rc = M3G_ERR_FORMAT; goto fail; }

        if (scheme == 0) {
            object_bytes = payload; payload = NULL;
            object_len = (size_t)uncompressed_length;
        } else {
            size_t dest_len = (size_t)uncompressed_length;
            int drc;
            object_bytes = (uint8_t *)m3g__xmalloc(dest_len ? dest_len : 1);
            if (!object_bytes) { rc = M3G_ERR_NOMEM; goto fail; }
            drc = m3g_deflate_uncompress(object_bytes, &dest_len, payload, (size_t)payload_length);
            M3G_FREE(payload); payload = NULL;
            if (drc != M3G_DEFLATE_OK || dest_len != (size_t)uncompressed_length) {
                m3g__set_err("inflate failed rc=%d", drc); rc = M3G_ERR_FORMAT; goto fail;
            }
            object_len = dest_len;
        }

        {
            m3g_section_info si;
            m3g_section_info *na;
            si.index = section_index; si.compression_scheme = scheme;
            si.total_section_length = total_section_length; si.uncompressed_length = uncompressed_length;
            na = (m3g_section_info *)m3g__xrealloc(file->sections, (size_t)(file->section_count + 1) * sizeof(m3g_section_info));
            if (!na) { rc = M3G_ERR_NOMEM; goto fail; }
            file->sections = na;
            file->sections[file->section_count++] = si;
        }

        {
            char slab[32];
            snprintf(slab, sizeof(slab), "section-%d", section_index);
            m3g__br_init(&section_reader, object_bytes, object_len, slab);
        }
        while (!m3g__br_eof(&section_reader)) {
            uint8_t otype; uint32_t olen_u; int olen; uint8_t *opay = NULL;
            m3g_object *obj;
            rc = m3g__read_u8(&section_reader, &otype); if (rc) goto fail;
            rc = m3g__read_u32(&section_reader, &olen_u); if (rc) goto fail;
            rc = m3g__checked_int(olen_u, &olen, "object length"); if (rc) goto fail;
            if (olen > 0) {
                if (m3g__br_need(&section_reader, (size_t)olen)) { rc = M3G_ERR_FORMAT; goto fail; }
                opay = (uint8_t *)(section_reader.data + section_reader.pos);
                section_reader.pos += (size_t)olen;
            }
            obj = (m3g_object *)m3g__xmalloc(sizeof(m3g_object));
            if (!obj) { rc = M3G_ERR_NOMEM; goto fail; }
            memset(obj, 0, sizeof(*obj));
            obj->object_id = next_id;
            rc = m3g__parse_object(obj, (int)otype, opay, (size_t)olen);
            if (rc) { m3g__object_free_contents(obj); M3G_FREE(obj); goto fail; }
            rc = m3g__file_add_object(file, obj); if (rc) { m3g__object_free_contents(obj); M3G_FREE(obj); goto fail; }
            if (otype == M3G_OBJ_HEADER && !file->header) file->header = obj;
            ++next_id;
        }
        M3G_FREE(object_bytes); object_bytes = NULL;
        continue;
    fail:
        M3G_FREE(payload);
        M3G_FREE(object_bytes);
        M3G_FREE(checksum_input);
        m3g_file_free(file);
        return rc;
    }
    return M3G_OK;
}

/* ---- scene free ---- */
static void m3g__free_primitive(m3g_scene_primitive *p) {
    M3G_FREE(p->name); M3G_FREE(p->positions); M3G_FREE(p->normals);
    M3G_FREE(p->tex_coords0); M3G_FREE(p->vertex_colors); M3G_FREE(p->indices);
}
static void m3g__free_mesh_ir(m3g_scene_mesh *m) {
    int i; M3G_FREE(m->name);
    for (i = 0; i < m->primitive_count; ++i) m3g__free_primitive(&m->primitives[i]);
    M3G_FREE(m->primitives);
}
M3G_API void m3g_scene_free(m3g_scene_ir *s) {
    int i, j;
    if (!s) return;
    for (i = 0; i < s->node_count; ++i) {
        M3G_FREE(s->nodes[i].name); M3G_FREE(s->nodes[i].children);
    }
    M3G_FREE(s->nodes); M3G_FREE(s->root_node_indices);
    for (i = 0; i < s->mesh_count; ++i) m3g__free_mesh_ir(&s->meshes[i]);
    M3G_FREE(s->meshes);
    for (i = 0; i < s->material_count; ++i) {
        M3G_FREE(s->materials[i].name); M3G_FREE(s->materials[i].alpha_mode);
    }
    M3G_FREE(s->materials);
    for (i = 0; i < s->texture_count; ++i) M3G_FREE(s->textures[i].name);
    M3G_FREE(s->textures);
    for (i = 0; i < s->image_count; ++i) {
        M3G_FREE(s->images[i].name); M3G_FREE(s->images[i].pixels); M3G_FREE(s->images[i].source_path);
    }
    M3G_FREE(s->images);
    M3G_FREE(s->samplers);
    for (i = 0; i < s->camera_count; ++i) M3G_FREE(s->cameras[i].name);
    M3G_FREE(s->cameras);
    for (i = 0; i < s->animation_count; ++i) {
        M3G_FREE(s->animations[i].name);
        for (j = 0; j < s->animations[i].sampler_count; ++j) {
            M3G_FREE(s->animations[i].samplers[j].times);
            M3G_FREE(s->animations[i].samplers[j].values);
        }
        M3G_FREE(s->animations[i].samplers);
        M3G_FREE(s->animations[i].channels);
    }
    M3G_FREE(s->animations);
    for (i = 0; i < s->warning_count; ++i) {
        M3G_FREE(s->warnings[i].code); M3G_FREE(s->warnings[i].message);
    }
    M3G_FREE(s->warnings);
    memset(s, 0, sizeof(*s));
}
M3G_API void m3g_decoded_free(m3g_decoded *d) {
    if (!d) return;
    M3G_FREE(d->source_path);
    m3g_scene_free(&d->scene);
    m3g_file_free(&d->file);
    memset(d, 0, sizeof(*d));
}

/* ---- scene builder ---- */
typedef struct m3g__map_ii { int k, v; int used; } m3g__map_ii;
typedef struct m3g__builder {
    m3g_file const *file;
    char const *input_path;
    m3g_scene_ir scene;
    m3g__map_ii *node_map; int node_map_cap;
    m3g__map_ii *mesh_map; int mesh_map_cap;
    m3g__map_ii *mat_map; int mat_map_cap;
    m3g__map_ii *tex_map; int tex_map_cap;
    m3g__map_ii *img_map; int img_map_cap;
    m3g__map_ii *cam_map; int cam_map_cap;
    /* sampler keys stored parallel to scene.samplers */
} m3g__builder;

static m3g_result m3g__map_get(m3g__map_ii *map, int cap, int k, int *out, int *found) {
    int i; *found = 0;
    for (i = 0; i < cap; ++i) if (map[i].used && map[i].k == k) { *out = map[i].v; *found = 1; return M3G_OK; }
    return M3G_OK;
}
static m3g_result m3g__map_set(m3g__map_ii **map, int *cap, int k, int v) {
    int i, slot = -1;
    for (i = 0; i < *cap; ++i) {
        if ((*map)[i].used && (*map)[i].k == k) { (*map)[i].v = v; return M3G_OK; }
        if (!(*map)[i].used && slot < 0) slot = i;
    }
    if (slot < 0) {
        int ncap = *cap ? *cap * 2 : 16;
        m3g__map_ii *na = (m3g__map_ii *)m3g__xrealloc(*map, (size_t)ncap * sizeof(m3g__map_ii));
        if (!na) return M3G_ERR_NOMEM;
        memset(na + *cap, 0, (size_t)(ncap - *cap) * sizeof(m3g__map_ii));
        *map = na; slot = *cap; *cap = ncap;
    }
    (*map)[slot].used = 1; (*map)[slot].k = k; (*map)[slot].v = v;
    return M3G_OK;
}

static m3g_result m3g__warn(m3g__builder *b, char const *code, char const *msg) {
    m3g_warning w; m3g_warning *na; int i;
    for (i = 0; i < b->scene.warning_count; ++i) {
        if (strcmp(b->scene.warnings[i].code, code) == 0 && strcmp(b->scene.warnings[i].message, msg) == 0)
            return M3G_OK;
    }
    w.code = m3g__strdup(code); w.message = m3g__strdup(msg);
    if (!w.code || !w.message) { M3G_FREE(w.code); M3G_FREE(w.message); return M3G_ERR_NOMEM; }
    na = (m3g_warning *)m3g__xrealloc(b->scene.warnings, (size_t)(b->scene.warning_count + 1) * sizeof(m3g_warning));
    if (!na) { M3G_FREE(w.code); M3G_FREE(w.message); return M3G_ERR_NOMEM; }
    b->scene.warnings = na;
    b->scene.warnings[b->scene.warning_count++] = w;
    return M3G_OK;
}

static int m3g__is_node_type(int t) {
    return t == M3G_OBJ_GROUP || t == M3G_OBJ_WORLD || t == M3G_OBJ_MESH || t == M3G_OBJ_SKINNED_MESH
        || t == M3G_OBJ_CAMERA || t == M3G_OBJ_LIGHT || t == M3G_OBJ_SPRITE_3D;
}
static m3g_node_meta *m3g__node_meta_of(m3g_object *o) {
    if (!o) return NULL;
    switch (o->object_type) {
    case M3G_OBJ_GROUP: case M3G_OBJ_WORLD: return &o->u.group.node;
    case M3G_OBJ_CAMERA: return &o->u.camera.node;
    case M3G_OBJ_LIGHT: return &o->u.light.node;
    case M3G_OBJ_MESH: case M3G_OBJ_SKINNED_MESH: return &o->u.mesh.node;
    default: return NULL;
    }
}
static void m3g__synthetic_name(m3g_object const *o, char *buf, size_t n) {
    int uid = 0; m3g_node_meta const *nm;
    nm = m3g__node_meta_of((m3g_object *)o);
    if (nm) uid = nm->transformable.object3d.user_id;
    else if (o->object_type == M3G_OBJ_APPEARANCE) uid = o->u.appearance.object3d.user_id;
    else if (o->object_type == M3G_OBJ_MATERIAL) uid = o->u.material.object3d.user_id;
    else if (o->object_type == M3G_OBJ_TEXTURE_2D) uid = o->u.texture2d.transformable.object3d.user_id;
    else if (o->object_type == M3G_OBJ_IMAGE_2D) uid = o->u.image2d.object3d.user_id;
    else if (o->object_type == M3G_OBJ_EXTERNAL_REFERENCE) uid = 0;
    if (uid) snprintf(buf, n, "%s_%d_u%d", m3g_type_name(o->object_type), o->object_id, uid);
    else snprintf(buf, n, "%s_%d", m3g_type_name(o->object_type), o->object_id);
}

static int m3g__map_wrap(int v) { return v == 241 ? 10497 : 33071; }
static int m3g__map_mag(int v) { return v == 210 ? 9728 : 9729; }
static int m3g__map_min(int level, int image) { (void)level; return image == 210 ? 9728 : 9729; }

static float m3g__clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

static m3g_result m3g__decode_scaled(m3g_vertex_array_data const *a, float scale, float const bias[3], int expc, float **out, int *out_n) {
    int vi, ci; float *v;
    if (a->component_count < expc) { m3g__set_err("component count too small"); return M3G_ERR_FORMAT; }
    *out_n = a->vertex_count * expc;
    v = (float *)m3g__xmalloc((size_t)(*out_n > 0 ? *out_n : 1) * sizeof(float));
    if (!v) return M3G_ERR_NOMEM;
    for (vi = 0; vi < a->vertex_count; ++vi) {
        for (ci = 0; ci < expc; ++ci) {
            int src = vi * a->component_count + ci;
            float b = ci < 3 ? bias[ci] : 0.f;
            v[vi * expc + ci] = a->components[src] * scale + b;
        }
    }
    *out = v; return M3G_OK;
}
static m3g_result m3g__decode_normals(m3g_vertex_array_data const *a, float **out, int *out_n) {
    float div; int vi, ci; float *v;
    if (a->component_count < 3) return M3G_ERR_FORMAT;
    if (a->component_size == 1) div = 127.f;
    else if (a->component_size == 2) div = 32767.f;
    else return M3G_ERR_FORMAT;
    *out_n = a->vertex_count * 3;
    v = (float *)m3g__xmalloc((size_t)(*out_n > 0 ? *out_n : 1) * sizeof(float));
    if (!v) return M3G_ERR_NOMEM;
    for (vi = 0; vi < a->vertex_count; ++vi)
        for (ci = 0; ci < 3; ++ci) {
            float x = a->components[vi * a->component_count + ci] / div;
            v[vi * 3 + ci] = m3g__clampf(x, -1.f, 1.f);
        }
    *out = v; return M3G_OK;
}
static m3g_result m3g__decode_colors(m3g_vertex_array_data const *a, float **out, int *out_n) {
    int i; float *v;
    if (a->component_count != 3 && a->component_count != 4) return M3G_ERR_FORMAT;
    *out_n = a->vertex_count * a->component_count;
    v = (float *)m3g__xmalloc((size_t)(*out_n > 0 ? *out_n : 1) * sizeof(float));
    if (!v) return M3G_ERR_NOMEM;
    for (i = 0; i < *out_n; ++i) v[i] = (a->components[i] & 0xFF) / 255.f;
    *out = v; return M3G_OK;
}
static m3g_result m3g__decode_uv(m3g_vertex_array_data const *a, m3g_tex_coord_binding const *b, float **out, int *out_n) {
    int vi; float *v;
    if (a->component_count < 2) return M3G_ERR_FORMAT;
    *out_n = a->vertex_count * 2;
    v = (float *)m3g__xmalloc((size_t)(*out_n > 0 ? *out_n : 1) * sizeof(float));
    if (!v) return M3G_ERR_NOMEM;
    for (vi = 0; vi < a->vertex_count; ++vi) {
        int u_i = vi * a->component_count;
        v[vi*2]   = a->components[u_i] * b->scale + b->bias[0];
        v[vi*2+1] = a->components[u_i+1] * b->scale + b->bias[1];
    }
    *out = v; return M3G_OK;
}
static m3g_result m3g__expand_strips(m3g_triangle_strip_data const *ib, int **out, int *out_n) {
    int *tri = NULL; int n = 0, cap = 0, cursor = 0, s, si;
    for (s = 0; s < ib->strip_count; ++s) {
        int sl = ib->strip_lengths[s];
        if (cursor + sl > ib->index_count) { m3g__set_err("strip overflow"); M3G_FREE(tri); return M3G_ERR_FORMAT; }
        for (si = 2; si < sl; ++si) {
            int a = ib->indices[cursor + si - 2];
            int b = ib->indices[cursor + si - 1];
            int c = ib->indices[cursor + si];
            if (a == b || b == c || a == c) continue;
            if (n + 3 > cap) {
                int ncap = cap ? cap * 2 : 64;
                int *na = (int *)m3g__xrealloc(tri, (size_t)ncap * sizeof(int));
                if (!na) { M3G_FREE(tri); return M3G_ERR_NOMEM; }
                tri = na; cap = ncap;
            }
            if (si % 2 == 0) { tri[n++]=a; tri[n++]=b; tri[n++]=c; }
            else { tri[n++]=b; tri[n++]=a; tri[n++]=c; }
        }
        cursor += sl;
    }
    *out = tri; *out_n = n; return M3G_OK;
}

static int m3g__img_entry_size(int format) {
    switch (format) {
    case 96: case 97: return 1;
    case 98: return 2;
    case 99: return 3;
    case 100: return 4;
    default: return 0;
    }
}
static void m3g__write_px(uint8_t const *src, int so, uint8_t *dst, int to, int format) {
    switch (format) {
    case 96: dst[to]=255; dst[to+1]=255; dst[to+2]=255; dst[to+3]=src[so]; break;
    case 97: dst[to]=src[so]; dst[to+1]=src[so]; dst[to+2]=src[so]; dst[to+3]=255; break;
    case 98: dst[to]=src[so]; dst[to+1]=src[so]; dst[to+2]=src[so]; dst[to+3]=src[so+1]; break;
    case 99: dst[to]=src[so]; dst[to+1]=src[so+1]; dst[to+2]=src[so+2]; dst[to+3]=255; break;
    case 100: dst[to]=src[so]; dst[to+1]=src[so+1]; dst[to+2]=src[so+2]; dst[to+3]=src[so+3]; break;
    }
}
static int m3g__looks_png_jpeg(uint8_t const *b, int n) {
    if (n >= 8 && b[0]==0x89 && b[1]==0x50 && b[2]==0x4e && b[3]==0x47) return 1;
    if (n >= 3 && b[0]==0xff && b[1]==0xd8 && b[2]==0xff) return 1;
    return 0;
}
static int m3g__looks_zlib(uint8_t const *b, int n) {
    return n >= 2 && b[0]==0x78 && (b[1]==0x01||b[1]==0x5e||b[1]==0x9c||b[1]==0xda);
}

static m3g_result m3g__decode_image_rgba(m3g_image2d_data const *img, uint8_t **out, int *out_len, int *ow, int *oh) {
    uint8_t const *px; int pxlen; int es, pc, expected, i;
    uint8_t *rgba; uint8_t *inflated = NULL;
    if (!img->has_pixels || !img->pixels) return M3G_ERR_FORMAT;
    px = img->pixels; pxlen = img->pixels_len;
    *ow = img->width; *oh = img->height;

    if (m3g__looks_png_jpeg(px, pxlen)) {
        int w=0,h=0,n=0; unsigned char *data = m3g_image_load_memory(px, pxlen, &w, &h, &n, 4);
        if (data && w > 0 && h > 0) {
            size_t L = (size_t)w * (size_t)h * 4u;
            rgba = (uint8_t *)m3g__xmalloc(L);
            if (!rgba) { m3g_image_free_pixels(data); return M3G_ERR_NOMEM; }
            memcpy(rgba, data, L); m3g_image_free_pixels(data);
            *out = rgba; *out_len = (int)L; *ow = w; *oh = h; return M3G_OK;
        }
        if (data) m3g_image_free_pixels(data);
    }
    es = m3g__img_entry_size(img->format);
    if (!es) return M3G_ERR_FORMAT;
    pc = img->width * img->height;
    expected = img->palette_len > 0 ? pc : pc * es;
    if (pxlen != expected || m3g__looks_zlib(px, pxlen)) {
        size_t dest_len = (size_t)expected;
        int ok = 0;
        if (expected > 0) {
            inflated = (uint8_t *)m3g__xmalloc(dest_len);
            if (inflated) {
                if (m3g_deflate_uncompress(inflated, &dest_len, px, (size_t)pxlen) == M3G_DEFLATE_OK && dest_len == (size_t)expected) {
                    px = inflated; pxlen = (int)dest_len; ok = 1;
                } else { M3G_FREE(inflated); inflated = NULL; }
            }
        }
        if (!ok) {
            size_t ol = 0; void *heap = m3g_deflate_uncompress_to_heap(px, (size_t)pxlen, &ol, 1);
            if (!heap) heap = m3g_deflate_uncompress_to_heap(px, (size_t)pxlen, &ol, 0);
            if (heap) {
                inflated = (uint8_t *)m3g__xmalloc(ol ? ol : 1);
                if (!inflated) { m3g_deflate_free(heap); return M3G_ERR_NOMEM; }
                memcpy(inflated, heap, ol); m3g_deflate_free(heap);
                px = inflated; pxlen = (int)ol; ok = 1;
            }
        }
        if (!ok && pxlen != expected) {
            int w=0,h=0,n=0; unsigned char *data = m3g_image_load_memory(img->pixels, img->pixels_len, &w, &h, &n, 4);
            if (data && w>0 && h>0) {
                size_t L = (size_t)w*(size_t)h*4u;
                rgba = (uint8_t *)m3g__xmalloc(L);
                if (!rgba) { m3g_image_free_pixels(data); return M3G_ERR_NOMEM; }
                memcpy(rgba, data, L); m3g_image_free_pixels(data);
                *out = rgba; *out_len = (int)L; *ow=w; *oh=h; return M3G_OK;
            }
            if (data) m3g_image_free_pixels(data);
            return M3G_ERR_FORMAT;
        }
    }
    rgba = (uint8_t *)m3g__xmalloc((size_t)pc * 4u);
    if (!rgba) { M3G_FREE(inflated); return M3G_ERR_NOMEM; }
    if (img->palette_len > 0) {
        if (pxlen != pc) { M3G_FREE(rgba); M3G_FREE(inflated); return M3G_ERR_FORMAT; }
        for (i = 0; i < pc; ++i) {
            int pi = px[i] & 0xFF; int po = pi * es;
            if (po + es > img->palette_len) { M3G_FREE(rgba); M3G_FREE(inflated); return M3G_ERR_FORMAT; }
            m3g__write_px(img->palette, po, rgba, i * 4, img->format);
        }
    } else {
        if (pxlen != pc * es) { M3G_FREE(rgba); M3G_FREE(inflated); return M3G_ERR_FORMAT; }
        for (i = 0; i < pc; ++i) m3g__write_px(px, i * es, rgba, i * 4, img->format);
    }
    M3G_FREE(inflated);
    *out = rgba; *out_len = pc * 4; return M3G_OK;
}

/* dirname helper */
static void m3g__join_parent(char const *base, char const *rel, char *out, size_t n) {
    char tmp[1024]; size_t i, last = 0;
    if (!base || !base[0]) { snprintf(out, n, "%s", rel ? rel : ""); return; }
    strncpy(tmp, base, sizeof(tmp)-1); tmp[sizeof(tmp)-1]=0;
    for (i = 0; tmp[i]; ++i) if (tmp[i]=='/' || tmp[i]=='\\') last = i;
    if (last == 0 && (tmp[0]!='/' && tmp[0]!='\\')) { snprintf(out, n, "%s", rel ? rel : ""); return; }
    tmp[last] = 0;
    snprintf(out, n, "%s/%s", tmp, rel ? rel : "");
}

static m3g_result m3g__build_image(m3g__builder *b, int image_id, int *out_idx);
static m3g_result m3g__build_texture(m3g__builder *b, int texture_id, int *out_idx);
static m3g_result m3g__build_material(m3g__builder *b, int appearance_id, int has_vc, int *out_idx);
static m3g_result m3g__build_mesh(m3g__builder *b, m3g_object *mesh_obj, int *out_idx);
static m3g_result m3g__build_camera(m3g__builder *b, m3g_object *cam_obj, int *out_idx, int *has);
static m3g_result m3g__build_node(m3g__builder *b, int object_id, int *out_idx, int *has);

static m3g_result m3g__build_image(m3g__builder *b, int image_id, int *out_idx) {
    int found = 0, v = -1; m3g_object *o; m3g_scene_image img; char name[128]; m3g_result rc;
    m3g_scene_image *na;
    if (image_id <= 0) { *out_idx = -1; return M3G_OK; }
    m3g__map_get(b->img_map, b->img_map_cap, image_id, &v, &found);
    if (found) { *out_idx = v; return M3G_OK; }
    o = m3g__obj(b->file, image_id);
    memset(&img, 0, sizeof(img));
    if (o && o->object_type == M3G_OBJ_IMAGE_2D) {
        int w=0,h=0,len=0; uint8_t *px=NULL;
        rc = m3g__decode_image_rgba(&o->u.image2d, &px, &len, &w, &h);
        if (rc) { m3g__warn(b, "image-format", "Embedded image uses unsupported format."); *out_idx = -1; m3g__map_set(&b->img_map, &b->img_map_cap, image_id, -1); return M3G_OK; }
        m3g__synthetic_name(o, name, sizeof(name));
        img.name = m3g__strdup(name);
        img.object_id = o->object_id;
        img.is_embedded = 1; img.width = w; img.height = h; img.pixels = px; img.pixels_len = len;
    } else if (o && o->object_type == M3G_OBJ_EXTERNAL_REFERENCE) {
        char path[1024];
        m3g__synthetic_name(o, name, sizeof(name));
        img.name = m3g__strdup(name);
        img.object_id = o->object_id;
        m3g__join_parent(b->input_path, o->u.external.uri, path, sizeof(path));
        img.source_path = m3g__strdup(path);
    } else {
        m3g__warn(b, "image-ref", "Texture image reference points to unsupported object.");
        *out_idx = -1; m3g__map_set(&b->img_map, &b->img_map_cap, image_id, -1); return M3G_OK;
    }
    *out_idx = b->scene.image_count;
    na = (m3g_scene_image *)m3g__xrealloc(b->scene.images, (size_t)(b->scene.image_count + 1) * sizeof(m3g_scene_image));
    if (!na) { M3G_FREE(img.name); M3G_FREE(img.pixels); M3G_FREE(img.source_path); return M3G_ERR_NOMEM; }
    b->scene.images = na;
    b->scene.images[b->scene.image_count++] = img;
    return m3g__map_set(&b->img_map, &b->img_map_cap, image_id, *out_idx);
}

static m3g_result m3g__build_texture(m3g__builder *b, int texture_id, int *out_idx) {
    int found=0,v=-1, img_i=-1, samp_i=-1, i; m3g_object *o; m3g_scene_texture tex; char name[128];
    m3g_result rc; m3g_scene_texture *na;
    int mag, minf, ws, wt;
    if (texture_id <= 0) { *out_idx = -1; return M3G_OK; }
    m3g__map_get(b->tex_map, b->tex_map_cap, texture_id, &v, &found);
    if (found) { *out_idx = v; return M3G_OK; }
    o = m3g__obj(b->file, texture_id);
    if (!o || o->object_type != M3G_OBJ_TEXTURE_2D) {
        m3g__warn(b, "texture", "Referenced texture missing"); *out_idx = -1;
        m3g__map_set(&b->tex_map, &b->tex_map_cap, texture_id, -1); return M3G_OK;
    }
    rc = m3g__build_image(b, o->u.texture2d.image_id, &img_i); if (rc) return rc;
    if (img_i < 0) { *out_idx = -1; m3g__map_set(&b->tex_map, &b->tex_map_cap, texture_id, -1); return M3G_OK; }
    mag = m3g__map_mag(o->u.texture2d.image_filter);
    minf = m3g__map_min(o->u.texture2d.level_filter, o->u.texture2d.image_filter);
    ws = m3g__map_wrap(o->u.texture2d.wrapping_s);
    wt = m3g__map_wrap(o->u.texture2d.wrapping_t);
    for (i = 0; i < b->scene.sampler_count; ++i) {
        m3g_scene_sampler *s = &b->scene.samplers[i];
        if (s->mag_filter == mag && s->min_filter == minf && s->wrap_s == ws && s->wrap_t == wt) { samp_i = i; break; }
    }
    if (samp_i < 0) {
        m3g_scene_sampler samp; m3g_scene_sampler *sna;
        samp.mag_filter = mag; samp.min_filter = minf; samp.wrap_s = ws; samp.wrap_t = wt;
        samp_i = b->scene.sampler_count;
        sna = (m3g_scene_sampler *)m3g__xrealloc(b->scene.samplers, (size_t)(b->scene.sampler_count+1)*sizeof(m3g_scene_sampler));
        if (!sna) return M3G_ERR_NOMEM;
        b->scene.samplers = sna; b->scene.samplers[b->scene.sampler_count++] = samp;
    }
    memset(&tex, 0, sizeof(tex));
    m3g__synthetic_name(o, name, sizeof(name));
    tex.name = m3g__strdup(name);
    tex.image_index = img_i; tex.sampler_index = samp_i;
    *out_idx = b->scene.texture_count;
    na = (m3g_scene_texture *)m3g__xrealloc(b->scene.textures, (size_t)(b->scene.texture_count+1)*sizeof(m3g_scene_texture));
    if (!na) { M3G_FREE(tex.name); return M3G_ERR_NOMEM; }
    b->scene.textures = na; b->scene.textures[b->scene.texture_count++] = tex;
    return m3g__map_set(&b->tex_map, &b->tex_map_cap, texture_id, *out_idx);
}

static m3g_result m3g__build_material(m3g__builder *b, int appearance_id, int has_vc, int *out_idx) {
    int found=0,v=-1, tex_i=-1; m3g_object *app, *mat=NULL, *poly=NULL; m3g_scene_material m; char name[128];
    m3g_result rc; m3g_scene_material *na;
    if (appearance_id <= 0) { *out_idx = -1; return M3G_OK; }
    m3g__map_get(b->mat_map, b->mat_map_cap, appearance_id, &v, &found);
    if (found) { *out_idx = v; return M3G_OK; }
    app = m3g__obj(b->file, appearance_id);
    if (!app || app->object_type != M3G_OBJ_APPEARANCE) {
        m3g__warn(b, "appearance", "Referenced appearance missing"); *out_idx = -1;
        m3g__map_set(&b->mat_map, &b->mat_map_cap, appearance_id, -1); return M3G_OK;
    }
    if (app->u.appearance.compositing_mode_id) m3g__warn(b, "compositing-mode", "CompositingMode is not exported in v1.");
    if (app->u.appearance.fog_id) m3g__warn(b, "appearance-fog", "Appearance fog references are not exported in v1.");
    if (app->u.appearance.texture_count > 1) m3g__warn(b, "multi-texture", "Only texture unit 0 is exported in v1.");
    if (app->u.appearance.material_id) mat = m3g__obj(b->file, app->u.appearance.material_id);
    if (app->u.appearance.polygon_mode_id) poly = m3g__obj(b->file, app->u.appearance.polygon_mode_id);
    if (app->u.appearance.texture_count > 0) {
        rc = m3g__build_texture(b, app->u.appearance.texture_ids[0], &tex_i); if (rc) return rc;
    }
    memset(&m, 0, sizeof(m));
    m.base_color_factor[0]=m.base_color_factor[1]=m.base_color_factor[2]=m.base_color_factor[3]=1.f;
    m.roughness_factor = 1.f; m.base_color_texture_index = -1;
    if (mat && mat->object_type == M3G_OBJ_MATERIAL) {
        m.base_color_factor[0] = mat->u.material.diffuse_color.red / 255.f;
        m.base_color_factor[1] = mat->u.material.diffuse_color.green / 255.f;
        m.base_color_factor[2] = mat->u.material.diffuse_color.blue / 255.f;
        m.base_color_factor[3] = mat->u.material.diffuse_color.alpha / 255.f;
        if (has_vc && mat->u.material.vertex_color_tracking_enabled) {
            m.base_color_factor[0]=m.base_color_factor[1]=m.base_color_factor[2]=1.f;
        }
        m.emissive_factor[0] = mat->u.material.emissive_color.red / 255.f;
        m.emissive_factor[1] = mat->u.material.emissive_color.green / 255.f;
        m.emissive_factor[2] = mat->u.material.emissive_color.blue / 255.f;
        m.roughness_factor = m3g__clampf(1.f - (mat->u.material.shininess / 128.f), 0.f, 1.f);
    }
    m.base_color_texture_index = tex_i;
    m.double_sided = (poly && poly->object_type == M3G_OBJ_POLYGON_MODE && poly->u.polygon_mode.culling == 162);
    if (m.base_color_factor[3] < 0.999f) m.alpha_mode = m3g__strdup("BLEND");
    if (app->u.appearance.object3d.user_id)
        snprintf(name, sizeof(name), "u%d", app->u.appearance.object3d.user_id);
    else m3g__synthetic_name(app, name, sizeof(name));
    m.name = m3g__strdup(name);
    *out_idx = b->scene.material_count;
    na = (m3g_scene_material *)m3g__xrealloc(b->scene.materials, (size_t)(b->scene.material_count+1)*sizeof(m3g_scene_material));
    if (!na) { M3G_FREE(m.name); M3G_FREE(m.alpha_mode); return M3G_ERR_NOMEM; }
    b->scene.materials = na; b->scene.materials[b->scene.material_count++] = m;
    return m3g__map_set(&b->mat_map, &b->mat_map_cap, appearance_id, *out_idx);
}

static m3g_result m3g__build_mesh(m3g__builder *b, m3g_object *mesh_obj, int *out_idx) {
    int found=0,v=-1, sm; m3g_object *vb, *pos_a, *n_a=NULL, *c_a=NULL;
    float *positions=NULL, *normals=NULL, *colors=NULL, *uvs=NULL;
    int pn=0, nn=0, cn=0, un=0; m3g_scene_mesh mesh; char name[128]; m3g_result rc;
    m3g_scene_mesh *na;
    m3g__map_get(b->mesh_map, b->mesh_map_cap, mesh_obj->object_id, &v, &found);
    if (found) { *out_idx = v; return M3G_OK; }
    vb = m3g__obj(b->file, mesh_obj->u.mesh.vertex_buffer_id);
    if (!vb || vb->object_type != M3G_OBJ_VERTEX_BUFFER) { m3g__set_err("missing vertex buffer"); return M3G_ERR_FORMAT; }
    pos_a = m3g__obj(b->file, vb->u.vertex_buffer.positions_id);
    if (!pos_a || pos_a->object_type != M3G_OBJ_VERTEX_ARRAY) { m3g__set_err("missing positions"); return M3G_ERR_FORMAT; }
    rc = m3g__decode_scaled(&pos_a->u.vertex_array, vb->u.vertex_buffer.position_scale, vb->u.vertex_buffer.position_bias, 3, &positions, &pn); if (rc) return rc;
    if (vb->u.vertex_buffer.normals_id) {
        n_a = m3g__obj(b->file, vb->u.vertex_buffer.normals_id);
        if (n_a && n_a->object_type == M3G_OBJ_VERTEX_ARRAY)
            m3g__decode_normals(&n_a->u.vertex_array, &normals, &nn);
    }
    if (vb->u.vertex_buffer.colors_id) {
        c_a = m3g__obj(b->file, vb->u.vertex_buffer.colors_id);
        if (c_a && c_a->object_type == M3G_OBJ_VERTEX_ARRAY)
            m3g__decode_colors(&c_a->u.vertex_array, &colors, &cn);
    }
    if (vb->u.vertex_buffer.tex_coord_binding_count > 0) {
        m3g_tex_coord_binding *tb = &vb->u.vertex_buffer.tex_coord_bindings[0];
        m3g_object *ua = m3g__obj(b->file, tb->vertex_array_id);
        if (vb->u.vertex_buffer.tex_coord_binding_count > 1)
            m3g__warn(b, "multi-uv", "Only the first texture coordinate set is exported in v1.");
        if (ua && ua->object_type == M3G_OBJ_VERTEX_ARRAY)
            m3g__decode_uv(&ua->u.vertex_array, tb, &uvs, &un);
        else m3g__warn(b, "uv-array", "Texture coordinates reference missing vertex array.");
    }
    memset(&mesh, 0, sizeof(mesh));
    m3g__synthetic_name(mesh_obj, name, sizeof(name));
    mesh.name = m3g__strdup(name);
    for (sm = 0; sm < mesh_obj->u.mesh.submesh_count; ++sm) {
        m3g_submesh_ref *sub = &mesh_obj->u.mesh.submeshes[sm];
        m3g_object *ib = m3g__obj(b->file, sub->index_buffer_id);
        m3g_scene_primitive prim; m3g_scene_primitive *pna; int mat_i = -1;
        char pname[160];
        memset(&prim, 0, sizeof(prim));
        if (!ib || ib->object_type != M3G_OBJ_TRIANGLE_STRIP_ARRAY) {
            m3g__warn(b, "index-buffer", "Submesh uses an unsupported index buffer.");
            continue;
        }
        snprintf(pname, sizeof(pname), "%s_Primitive_%d", name, sm);
        prim.name = m3g__strdup(pname);
        prim.positions = (float *)m3g__xmalloc((size_t)(pn > 0 ? pn : 1) * sizeof(float));
        if (!prim.positions) { M3G_FREE(prim.name); rc = M3G_ERR_NOMEM; goto mesh_fail; }
        memcpy(prim.positions, positions, (size_t)pn * sizeof(float)); prim.position_count = pn;
        if (normals) {
            prim.normals = (float *)m3g__xmalloc((size_t)nn * sizeof(float));
            if (!prim.normals) { m3g__free_primitive(&prim); rc = M3G_ERR_NOMEM; goto mesh_fail; }
            memcpy(prim.normals, normals, (size_t)nn * sizeof(float)); prim.normal_count = nn;
        }
        if (uvs) {
            prim.tex_coords0 = (float *)m3g__xmalloc((size_t)un * sizeof(float));
            if (!prim.tex_coords0) { m3g__free_primitive(&prim); rc = M3G_ERR_NOMEM; goto mesh_fail; }
            memcpy(prim.tex_coords0, uvs, (size_t)un * sizeof(float)); prim.tex_coord_count = un;
        }
        if (colors) {
            prim.vertex_colors = (float *)m3g__xmalloc((size_t)cn * sizeof(float));
            if (!prim.vertex_colors) { m3g__free_primitive(&prim); rc = M3G_ERR_NOMEM; goto mesh_fail; }
            memcpy(prim.vertex_colors, colors, (size_t)cn * sizeof(float)); prim.vertex_color_count = cn;
        }
        rc = m3g__expand_strips(&ib->u.triangle_strip, &prim.indices, &prim.index_count);
        if (rc) { m3g__free_primitive(&prim); goto mesh_fail; }
        rc = m3g__build_material(b, sub->appearance_id, colors != NULL, &mat_i); if (rc) { m3g__free_primitive(&prim); goto mesh_fail; }
        prim.material_index = mat_i;
        pna = (m3g_scene_primitive *)m3g__xrealloc(mesh.primitives, (size_t)(mesh.primitive_count+1)*sizeof(m3g_scene_primitive));
        if (!pna) { m3g__free_primitive(&prim); rc = M3G_ERR_NOMEM; goto mesh_fail; }
        mesh.primitives = pna; mesh.primitives[mesh.primitive_count++] = prim;
    }
    M3G_FREE(positions); M3G_FREE(normals); M3G_FREE(colors); M3G_FREE(uvs);
    *out_idx = b->scene.mesh_count;
    na = (m3g_scene_mesh *)m3g__xrealloc(b->scene.meshes, (size_t)(b->scene.mesh_count+1)*sizeof(m3g_scene_mesh));
    if (!na) { m3g__free_mesh_ir(&mesh); return M3G_ERR_NOMEM; }
    b->scene.meshes = na; b->scene.meshes[b->scene.mesh_count++] = mesh;
    return m3g__map_set(&b->mesh_map, &b->mesh_map_cap, mesh_obj->object_id, *out_idx);
mesh_fail:
    M3G_FREE(positions); M3G_FREE(normals); M3G_FREE(colors); M3G_FREE(uvs);
    m3g__free_mesh_ir(&mesh);
    return rc;
}

static m3g_result m3g__build_camera(m3g__builder *b, m3g_object *cam_obj, int *out_idx, int *has) {
    int found=0,v=-1; m3g_scene_camera cam; char name[128]; m3g_scene_camera *na;
    *has = 0;
    m3g__map_get(b->cam_map, b->cam_map_cap, cam_obj->object_id, &v, &found);
    if (found) { *out_idx = v; *has = (v >= 0); return M3G_OK; }
    if (!cam_obj->u.camera.has_perspective) {
        m3g__warn(b, "camera", "Camera uses unsupported projection; transform only.");
        m3g__map_set(&b->cam_map, &b->cam_map_cap, cam_obj->object_id, -1);
        *out_idx = -1; return M3G_OK;
    }
    memset(&cam, 0, sizeof(cam));
    m3g__synthetic_name(cam_obj, name, sizeof(name));
    cam.name = m3g__strdup(name);
    cam.has_perspective = 1;
    cam.yfov_radians = cam_obj->u.camera.fov_degrees / 180.f * (float)M3G_PI;
    cam.aspect_ratio = cam_obj->u.camera.aspect_ratio > 0.f ? cam_obj->u.camera.aspect_ratio : 0.f;
    cam.znear = cam_obj->u.camera.near_distance > 0.0001f ? cam_obj->u.camera.near_distance : 0.0001f;
    cam.zfar = cam_obj->u.camera.far_distance > 0.f ? cam_obj->u.camera.far_distance : 0.f;
    *out_idx = b->scene.camera_count; *has = 1;
    na = (m3g_scene_camera *)m3g__xrealloc(b->scene.cameras, (size_t)(b->scene.camera_count+1)*sizeof(m3g_scene_camera));
    if (!na) { M3G_FREE(cam.name); return M3G_ERR_NOMEM; }
    b->scene.cameras = na; b->scene.cameras[b->scene.camera_count++] = cam;
    return m3g__map_set(&b->cam_map, &b->cam_map_cap, cam_obj->object_id, *out_idx);
}

static m3g_result m3g__register_node(m3g__builder *b, m3g_object *obj, int mesh_i, int cam_i, int *out_idx) {
    int found=0,v=-1; m3g_node_meta *nm; m3g_scene_node node; char name[128]; float M[16];
    m3g_scene_node *na;
    m3g__map_get(b->node_map, b->node_map_cap, obj->object_id, &v, &found);
    if (found) { *out_idx = v; return M3G_OK; }
    nm = m3g__node_meta_of(obj);
    if (nm && nm->alignment.present) m3g__warn(b, "alignment", "Node alignment is present but not exported in v1.");
    memset(&node, 0, sizeof(node));
    m3g__synthetic_name(obj, name, sizeof(name));
    node.name = m3g__strdup(name);
    node.mesh_index = mesh_i; node.camera_index = cam_i;
    if (nm) {
        m3g_node_matrix(nm, M);
        if (!m3g_mat4_is_identity(M, 1e-5f)) {
            memcpy(node.matrix, M, sizeof(M));
            node.has_matrix = 1;
        }
    }
    *out_idx = b->scene.node_count;
    na = (m3g_scene_node *)m3g__xrealloc(b->scene.nodes, (size_t)(b->scene.node_count+1)*sizeof(m3g_scene_node));
    if (!na) { M3G_FREE(node.name); return M3G_ERR_NOMEM; }
    b->scene.nodes = na; b->scene.nodes[b->scene.node_count++] = node;
    return m3g__map_set(&b->node_map, &b->node_map_cap, obj->object_id, *out_idx);
}

static m3g_result m3g__build_node(m3g__builder *b, int object_id, int *out_idx, int *has) {
    int found=0,v=-1; m3g_object *o; m3g_result rc; int mesh_i=-1, cam_i=-1, chas=0, i;
    *has = 0;
    m3g__map_get(b->node_map, b->node_map_cap, object_id, &v, &found);
    if (found) { *out_idx = v; *has = 1; return M3G_OK; }
    o = m3g__obj(b->file, object_id);
    if (!o) { m3g__set_err("missing object %d", object_id); return M3G_ERR_FORMAT; }
    if (o->object_type == M3G_OBJ_GROUP || o->object_type == M3G_OBJ_WORLD) {
        int extra = (o->object_type == M3G_OBJ_WORLD) ? o->u.group.active_camera_id : 0;
        rc = m3g__register_node(b, o, -1, -1, out_idx); if (rc) return rc; *has = 1;
        for (i = 0; i < o->u.group.child_count; ++i) {
            int ci=0, ch=0;
            rc = m3g__build_node(b, o->u.group.child_ids[i], &ci, &ch); if (rc) return rc;
            if (ch) {
                int *na = (int *)m3g__xrealloc(b->scene.nodes[*out_idx].children,
                    (size_t)(b->scene.nodes[*out_idx].child_count + 1) * sizeof(int));
                if (!na) return M3G_ERR_NOMEM;
                b->scene.nodes[*out_idx].children = na;
                b->scene.nodes[*out_idx].children[b->scene.nodes[*out_idx].child_count++] = ci;
            }
        }
        if (extra) {
            int already = 0;
            for (i = 0; i < o->u.group.child_count; ++i) if (o->u.group.child_ids[i] == extra) already = 1;
            if (!already) {
                int ci=0, ch=0;
                rc = m3g__build_node(b, extra, &ci, &ch); if (rc) return rc;
                if (ch) {
                    int *na = (int *)m3g__xrealloc(b->scene.nodes[*out_idx].children,
                        (size_t)(b->scene.nodes[*out_idx].child_count + 1) * sizeof(int));
                    if (!na) return M3G_ERR_NOMEM;
                    b->scene.nodes[*out_idx].children = na;
                    b->scene.nodes[*out_idx].children[b->scene.nodes[*out_idx].child_count++] = ci;
                }
            }
        }
        return M3G_OK;
    }
    if (o->object_type == M3G_OBJ_MESH || o->object_type == M3G_OBJ_SKINNED_MESH) {
        if (o->object_type == M3G_OBJ_SKINNED_MESH && o->u.mesh.bone_transform_count > 0)
            m3g__warn(b, "skinning", "Skinned meshes are exported as static meshes in v1.");
        rc = m3g__build_mesh(b, o, &mesh_i); if (rc) return rc;
        rc = m3g__register_node(b, o, mesh_i, -1, out_idx); if (rc) return rc; *has = 1; return M3G_OK;
    }
    if (o->object_type == M3G_OBJ_CAMERA) {
        rc = m3g__build_camera(b, o, &cam_i, &chas); if (rc) return rc;
        rc = m3g__register_node(b, o, -1, chas ? cam_i : -1, out_idx); if (rc) return rc; *has = 1; return M3G_OK;
    }
    if (o->object_type == M3G_OBJ_LIGHT) {
        rc = m3g__register_node(b, o, -1, -1, out_idx); if (rc) return rc; *has = 1; return M3G_OK;
    }
    m3g__warn(b, "unsupported-node", "Unsupported node object skipped.");
    return M3G_OK;
}

static m3g_result m3g__find_roots(m3g__builder *b, int **roots, int *nroots) {
    int *node_ids = NULL; int nn = 0, nc = 0, i, j;
    int *child_refs = NULL; int crn = 0, crc = 0;
    int *top = NULL; int tn = 0; m3g_object *world = NULL;
    for (i = 1; i <= b->file->object_count; ++i) {
        m3g_object *o = m3g__obj(b->file, i);
        if (o && m3g__is_node_type(o->object_type)) {
            if (nn + 1 > nc) {
                int ncap = nc ? nc * 2 : 16;
                int *na = (int *)m3g__xrealloc(node_ids, (size_t)ncap * sizeof(int));
                if (!na) { M3G_FREE(node_ids); return M3G_ERR_NOMEM; }
                node_ids = na; nc = ncap;
            }
            node_ids[nn++] = i;
            if (o->object_type == M3G_OBJ_WORLD) world = o;
        }
        if (o && (o->object_type == M3G_OBJ_GROUP || o->object_type == M3G_OBJ_WORLD)) {
            for (j = 0; j < o->u.group.child_count; ++j) {
                if (crn + 1 > crc) {
                    int ncap = crc ? crc * 2 : 16;
                    int *na = (int *)m3g__xrealloc(child_refs, (size_t)ncap * sizeof(int));
                    if (!na) { M3G_FREE(node_ids); M3G_FREE(child_refs); return M3G_ERR_NOMEM; }
                    child_refs = na; crc = ncap;
                }
                child_refs[crn++] = o->u.group.child_ids[j];
            }
            if (o->object_type == M3G_OBJ_WORLD && o->u.group.active_camera_id) {
                if (crn + 1 > crc) {
                    int ncap = crc ? crc * 2 : 16;
                    int *na = (int *)m3g__xrealloc(child_refs, (size_t)ncap * sizeof(int));
                    if (!na) { M3G_FREE(node_ids); M3G_FREE(child_refs); return M3G_ERR_NOMEM; }
                    child_refs = na; crc = ncap;
                }
                child_refs[crn++] = o->u.group.active_camera_id;
            }
        }
    }
    for (i = 0; i < nn; ++i) {
        int id = node_ids[i], is_child = 0;
        for (j = 0; j < crn; ++j) if (child_refs[j] == id) { is_child = 1; break; }
        if (!is_child) {
            int *na = (int *)m3g__xrealloc(top, (size_t)(tn + 1) * sizeof(int));
            if (!na) { M3G_FREE(node_ids); M3G_FREE(child_refs); M3G_FREE(top); return M3G_ERR_NOMEM; }
            top = na; top[tn++] = id;
        }
    }
    M3G_FREE(node_ids); M3G_FREE(child_refs);
    if (tn == 0 && world) {
        top = (int *)m3g__xmalloc(sizeof(int));
        if (!top) return M3G_ERR_NOMEM;
        top[0] = world->object_id; tn = 1;
    }
    if (world) {
        int *roots2 = (int *)m3g__xmalloc(sizeof(int) * (size_t)(tn + 1));
        int rn = 0, k;
        if (!roots2) { M3G_FREE(top); return M3G_ERR_NOMEM; }
        roots2[rn++] = world->object_id;
        for (k = 0; k < tn; ++k) if (top[k] != world->object_id) {
            roots2[rn++] = top[k];
            m3g__warn(b, "top-level-nodes", "Node roots exist outside the World object.");
        }
        M3G_FREE(top); *roots = roots2; *nroots = rn; return M3G_OK;
    }
    m3g__warn(b, "no-world", "No World object found; exporting top-level node hierarchy.");
    *roots = top; *nroots = tn; return M3G_OK;
}

static m3g_result m3g__build_scene(m3g_file const *file, char const *input_path, m3g_scene_ir *out) {
    m3g__builder b; int *roots = NULL; int nroots = 0, i; m3g_result rc;
    memset(&b, 0, sizeof(b));
    b.file = file; b.input_path = input_path ? input_path : "";
    /* global warnings */
    for (i = 1; i <= file->object_count; ++i) {
        m3g_object *o = m3g__obj(file, i);
        if (!o) continue;
        if (o->object_type == M3G_OBJ_MORPHING_MESH)
            m3g__warn(&b, "morphing", "MorphingMesh objects are not supported in v1 and will be skipped.");
        if (o->object_type == M3G_OBJ_LIGHT)
            m3g__warn(&b, "lights", "Lights are not exported in v1; light nodes are emitted without glTF light payloads.");
        if (o->object_type == M3G_OBJ_FOG)
            m3g__warn(&b, "fog", "Fog objects are not exported in v1.");
        if (o->object_type == M3G_OBJ_BACKGROUND && o->u.background.background_image_id)
            m3g__warn(&b, "background-image", "Background images are not exported in v1.");
    }
    rc = m3g__find_roots(&b, &roots, &nroots); if (rc) goto fail;
    if (nroots == 0) { m3g__set_err("No convertible node objects"); rc = M3G_ERR_FORMAT; goto fail; }
    for (i = 0; i < nroots; ++i) {
        int idx=0, has=0;
        rc = m3g__build_node(&b, roots[i], &idx, &has); if (rc) goto fail;
        if (has) {
            int *na = (int *)m3g__xrealloc(b.scene.root_node_indices, (size_t)(b.scene.root_count + 1) * sizeof(int));
            if (!na) { rc = M3G_ERR_NOMEM; goto fail; }
            b.scene.root_node_indices = na;
            b.scene.root_node_indices[b.scene.root_count++] = idx;
        }
    }
    if (b.scene.root_count == 0) { m3g__set_err("No convertible node objects"); rc = M3G_ERR_FORMAT; goto fail; }
    /* animations omitted for brevity parity-lite: still scan tracks for TRS */
    {
        /* simplified animation export */
        int ni;
        for (ni = 0; ni < b.node_map_cap; ++ni) {
            int oid, nidx, t;
            m3g_object *node_o; m3g_node_meta *nm;
            if (!b.node_map || !b.node_map[ni].used) continue;
            oid = b.node_map[ni].k; nidx = b.node_map[ni].v;
            node_o = m3g__obj(file, oid);
            nm = m3g__node_meta_of(node_o);
            if (!nm) continue;
            for (t = 0; t < nm->transformable.object3d.animation_track_count; ++t) {
                m3g_object *track = m3g__obj(file, nm->transformable.object3d.animation_track_ids[t]);
                m3g_object *seq, *ctrl = NULL;
                m3g_scene_animation anim; m3g_scene_anim_sampler samp; m3g_scene_anim_channel ch;
                char const *path = NULL; int comps = 3; int first, last, k, prop;
                m3g_scene_animation *ana;
                if (!track || track->object_type != M3G_OBJ_ANIMATION_TRACK) continue;
                prop = track->u.anim_track.property_id;
                if (prop == M3G_ANIM_TRANSLATION) path = "translation";
                else if (prop == M3G_ANIM_SCALE) path = "scale";
                else if (prop == M3G_ANIM_ORIENTATION) { path = "rotation"; comps = 4; }
                else continue;
                seq = m3g__obj(file, track->u.anim_track.keyframe_sequence_id);
                if (!seq || seq->object_type != M3G_OBJ_KEYFRAME_SEQUENCE) continue;
                if (track->u.anim_track.animation_controller_id)
                    ctrl = m3g__obj(file, track->u.anim_track.animation_controller_id);
                first = seq->u.keyframe_seq.valid_range_first;
                last = seq->u.keyframe_seq.valid_range_last;
                if (last < 0 || first < 0) { first = 0; last = seq->u.keyframe_seq.keyframe_count - 1; }
                if (first < 0) first = 0;
                if (last >= seq->u.keyframe_seq.keyframe_count) last = seq->u.keyframe_seq.keyframe_count - 1;
                if (last < first) continue;
                memset(&samp, 0, sizeof(samp));
                strncpy(samp.interpolation, seq->u.keyframe_seq.interpolation == M3G_KF_STEP ? "STEP" : "LINEAR", sizeof(samp.interpolation)-1);
                samp.component_count = comps;
                samp.time_count = last - first + 1;
                samp.times = (float *)m3g__xmalloc((size_t)samp.time_count * sizeof(float));
                samp.value_count = samp.time_count * comps;
                samp.values = (float *)m3g__xmalloc((size_t)samp.value_count * sizeof(float));
                if (!samp.times || !samp.values) { M3G_FREE(samp.times); M3G_FREE(samp.values); continue; }
                for (k = first; k <= last; ++k) {
                    m3g_keyframe *kf = &seq->u.keyframe_seq.keyframes[k];
                    float world = (float)kf->time;
                    int oi = k - first;
                    if (ctrl && ctrl->object_type == M3G_OBJ_ANIMATION_CONTROLLER && fabsf(ctrl->u.anim_controller.speed) > 1e-8f) {
                        world = (world - ctrl->u.anim_controller.reference_sequence_time) / ctrl->u.anim_controller.speed
                              + (float)ctrl->u.anim_controller.reference_world_time;
                    }
                    samp.times[oi] = world / 1000.f;
                    if (prop == M3G_ANIM_TRANSLATION) {
                        samp.values[oi*3+0] = kf->value_count>0?kf->values[0]:0;
                        samp.values[oi*3+1] = kf->value_count>1?kf->values[1]:0;
                        samp.values[oi*3+2] = kf->value_count>2?kf->values[2]:0;
                    } else if (prop == M3G_ANIM_SCALE) {
                        if (seq->u.keyframe_seq.component_count == 1) {
                            float s = kf->value_count>0?kf->values[0]:1.f;
                            samp.values[oi*3+0]=samp.values[oi*3+1]=samp.values[oi*3+2]=s;
                        } else {
                            samp.values[oi*3+0] = kf->value_count>0?kf->values[0]:1;
                            samp.values[oi*3+1] = kf->value_count>1?kf->values[1]:1;
                            samp.values[oi*3+2] = kf->value_count>2?kf->values[2]:1;
                        }
                    } else {
                        float q[4];
                        m3g__quat_from_axis_angle_deg(
                            kf->value_count>0?kf->values[0]:0,
                            kf->value_count>1?kf->values[1]:0,
                            kf->value_count>2?kf->values[2]:0,
                            kf->value_count>3?kf->values[3]:1, q);
                        samp.values[oi*4+0]=q[0]; samp.values[oi*4+1]=q[1]; samp.values[oi*4+2]=q[2]; samp.values[oi*4+3]=q[3];
                    }
                }
                /* ensure TRS on node */
                if (!b.scene.nodes[nidx].has_trs) {
                    float M[16], t[3], q[4], s[3];
                    if (b.scene.nodes[nidx].has_matrix) memcpy(M, b.scene.nodes[nidx].matrix, sizeof(M));
                    else m3g_mat4_identity(M);
                    m3g_decompose_trs(M, t, q, s);
                    memcpy(b.scene.nodes[nidx].translation, t, sizeof(t));
                    memcpy(b.scene.nodes[nidx].rotation, q, sizeof(q));
                    memcpy(b.scene.nodes[nidx].scale, s, sizeof(s));
                    b.scene.nodes[nidx].has_trs = 1;
                    b.scene.nodes[nidx].has_matrix = 0;
                }
                memset(&anim, 0, sizeof(anim));
                anim.name = m3g__strdup("Animation");
                anim.samplers = (m3g_scene_anim_sampler *)m3g__xmalloc(sizeof(m3g_scene_anim_sampler));
                anim.channels = (m3g_scene_anim_channel *)m3g__xmalloc(sizeof(m3g_scene_anim_channel));
                if (!anim.name || !anim.samplers || !anim.channels) {
                    M3G_FREE(anim.name); M3G_FREE(anim.samplers); M3G_FREE(anim.channels);
                    M3G_FREE(samp.times); M3G_FREE(samp.values); continue;
                }
                anim.samplers[0] = samp; anim.sampler_count = 1;
                memset(&ch, 0, sizeof(ch));
                ch.sampler_index = 0; ch.node_index = nidx;
                strncpy(ch.path, path, sizeof(ch.path)-1);
                anim.channels[0] = ch; anim.channel_count = 1;
                ana = (m3g_scene_animation *)m3g__xrealloc(b.scene.animations, (size_t)(b.scene.animation_count+1)*sizeof(m3g_scene_animation));
                if (!ana) {
                    M3G_FREE(anim.name); M3G_FREE(samp.times); M3G_FREE(samp.values); M3G_FREE(anim.samplers); M3G_FREE(anim.channels);
                    continue;
                }
                b.scene.animations = ana; b.scene.animations[b.scene.animation_count++] = anim;
            }
        }
    }
    M3G_FREE(roots);
    M3G_FREE(b.node_map); M3G_FREE(b.mesh_map); M3G_FREE(b.mat_map);
    M3G_FREE(b.tex_map); M3G_FREE(b.img_map); M3G_FREE(b.cam_map);
    *out = b.scene;
    return M3G_OK;
fail:
    M3G_FREE(roots);
    m3g_scene_free(&b.scene);
    M3G_FREE(b.node_map); M3G_FREE(b.mesh_map); M3G_FREE(b.mat_map);
    M3G_FREE(b.tex_map); M3G_FREE(b.img_map); M3G_FREE(b.cam_map);
    return rc;
}

/* pattern attach (minimal) */
static m3g_result m3g__pattern_attach(m3g_scene_ir *scene, char const *pattern_path) {
    int i, any = 0, img_i, tex_i, samp_i = -1;
    m3g_scene_image img; m3g_scene_texture tex; char name[256];
    if (!pattern_path || !pattern_path[0]) return M3G_OK;
    for (i = 0; i < scene->material_count; ++i)
        if (scene->materials[i].base_color_texture_index < 0) any = 1;
    if (!any) return M3G_OK;
    memset(&img, 0, sizeof(img));
    snprintf(name, sizeof(name), "AutoPattern");
    img.name = m3g__strdup(name);
    img.source_path = m3g__strdup(pattern_path);
    img_i = scene->image_count;
    {
        m3g_scene_image *na = (m3g_scene_image *)m3g__xrealloc(scene->images, (size_t)(scene->image_count+1)*sizeof(m3g_scene_image));
        if (!na) { M3G_FREE(img.name); M3G_FREE(img.source_path); return M3G_ERR_NOMEM; }
        scene->images = na; scene->images[scene->image_count++] = img;
    }
    for (i = 0; i < scene->sampler_count; ++i)
        if (scene->samplers[i].wrap_s == 10497 && scene->samplers[i].wrap_t == 10497) { samp_i = i; break; }
    if (samp_i < 0) {
        m3g_scene_sampler s; m3g_scene_sampler *na;
        memset(&s, 0, sizeof(s)); s.mag_filter = s.min_filter = -1; s.wrap_s = s.wrap_t = 10497;
        samp_i = scene->sampler_count;
        na = (m3g_scene_sampler *)m3g__xrealloc(scene->samplers, (size_t)(scene->sampler_count+1)*sizeof(m3g_scene_sampler));
        if (!na) return M3G_ERR_NOMEM;
        scene->samplers = na; scene->samplers[scene->sampler_count++] = s;
    }
    memset(&tex, 0, sizeof(tex));
    tex.name = m3g__strdup("AutoPatternTexture");
    tex.image_index = img_i; tex.sampler_index = samp_i;
    tex_i = scene->texture_count;
    {
        m3g_scene_texture *na = (m3g_scene_texture *)m3g__xrealloc(scene->textures, (size_t)(scene->texture_count+1)*sizeof(m3g_scene_texture));
        if (!na) { M3G_FREE(tex.name); return M3G_ERR_NOMEM; }
        scene->textures = na; scene->textures[scene->texture_count++] = tex;
    }
    for (i = 0; i < scene->material_count; ++i)
        if (scene->materials[i].base_color_texture_index < 0)
            scene->materials[i].base_color_texture_index = tex_i;
    return M3G_OK;
}

static m3g_result m3g__load_file_bytes(char const *path, uint8_t **out, size_t *out_len) {
    FILE *f = fopen(path, "rb"); long sz; uint8_t *buf;
    if (!f) { m3g__set_err("failed to open %s", path); return M3G_ERR_IO; }
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); m3g__set_err("seek failed"); return M3G_ERR_IO; }
    sz = ftell(f); if (sz < 0) { fclose(f); return M3G_ERR_IO; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return M3G_ERR_IO; }
    buf = (uint8_t *)m3g__xmalloc((size_t)sz + 1);
    if (!buf) { fclose(f); return M3G_ERR_NOMEM; }
    if (sz > 0 && fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        M3G_FREE(buf); fclose(f); m3g__set_err("read failed"); return M3G_ERR_IO;
    }
    fclose(f);
    *out = buf; *out_len = (size_t)sz; return M3G_OK;
}

M3G_API m3g_result m3g_decode_bytes(uint8_t const *bytes, size_t len, char const *source_path,
                                    m3g_decode_options const *options, m3g_decoded *out) {
    m3g_result rc;
    m3g_decode_options opt;
    memset(out, 0, sizeof(*out));
    memset(&opt, 0, sizeof(opt));
    if (options) opt = *options;
    m3g__errbuf[0] = 0;
    rc = m3g__parse_bytes(bytes, len, &out->file); if (rc) return rc;
    out->source_path = m3g__strdup(source_path ? source_path : "");
    rc = m3g__build_scene(&out->file, out->source_path, &out->scene);
    if (rc) { m3g_decoded_free(out); return rc; }
    rc = m3g__pattern_attach(&out->scene, opt.pattern_path);
    if (rc) { m3g_decoded_free(out); return rc; }
    return M3G_OK;
}

M3G_API m3g_result m3g_decode_file(char const *path, m3g_decode_options const *options, m3g_decoded *out) {
    uint8_t *bytes = NULL; size_t len = 0; m3g_result rc;
    rc = m3g__load_file_bytes(path, &bytes, &len); if (rc) return rc;
    rc = m3g_decode_bytes(bytes, len, path, options, out);
    M3G_FREE(bytes);
    return rc;
}

#endif /* M3G_IMPLEMENTATION_INCLUDED */
#endif /* M3G_IMPLEMENTATION */
#endif /* DOXYGEN */
