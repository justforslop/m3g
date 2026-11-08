const __root = @This();
pub const __builtin = @import("std").zig.c_translation.builtins;
pub const __helpers = @import("std").zig.c_translation.helpers;
pub const ptrdiff_t = c_long;
pub const wchar_t = c_int;
pub const max_align_t = extern struct {
    __aro_max_align_ll: c_longlong = 0,
    __aro_max_align_ld: c_longdouble = 0,
};
pub const __u_char = u8;
pub const __u_short = c_ushort;
pub const __u_int = c_uint;
pub const __u_long = c_ulong;
pub const __int8_t = i8;
pub const __uint8_t = u8;
pub const __int16_t = c_short;
pub const __uint16_t = c_ushort;
pub const __int32_t = c_int;
pub const __uint32_t = c_uint;
pub const __int64_t = c_long;
pub const __uint64_t = c_ulong;
pub const __int_least8_t = __int8_t;
pub const __uint_least8_t = __uint8_t;
pub const __int_least16_t = __int16_t;
pub const __uint_least16_t = __uint16_t;
pub const __int_least32_t = __int32_t;
pub const __uint_least32_t = __uint32_t;
pub const __int_least64_t = __int64_t;
pub const __uint_least64_t = __uint64_t;
pub const __quad_t = c_long;
pub const __u_quad_t = c_ulong;
pub const __intmax_t = c_long;
pub const __uintmax_t = c_ulong;
pub const __dev_t = c_ulong;
pub const __uid_t = c_uint;
pub const __gid_t = c_uint;
pub const __ino_t = c_ulong;
pub const __ino64_t = c_ulong;
pub const __mode_t = c_uint;
pub const __nlink_t = c_ulong;
pub const __off_t = c_long;
pub const __off64_t = c_long;
pub const __pid_t = c_int;
pub const __fsid_t = extern struct {
    __val: [2]c_int = @import("std").mem.zeroes([2]c_int),
};
pub const __clock_t = c_long;
pub const __rlim_t = c_ulong;
pub const __rlim64_t = c_ulong;
pub const __id_t = c_uint;
pub const __time_t = c_long;
pub const __useconds_t = c_uint;
pub const __suseconds_t = c_long;
pub const __suseconds64_t = c_long;
pub const __daddr_t = c_int;
pub const __key_t = c_int;
pub const __clockid_t = c_int;
pub const __timer_t = ?*anyopaque;
pub const __blksize_t = c_long;
pub const __blkcnt_t = c_long;
pub const __blkcnt64_t = c_long;
pub const __fsblkcnt_t = c_ulong;
pub const __fsblkcnt64_t = c_ulong;
pub const __fsfilcnt_t = c_ulong;
pub const __fsfilcnt64_t = c_ulong;
pub const __fsword_t = c_long;
pub const __ssize_t = c_long;
pub const __syscall_slong_t = c_long;
pub const __syscall_ulong_t = c_ulong;
pub const __loff_t = __off64_t;
pub const __caddr_t = [*c]u8;
pub const __intptr_t = c_long;
pub const __socklen_t = c_uint;
pub const __sig_atomic_t = c_int;
pub const int_least8_t = __int_least8_t;
pub const int_least16_t = __int_least16_t;
pub const int_least32_t = __int_least32_t;
pub const int_least64_t = __int_least64_t;
pub const uint_least8_t = __uint_least8_t;
pub const uint_least16_t = __uint_least16_t;
pub const uint_least32_t = __uint_least32_t;
pub const uint_least64_t = __uint_least64_t;
pub const int_fast8_t = i8;
pub const int_fast16_t = c_long;
pub const int_fast32_t = c_long;
pub const int_fast64_t = c_long;
pub const uint_fast8_t = u8;
pub const uint_fast16_t = c_ulong;
pub const uint_fast32_t = c_ulong;
pub const uint_fast64_t = c_ulong;
pub const intmax_t = __intmax_t;
pub const uintmax_t = __uintmax_t;
pub extern fn memcpy(noalias __dest: ?*anyopaque, noalias __src: ?*const anyopaque, __n: usize) ?*anyopaque;
pub extern fn memmove(__dest: ?*anyopaque, __src: ?*const anyopaque, __n: usize) ?*anyopaque;
pub extern fn memccpy(noalias __dest: ?*anyopaque, noalias __src: ?*const anyopaque, __c: c_int, __n: usize) ?*anyopaque;
pub extern fn memset(__s: ?*anyopaque, __c: c_int, __n: usize) ?*anyopaque;
pub extern fn memset_explicit(__s: ?*anyopaque, __c: c_int, __n: usize) ?*anyopaque;
pub extern fn memcmp(__s1: ?*const anyopaque, __s2: ?*const anyopaque, __n: usize) c_int;
pub extern fn __memcmpeq(__s1: ?*const anyopaque, __s2: ?*const anyopaque, __n: usize) c_int;
pub extern fn memchr(__s: ?*const anyopaque, __c: c_int, __n: usize) ?*anyopaque;
pub extern fn strcpy(noalias __dest: [*c]u8, noalias __src: [*c]const u8) [*c]u8;
pub extern fn strncpy(noalias __dest: [*c]u8, noalias __src: [*c]const u8, __n: usize) [*c]u8;
pub extern fn strcat(noalias __dest: [*c]u8, noalias __src: [*c]const u8) [*c]u8;
pub extern fn strncat(noalias __dest: [*c]u8, noalias __src: [*c]const u8, __n: usize) [*c]u8;
pub extern fn strcmp(__s1: [*c]const u8, __s2: [*c]const u8) c_int;
pub extern fn strncmp(__s1: [*c]const u8, __s2: [*c]const u8, __n: usize) c_int;
pub extern fn strcoll(__s1: [*c]const u8, __s2: [*c]const u8) c_int;
pub extern fn strxfrm(noalias __dest: [*c]u8, noalias __src: [*c]const u8, __n: usize) usize;
pub const struct___locale_data_1 = opaque {};
pub const struct___locale_struct = extern struct {
    __locales: [13]?*struct___locale_data_1 = @import("std").mem.zeroes([13]?*struct___locale_data_1),
    __ctype_b: [*c]const c_ushort = null,
    __ctype_tolower: [*c]const c_int = null,
    __ctype_toupper: [*c]const c_int = null,
    __names: [13][*c]const u8 = @import("std").mem.zeroes([13][*c]const u8),
};
pub const __locale_t = [*c]struct___locale_struct;
pub const locale_t = __locale_t;
pub extern fn strcoll_l(__s1: [*c]const u8, __s2: [*c]const u8, __l: locale_t) c_int;
pub extern fn strxfrm_l(__dest: [*c]u8, __src: [*c]const u8, __n: usize, __l: locale_t) usize;
pub extern fn strdup(__s: [*c]const u8) [*c]u8;
pub extern fn strndup(__string: [*c]const u8, __n: usize) [*c]u8;
pub extern fn strchr(__s: [*c]const u8, __c: c_int) [*c]u8;
pub extern fn strrchr(__s: [*c]const u8, __c: c_int) [*c]u8;
pub extern fn strchrnul(__s: [*c]const u8, __c: c_int) [*c]u8;
pub extern fn strcspn(__s: [*c]const u8, __reject: [*c]const u8) usize;
pub extern fn strspn(__s: [*c]const u8, __accept: [*c]const u8) usize;
pub extern fn strpbrk(__s: [*c]const u8, __accept: [*c]const u8) [*c]u8;
pub extern fn strstr(__haystack: [*c]const u8, __needle: [*c]const u8) [*c]u8;
pub extern fn strtok(noalias __s: [*c]u8, noalias __delim: [*c]const u8) [*c]u8;
pub extern fn __strtok_r(noalias __s: [*c]u8, noalias __delim: [*c]const u8, noalias __save_ptr: [*c][*c]u8) [*c]u8;
pub extern fn strtok_r(noalias __s: [*c]u8, noalias __delim: [*c]const u8, noalias __save_ptr: [*c][*c]u8) [*c]u8;
pub extern fn strcasestr(__haystack: [*c]const u8, __needle: [*c]const u8) [*c]u8;
pub extern fn memmem(__haystack: ?*const anyopaque, __haystacklen: usize, __needle: ?*const anyopaque, __needlelen: usize) ?*anyopaque;
pub extern fn __mempcpy(noalias __dest: ?*anyopaque, noalias __src: ?*const anyopaque, __n: usize) ?*anyopaque;
pub extern fn mempcpy(noalias __dest: ?*anyopaque, noalias __src: ?*const anyopaque, __n: usize) ?*anyopaque;
pub extern fn strlen(__s: [*c]const u8) usize;
pub extern fn strnlen(__string: [*c]const u8, __maxlen: usize) usize;
pub extern fn strerror(__errnum: c_int) [*c]u8;
pub extern fn strerror_r(__errnum: c_int, __buf: [*c]u8, __buflen: usize) c_int;
pub extern fn strerror_l(__errnum: c_int, __l: locale_t) [*c]u8;
pub extern fn bcmp(__s1: ?*const anyopaque, __s2: ?*const anyopaque, __n: usize) c_int;
pub extern fn bcopy(__src: ?*const anyopaque, __dest: ?*anyopaque, __n: usize) void;
pub extern fn bzero(__s: ?*anyopaque, __n: usize) void;
pub extern fn index(__s: [*c]const u8, __c: c_int) [*c]u8;
pub extern fn rindex(__s: [*c]const u8, __c: c_int) [*c]u8;
pub extern fn ffs(__i: c_int) c_int;
pub extern fn ffsl(__l: c_long) c_int;
pub extern fn ffsll(__ll: c_longlong) c_int;
pub extern fn strcasecmp(__s1: [*c]const u8, __s2: [*c]const u8) c_int;
pub extern fn strncasecmp(__s1: [*c]const u8, __s2: [*c]const u8, __n: usize) c_int;
pub extern fn strcasecmp_l(__s1: [*c]const u8, __s2: [*c]const u8, __loc: locale_t) c_int;
pub extern fn strncasecmp_l(__s1: [*c]const u8, __s2: [*c]const u8, __n: usize, __loc: locale_t) c_int;
pub extern fn explicit_bzero(__s: ?*anyopaque, __n: usize) void;
pub extern fn strsep(noalias __stringp: [*c][*c]u8, noalias __delim: [*c]const u8) [*c]u8;
pub extern fn strsignal(__sig: c_int) [*c]u8;
pub extern fn __stpcpy(noalias __dest: [*c]u8, noalias __src: [*c]const u8) [*c]u8;
pub extern fn stpcpy(noalias __dest: [*c]u8, noalias __src: [*c]const u8) [*c]u8;
pub extern fn __stpncpy(noalias __dest: [*c]u8, noalias __src: [*c]const u8, __n: usize) [*c]u8;
pub extern fn stpncpy(noalias __dest: [*c]u8, noalias __src: [*c]const u8, __n: usize) [*c]u8;
pub extern fn strlcpy(noalias __dest: [*c]u8, noalias __src: [*c]const u8, __n: usize) usize;
pub extern fn strlcat(noalias __dest: [*c]u8, noalias __src: [*c]const u8, __n: usize) usize;
pub const M3G_OK: c_int = 0;
pub const M3G_ERR: c_int = -1;
pub const M3G_ERR_IO: c_int = -2;
pub const M3G_ERR_FORMAT: c_int = -3;
pub const M3G_ERR_NOMEM: c_int = -4;
pub const M3G_ERR_BACKEND: c_int = -5;
pub const M3G_ERR_RANGE: c_int = -6;
const enum_unnamed_2 = c_int;
pub const m3g_result = c_int;
pub const struct_m3g_error = extern struct {
    message: [*c]const u8 = null,
    offset: usize = 0,
};
pub const m3g_error = struct_m3g_error;
pub extern fn m3g_last_error() [*c]const u8;
pub const M3G_DEFLATE_OK: c_int = 0;
const enum_unnamed_3 = c_uint;
pub const M3G_ADLER32_INIT: c_int = 1;
const enum_unnamed_4 = c_uint;
pub const struct_m3g_deflate_io = extern struct {
    adler32: ?*const fn (adler: u32, ptr: [*c]const u8, buf_len: usize, user: ?*anyopaque) callconv(.c) u32 = null,
    uncompress: ?*const fn (dest: [*c]u8, dest_len: [*c]usize, source: [*c]const u8, source_len: usize, user: ?*anyopaque) callconv(.c) c_int = null,
    uncompress_to_heap: ?*const fn (source: [*c]const u8, source_len: usize, out_len: [*c]usize, zlib_header: c_int, user: ?*anyopaque) callconv(.c) ?*anyopaque = null,
    free_mem: ?*const fn (p: ?*anyopaque, user: ?*anyopaque) callconv(.c) void = null,
    user: ?*anyopaque = null,
    pub const m3g_set_deflate_io = __root.m3g_set_deflate_io;
    pub const io = __root.m3g_set_deflate_io;
};
pub const m3g_deflate_io = struct_m3g_deflate_io;
pub const struct_m3g_image_io = extern struct {
    load_file: ?*const fn (filename: [*c]const u8, width: [*c]c_int, height: [*c]c_int, channels_in_file: [*c]c_int, req_comp: c_int, user: ?*anyopaque) callconv(.c) [*c]u8 = null,
    load_memory: ?*const fn (buffer: [*c]const u8, len: c_int, width: [*c]c_int, height: [*c]c_int, channels_in_file: [*c]c_int, req_comp: c_int, user: ?*anyopaque) callconv(.c) [*c]u8 = null,
    free_pixels: ?*const fn (pixels: ?*anyopaque, user: ?*anyopaque) callconv(.c) void = null,
    user: ?*anyopaque = null,
    pub const m3g_set_image_io = __root.m3g_set_image_io;
    pub const io = __root.m3g_set_image_io;
};
pub const m3g_image_io = struct_m3g_image_io;
pub const struct_m3g_math_io = extern struct {
    vec3_length: ?*const fn (x: f32, y: f32, z: f32, user: ?*anyopaque) callconv(.c) f32 = null,
    mat4_identity: ?*const fn (out: [*c]f32, user: ?*anyopaque) callconv(.c) void = null,
    mat4_mul: ?*const fn (a: [*c]const f32, b: [*c]const f32, out: [*c]f32, user: ?*anyopaque) callconv(.c) void = null,
    mat4_is_identity: ?*const fn (m: [*c]const f32, epsilon: f32, user: ?*anyopaque) callconv(.c) c_int = null,
    mat4_translation: ?*const fn (x: f32, y: f32, z: f32, out: [*c]f32, user: ?*anyopaque) callconv(.c) void = null,
    mat4_scale: ?*const fn (x: f32, y: f32, z: f32, out: [*c]f32, user: ?*anyopaque) callconv(.c) void = null,
    mat4_rotation_axis: ?*const fn (angle_rad: f32, ax: f32, ay: f32, az: f32, out: [*c]f32, user: ?*anyopaque) callconv(.c) void = null,
    decompose_trs: ?*const fn (m: [*c]const f32, t: [*c]f32, q_xyzw: [*c]f32, s: [*c]f32, user: ?*anyopaque) callconv(.c) void = null,
    quat_from_axis_angle_deg: ?*const fn (angle_deg: f32, ax: f32, ay: f32, az: f32, q_xyzw: [*c]f32, user: ?*anyopaque) callconv(.c) void = null,
    user: ?*anyopaque = null,
    pub const m3g_set_math_io = __root.m3g_set_math_io;
    pub const io = __root.m3g_set_math_io;
};
pub const m3g_math_io = struct_m3g_math_io;
pub extern fn m3g_set_deflate_io(io: [*c]const m3g_deflate_io) void;
pub extern fn m3g_get_deflate_io() [*c]const m3g_deflate_io;
pub extern fn m3g_set_image_io(io: [*c]const m3g_image_io) void;
pub extern fn m3g_get_image_io() [*c]const m3g_image_io;
pub extern fn m3g_set_math_io(io: [*c]const m3g_math_io) void;
pub extern fn m3g_get_math_io() [*c]const m3g_math_io;
pub extern fn m3g_install_portable_math_io() void;
pub extern fn m3g_install_vecmath_math_io() void;
pub extern fn m3g_install_miniz_deflate_io() void;
pub extern fn m3g_install_stb_image_io() void;
pub extern fn m3g_deflate_adler32(adler: u32, ptr: [*c]const u8, buf_len: usize) u32;
pub extern fn m3g_deflate_uncompress(dest: [*c]u8, dest_len: [*c]usize, source: [*c]const u8, source_len: usize) c_int;
pub extern fn m3g_deflate_uncompress_to_heap(source: [*c]const u8, source_len: usize, out_len: [*c]usize, zlib_header: c_int) ?*anyopaque;
pub extern fn m3g_deflate_free(p: ?*anyopaque) void;
pub extern fn m3g_image_load_memory(buffer: [*c]const u8, len: c_int, width: [*c]c_int, height: [*c]c_int, channels_in_file: [*c]c_int, req_comp: c_int) [*c]u8;
pub extern fn m3g_image_load_file(filename: [*c]const u8, width: [*c]c_int, height: [*c]c_int, channels_in_file: [*c]c_int, req_comp: c_int) [*c]u8;
pub extern fn m3g_image_free_pixels(pixels: ?*anyopaque) void;
pub const M3G_OBJ_HEADER: c_int = 0;
pub const M3G_OBJ_ANIMATION_CONTROLLER: c_int = 1;
pub const M3G_OBJ_ANIMATION_TRACK: c_int = 2;
pub const M3G_OBJ_APPEARANCE: c_int = 3;
pub const M3G_OBJ_BACKGROUND: c_int = 4;
pub const M3G_OBJ_CAMERA: c_int = 5;
pub const M3G_OBJ_COMPOSITING_MODE: c_int = 6;
pub const M3G_OBJ_FOG: c_int = 7;
pub const M3G_OBJ_POLYGON_MODE: c_int = 8;
pub const M3G_OBJ_GROUP: c_int = 9;
pub const M3G_OBJ_IMAGE_2D: c_int = 10;
pub const M3G_OBJ_TRIANGLE_STRIP_ARRAY: c_int = 11;
pub const M3G_OBJ_LIGHT: c_int = 12;
pub const M3G_OBJ_MATERIAL: c_int = 13;
pub const M3G_OBJ_MESH: c_int = 14;
pub const M3G_OBJ_MORPHING_MESH: c_int = 15;
pub const M3G_OBJ_SKINNED_MESH: c_int = 16;
pub const M3G_OBJ_TEXTURE_2D: c_int = 17;
pub const M3G_OBJ_SPRITE_3D: c_int = 18;
pub const M3G_OBJ_KEYFRAME_SEQUENCE: c_int = 19;
pub const M3G_OBJ_VERTEX_ARRAY: c_int = 20;
pub const M3G_OBJ_VERTEX_BUFFER: c_int = 21;
pub const M3G_OBJ_WORLD: c_int = 22;
pub const M3G_OBJ_EXTERNAL_REFERENCE: c_int = 255;
const enum_unnamed_5 = c_uint;
pub const M3G_ANIM_ALPHA: c_int = 256;
pub const M3G_ANIM_AMBIENT_COLOR: c_int = 257;
pub const M3G_ANIM_COLOR: c_int = 258;
pub const M3G_ANIM_CROP: c_int = 259;
pub const M3G_ANIM_DENSITY: c_int = 260;
pub const M3G_ANIM_DIFFUSE_COLOR: c_int = 261;
pub const M3G_ANIM_EMISSIVE_COLOR: c_int = 262;
pub const M3G_ANIM_FAR_DISTANCE: c_int = 263;
pub const M3G_ANIM_FIELD_OF_VIEW: c_int = 264;
pub const M3G_ANIM_INTENSITY: c_int = 265;
pub const M3G_ANIM_MORPH_WEIGHTS: c_int = 266;
pub const M3G_ANIM_NEAR_DISTANCE: c_int = 267;
pub const M3G_ANIM_ORIENTATION: c_int = 268;
pub const M3G_ANIM_PICKABILITY: c_int = 269;
pub const M3G_ANIM_SCALE: c_int = 270;
pub const M3G_ANIM_SHININESS: c_int = 271;
pub const M3G_ANIM_SPECULAR_COLOR: c_int = 272;
pub const M3G_ANIM_SPOT_ANGLE: c_int = 273;
pub const M3G_ANIM_SPOT_EXPONENT: c_int = 274;
pub const M3G_ANIM_TRANSLATION: c_int = 275;
pub const M3G_ANIM_VISIBILITY: c_int = 276;
const enum_unnamed_6 = c_uint;
pub const M3G_KF_LINEAR: c_int = 176;
pub const M3G_KF_SLERP: c_int = 177;
pub const M3G_KF_SPLINE: c_int = 178;
pub const M3G_KF_SQUAD: c_int = 179;
pub const M3G_KF_STEP: c_int = 180;
const enum_unnamed_7 = c_uint;
pub extern fn m3g_type_name(object_type: c_int) [*c]const u8;
pub const struct_m3g_rgb = extern struct {
    red: c_int = 0,
    green: c_int = 0,
    blue: c_int = 0,
    pub const m3g_rgb_to_float = __root.m3g_rgb_to_float;
    pub const to_float = __root.m3g_rgb_to_float;
};
pub const m3g_rgb = struct_m3g_rgb;
pub const struct_m3g_rgba = extern struct {
    red: c_int = 0,
    green: c_int = 0,
    blue: c_int = 0,
    alpha: c_int = 0,
    pub const m3g_rgba_to_float = __root.m3g_rgba_to_float;
    pub const to_float = __root.m3g_rgba_to_float;
};
pub const m3g_rgba = struct_m3g_rgba;
pub extern fn m3g_rgb_to_float(c: [*c]const m3g_rgb, out: [*c]f32) void;
pub extern fn m3g_rgba_to_float(c: [*c]const m3g_rgba, out: [*c]f32) void;
pub const struct_m3g_section_info = extern struct {
    index: c_int = 0,
    compression_scheme: c_int = 0,
    total_section_length: c_int = 0,
    uncompressed_length: c_int = 0,
};
pub const m3g_section_info = struct_m3g_section_info;
pub const struct_m3g_user_parameter = extern struct {
    parameter_id: c_int = 0,
    value: [*c]u8 = null,
    value_len: c_int = 0,
};
pub const m3g_user_parameter = struct_m3g_user_parameter;
pub const struct_m3g_object3d_meta = extern struct {
    user_id: c_int = 0,
    animation_track_ids: [*c]c_int = null,
    animation_track_count: c_int = 0,
    user_parameters: [*c]m3g_user_parameter = null,
    user_parameter_count: c_int = 0,
};
pub const m3g_object3d_meta = struct_m3g_object3d_meta;
pub const struct_m3g_component_transform = extern struct {
    translation: [3]f32 = @import("std").mem.zeroes([3]f32),
    scale: [3]f32 = @import("std").mem.zeroes([3]f32),
    orientation_angle: f32 = 0,
    orientation_axis: [3]f32 = @import("std").mem.zeroes([3]f32),
    present: c_int = 0,
};
pub const m3g_component_transform = struct_m3g_component_transform;
pub const struct_m3g_transformable_meta = extern struct {
    object3d: m3g_object3d_meta = @import("std").mem.zeroes(m3g_object3d_meta),
    component: m3g_component_transform = @import("std").mem.zeroes(m3g_component_transform),
    general_transform: [16]f32 = @import("std").mem.zeroes([16]f32),
    has_general_transform: c_int = 0,
};
pub const m3g_transformable_meta = struct_m3g_transformable_meta;
pub const struct_m3g_alignment = extern struct {
    z_target: c_int = 0,
    y_target: c_int = 0,
    z_reference_id: c_int = 0,
    y_reference_id: c_int = 0,
    present: c_int = 0,
};
pub const m3g_alignment = struct_m3g_alignment;
pub const struct_m3g_node_meta = extern struct {
    transformable: m3g_transformable_meta = @import("std").mem.zeroes(m3g_transformable_meta),
    enable_rendering: c_int = 0,
    enable_picking: c_int = 0,
    alpha_factor: c_int = 0,
    scope: u32 = 0,
    alignment: m3g_alignment = @import("std").mem.zeroes(m3g_alignment),
    pub const m3g_node_matrix = __root.m3g_node_matrix;
    pub const matrix = __root.m3g_node_matrix;
};
pub const m3g_node_meta = struct_m3g_node_meta;
pub const struct_m3g_tex_coord_binding = extern struct {
    vertex_array_id: c_int = 0,
    bias: [3]f32 = @import("std").mem.zeroes([3]f32),
    scale: f32 = 0,
};
pub const m3g_tex_coord_binding = struct_m3g_tex_coord_binding;
pub const struct_m3g_submesh_ref = extern struct {
    index_buffer_id: c_int = 0,
    appearance_id: c_int = 0,
};
pub const m3g_submesh_ref = struct_m3g_submesh_ref;
pub const struct_m3g_bone_transform = extern struct {
    transform_node_id: c_int = 0,
    first_vertex: c_int = 0,
    vertex_count: c_int = 0,
    weight: c_int = 0,
};
pub const m3g_bone_transform = struct_m3g_bone_transform;
pub const struct_m3g_keyframe = extern struct {
    time: c_int = 0,
    values: [*c]f32 = null,
    value_count: c_int = 0,
};
pub const m3g_keyframe = struct_m3g_keyframe;
pub const struct_m3g_header_data = extern struct {
    version_major: c_int = 0,
    version_minor: c_int = 0,
    has_external_references: c_int = 0,
    total_file_size: u32 = 0,
    approximate_content_size: u32 = 0,
    authoring_field: [*c]u8 = null,
};
pub const m3g_header_data = struct_m3g_header_data;
pub const struct_m3g_external_ref_data = extern struct {
    uri: [*c]u8 = null,
};
pub const m3g_external_ref_data = struct_m3g_external_ref_data;
pub const struct_m3g_group_data = extern struct {
    node: m3g_node_meta = @import("std").mem.zeroes(m3g_node_meta),
    child_ids: [*c]c_int = null,
    child_count: c_int = 0,
    active_camera_id: c_int = 0,
    background_id: c_int = 0,
    is_world: c_int = 0,
};
pub const m3g_group_data = struct_m3g_group_data;
pub const struct_m3g_camera_data = extern struct {
    node: m3g_node_meta = @import("std").mem.zeroes(m3g_node_meta),
    projection_type: c_int = 0,
    fov_degrees: f32 = 0,
    aspect_ratio: f32 = 0,
    near_distance: f32 = 0,
    far_distance: f32 = 0,
    generic_values: [16]f32 = @import("std").mem.zeroes([16]f32),
    has_perspective: c_int = 0,
    has_generic16: c_int = 0,
};
pub const m3g_camera_data = struct_m3g_camera_data;
pub const struct_m3g_light_data = extern struct {
    node: m3g_node_meta = @import("std").mem.zeroes(m3g_node_meta),
    attenuation_constant: f32 = 0,
    attenuation_linear: f32 = 0,
    attenuation_quadratic: f32 = 0,
    color: m3g_rgb = @import("std").mem.zeroes(m3g_rgb),
    mode: c_int = 0,
    intensity: f32 = 0,
    spot_angle: f32 = 0,
    spot_exponent: f32 = 0,
};
pub const m3g_light_data = struct_m3g_light_data;
pub const struct_m3g_background_data = extern struct {
    object3d: m3g_object3d_meta = @import("std").mem.zeroes(m3g_object3d_meta),
    background_color: m3g_rgba = @import("std").mem.zeroes(m3g_rgba),
    background_image_id: c_int = 0,
    image_mode_x: c_int = 0,
    image_mode_y: c_int = 0,
    crop_x: c_int = 0,
    crop_y: c_int = 0,
    crop_width: c_int = 0,
    crop_height: c_int = 0,
    depth_clear_enabled: c_int = 0,
    color_clear_enabled: c_int = 0,
};
pub const m3g_background_data = struct_m3g_background_data;
pub const struct_m3g_fog_data = extern struct {
    object3d: m3g_object3d_meta = @import("std").mem.zeroes(m3g_object3d_meta),
    color: m3g_rgb = @import("std").mem.zeroes(m3g_rgb),
    mode: c_int = 0,
    density: f32 = 0,
    near_distance: f32 = 0,
    far_distance: f32 = 0,
};
pub const m3g_fog_data = struct_m3g_fog_data;
pub const struct_m3g_polygon_mode_data = extern struct {
    object3d: m3g_object3d_meta = @import("std").mem.zeroes(m3g_object3d_meta),
    culling: c_int = 0,
    shading: c_int = 0,
    winding: c_int = 0,
    two_sided_lighting_enabled: c_int = 0,
    local_camera_lighting_enabled: c_int = 0,
    perspective_correction_enabled: c_int = 0,
};
pub const m3g_polygon_mode_data = struct_m3g_polygon_mode_data;
pub const struct_m3g_material_data = extern struct {
    object3d: m3g_object3d_meta = @import("std").mem.zeroes(m3g_object3d_meta),
    ambient_color: m3g_rgb = @import("std").mem.zeroes(m3g_rgb),
    diffuse_color: m3g_rgba = @import("std").mem.zeroes(m3g_rgba),
    emissive_color: m3g_rgb = @import("std").mem.zeroes(m3g_rgb),
    specular_color: m3g_rgb = @import("std").mem.zeroes(m3g_rgb),
    shininess: f32 = 0,
    vertex_color_tracking_enabled: c_int = 0,
};
pub const m3g_material_data = struct_m3g_material_data;
pub const struct_m3g_vertex_array_data = extern struct {
    object3d: m3g_object3d_meta = @import("std").mem.zeroes(m3g_object3d_meta),
    component_size: c_int = 0,
    component_count: c_int = 0,
    encoding: c_int = 0,
    vertex_count: c_int = 0,
    components: [*c]c_int = null,
    component_total: c_int = 0,
};
pub const m3g_vertex_array_data = struct_m3g_vertex_array_data;
pub const struct_m3g_vertex_buffer_data = extern struct {
    object3d: m3g_object3d_meta = @import("std").mem.zeroes(m3g_object3d_meta),
    default_color: m3g_rgba = @import("std").mem.zeroes(m3g_rgba),
    positions_id: c_int = 0,
    normals_id: c_int = 0,
    colors_id: c_int = 0,
    position_bias: [3]f32 = @import("std").mem.zeroes([3]f32),
    position_scale: f32 = 0,
    tex_coord_bindings: [*c]m3g_tex_coord_binding = null,
    tex_coord_binding_count: c_int = 0,
};
pub const m3g_vertex_buffer_data = struct_m3g_vertex_buffer_data;
pub const struct_m3g_triangle_strip_data = extern struct {
    object3d: m3g_object3d_meta = @import("std").mem.zeroes(m3g_object3d_meta),
    encoding: c_int = 0,
    indices: [*c]c_int = null,
    index_count: c_int = 0,
    strip_lengths: [*c]c_int = null,
    strip_count: c_int = 0,
};
pub const m3g_triangle_strip_data = struct_m3g_triangle_strip_data;
pub const struct_m3g_appearance_data = extern struct {
    object3d: m3g_object3d_meta = @import("std").mem.zeroes(m3g_object3d_meta),
    layer: c_int = 0,
    compositing_mode_id: c_int = 0,
    fog_id: c_int = 0,
    polygon_mode_id: c_int = 0,
    material_id: c_int = 0,
    texture_ids: [*c]c_int = null,
    texture_count: c_int = 0,
};
pub const m3g_appearance_data = struct_m3g_appearance_data;
pub const struct_m3g_texture2d_data = extern struct {
    transformable: m3g_transformable_meta = @import("std").mem.zeroes(m3g_transformable_meta),
    image_id: c_int = 0,
    blend_color: m3g_rgb = @import("std").mem.zeroes(m3g_rgb),
    blending: c_int = 0,
    wrapping_s: c_int = 0,
    wrapping_t: c_int = 0,
    level_filter: c_int = 0,
    image_filter: c_int = 0,
};
pub const m3g_texture2d_data = struct_m3g_texture2d_data;
pub const struct_m3g_image2d_data = extern struct {
    object3d: m3g_object3d_meta = @import("std").mem.zeroes(m3g_object3d_meta),
    format: c_int = 0,
    is_mutable: c_int = 0,
    width: c_int = 0,
    height: c_int = 0,
    palette: [*c]u8 = null,
    palette_len: c_int = 0,
    pixels: [*c]u8 = null,
    pixels_len: c_int = 0,
    has_pixels: c_int = 0,
};
pub const m3g_image2d_data = struct_m3g_image2d_data;
pub const struct_m3g_mesh_data = extern struct {
    node: m3g_node_meta = @import("std").mem.zeroes(m3g_node_meta),
    vertex_buffer_id: c_int = 0,
    submeshes: [*c]m3g_submesh_ref = null,
    submesh_count: c_int = 0,
    skeleton_id: c_int = 0,
    bone_transforms: [*c]m3g_bone_transform = null,
    bone_transform_count: c_int = 0,
    is_skinned: c_int = 0,
    morph_target_ids: [*c]c_int = null,
    morph_weights: [*c]f32 = null,
    morph_target_count: c_int = 0,
    is_morphing: c_int = 0,
};
pub const m3g_mesh_data = struct_m3g_mesh_data;
pub const struct_m3g_compositing_mode_data = extern struct {
    object3d: m3g_object3d_meta = @import("std").mem.zeroes(m3g_object3d_meta),
    depth_test_enabled: c_int = 0,
    depth_write_enabled: c_int = 0,
    color_write_enabled: c_int = 0,
    alpha_write_enabled: c_int = 0,
    blending: c_int = 0,
    alpha_threshold: c_int = 0,
    depth_offset_factor: f32 = 0,
    depth_offset_units: f32 = 0,
};
pub const m3g_compositing_mode_data = struct_m3g_compositing_mode_data;
pub const struct_m3g_sprite3d_data = extern struct {
    node: m3g_node_meta = @import("std").mem.zeroes(m3g_node_meta),
    image_id: c_int = 0,
    appearance_id: c_int = 0,
    is_scaled: c_int = 0,
    crop_x: c_int = 0,
    crop_y: c_int = 0,
    crop_width: c_int = 0,
    crop_height: c_int = 0,
};
pub const m3g_sprite3d_data = struct_m3g_sprite3d_data;
pub const struct_m3g_anim_controller_data = extern struct {
    object3d: m3g_object3d_meta = @import("std").mem.zeroes(m3g_object3d_meta),
    speed: f32 = 0,
    weight: f32 = 0,
    active_interval_start: c_int = 0,
    active_interval_end: c_int = 0,
    reference_sequence_time: f32 = 0,
    reference_world_time: c_int = 0,
};
pub const m3g_anim_controller_data = struct_m3g_anim_controller_data;
pub const struct_m3g_anim_track_data = extern struct {
    object3d: m3g_object3d_meta = @import("std").mem.zeroes(m3g_object3d_meta),
    keyframe_sequence_id: c_int = 0,
    animation_controller_id: c_int = 0,
    property_id: c_int = 0,
};
pub const m3g_anim_track_data = struct_m3g_anim_track_data;
pub const struct_m3g_keyframe_seq_data = extern struct {
    object3d: m3g_object3d_meta = @import("std").mem.zeroes(m3g_object3d_meta),
    interpolation: c_int = 0,
    repeat_mode: c_int = 0,
    encoding: c_int = 0,
    duration: c_int = 0,
    valid_range_first: c_int = 0,
    valid_range_last: c_int = 0,
    component_count: c_int = 0,
    keyframes: [*c]m3g_keyframe = null,
    keyframe_count: c_int = 0,
};
pub const m3g_keyframe_seq_data = struct_m3g_keyframe_seq_data;
pub const struct_m3g_unknown_data = extern struct {
    raw_data: [*c]u8 = null,
    raw_len: c_int = 0,
};
pub const m3g_unknown_data = struct_m3g_unknown_data;
const union_unnamed_8 = extern union {
    header: m3g_header_data,
    external: m3g_external_ref_data,
    group: m3g_group_data,
    camera: m3g_camera_data,
    light: m3g_light_data,
    background: m3g_background_data,
    fog: m3g_fog_data,
    polygon_mode: m3g_polygon_mode_data,
    material: m3g_material_data,
    vertex_array: m3g_vertex_array_data,
    vertex_buffer: m3g_vertex_buffer_data,
    triangle_strip: m3g_triangle_strip_data,
    appearance: m3g_appearance_data,
    texture2d: m3g_texture2d_data,
    image2d: m3g_image2d_data,
    mesh: m3g_mesh_data,
    compositing_mode: m3g_compositing_mode_data,
    sprite3d: m3g_sprite3d_data,
    anim_controller: m3g_anim_controller_data,
    anim_track: m3g_anim_track_data,
    keyframe_seq: m3g_keyframe_seq_data,
    unknown: m3g_unknown_data,
};
pub const struct_m3g_object = extern struct {
    object_id: c_int = 0,
    object_type: c_int = 0,
    raw_length: c_int = 0,
    u: union_unnamed_8 = @import("std").mem.zeroes(union_unnamed_8),
};
pub const m3g_object = struct_m3g_object;
pub const struct_m3g_file = extern struct {
    header: [*c]m3g_object = null,
    sections: [*c]m3g_section_info = null,
    section_count: c_int = 0,
    objects: [*c][*c]m3g_object = null,
    object_count: c_int = 0,
    object_capacity: c_int = 0,
    pub const m3g_file_free = __root.m3g_file_free;
    pub const free = __root.m3g_file_free;
};
pub const m3g_file = struct_m3g_file;
pub extern fn m3g_file_free(f: [*c]m3g_file) void;
pub const struct_m3g_warning = extern struct {
    code: [*c]u8 = null,
    message: [*c]u8 = null,
};
pub const m3g_warning = struct_m3g_warning;
pub const struct_m3g_scene_image = extern struct {
    name: [*c]u8 = null,
    object_id: c_int = 0,
    is_embedded: c_int = 0,
    width: c_int = 0,
    height: c_int = 0,
    pixels: [*c]u8 = null,
    pixels_len: c_int = 0,
    source_path: [*c]u8 = null,
};
pub const m3g_scene_image = struct_m3g_scene_image;
pub const struct_m3g_scene_sampler = extern struct {
    mag_filter: c_int = 0,
    min_filter: c_int = 0,
    wrap_s: c_int = 0,
    wrap_t: c_int = 0,
};
pub const m3g_scene_sampler = struct_m3g_scene_sampler;
pub const struct_m3g_scene_texture = extern struct {
    name: [*c]u8 = null,
    image_index: c_int = 0,
    sampler_index: c_int = 0,
};
pub const m3g_scene_texture = struct_m3g_scene_texture;
pub const struct_m3g_scene_material = extern struct {
    name: [*c]u8 = null,
    base_color_factor: [4]f32 = @import("std").mem.zeroes([4]f32),
    base_color_texture_index: c_int = 0,
    emissive_factor: [3]f32 = @import("std").mem.zeroes([3]f32),
    roughness_factor: f32 = 0,
    metallic_factor: f32 = 0,
    double_sided: c_int = 0,
    alpha_mode: [*c]u8 = null,
};
pub const m3g_scene_material = struct_m3g_scene_material;
pub const struct_m3g_scene_primitive = extern struct {
    name: [*c]u8 = null,
    positions: [*c]f32 = null,
    position_count: c_int = 0,
    normals: [*c]f32 = null,
    normal_count: c_int = 0,
    tex_coords0: [*c]f32 = null,
    tex_coord_count: c_int = 0,
    vertex_colors: [*c]f32 = null,
    vertex_color_count: c_int = 0,
    indices: [*c]c_int = null,
    index_count: c_int = 0,
    material_index: c_int = 0,
};
pub const m3g_scene_primitive = struct_m3g_scene_primitive;
pub const struct_m3g_scene_mesh = extern struct {
    name: [*c]u8 = null,
    primitives: [*c]m3g_scene_primitive = null,
    primitive_count: c_int = 0,
};
pub const m3g_scene_mesh = struct_m3g_scene_mesh;
pub const struct_m3g_scene_camera = extern struct {
    name: [*c]u8 = null,
    has_perspective: c_int = 0,
    yfov_radians: f32 = 0,
    aspect_ratio: f32 = 0,
    znear: f32 = 0,
    zfar: f32 = 0,
};
pub const m3g_scene_camera = struct_m3g_scene_camera;
pub const struct_m3g_scene_node = extern struct {
    name: [*c]u8 = null,
    matrix: [16]f32 = @import("std").mem.zeroes([16]f32),
    has_matrix: c_int = 0,
    translation: [3]f32 = @import("std").mem.zeroes([3]f32),
    rotation: [4]f32 = @import("std").mem.zeroes([4]f32),
    scale: [3]f32 = @import("std").mem.zeroes([3]f32),
    has_trs: c_int = 0,
    mesh_index: c_int = 0,
    camera_index: c_int = 0,
    children: [*c]c_int = null,
    child_count: c_int = 0,
};
pub const m3g_scene_node = struct_m3g_scene_node;
pub const struct_m3g_scene_anim_sampler = extern struct {
    times: [*c]f32 = null,
    time_count: c_int = 0,
    values: [*c]f32 = null,
    value_count: c_int = 0,
    interpolation: [16]u8 = @import("std").mem.zeroes([16]u8),
    component_count: c_int = 0,
};
pub const m3g_scene_anim_sampler = struct_m3g_scene_anim_sampler;
pub const struct_m3g_scene_anim_channel = extern struct {
    sampler_index: c_int = 0,
    node_index: c_int = 0,
    path: [32]u8 = @import("std").mem.zeroes([32]u8),
};
pub const m3g_scene_anim_channel = struct_m3g_scene_anim_channel;
pub const struct_m3g_scene_animation = extern struct {
    name: [*c]u8 = null,
    samplers: [*c]m3g_scene_anim_sampler = null,
    sampler_count: c_int = 0,
    channels: [*c]m3g_scene_anim_channel = null,
    channel_count: c_int = 0,
};
pub const m3g_scene_animation = struct_m3g_scene_animation;
pub const struct_m3g_scene_ir = extern struct {
    nodes: [*c]m3g_scene_node = null,
    node_count: c_int = 0,
    root_node_indices: [*c]c_int = null,
    root_count: c_int = 0,
    meshes: [*c]m3g_scene_mesh = null,
    mesh_count: c_int = 0,
    materials: [*c]m3g_scene_material = null,
    material_count: c_int = 0,
    textures: [*c]m3g_scene_texture = null,
    texture_count: c_int = 0,
    images: [*c]m3g_scene_image = null,
    image_count: c_int = 0,
    samplers: [*c]m3g_scene_sampler = null,
    sampler_count: c_int = 0,
    cameras: [*c]m3g_scene_camera = null,
    camera_count: c_int = 0,
    animations: [*c]m3g_scene_animation = null,
    animation_count: c_int = 0,
    warnings: [*c]m3g_warning = null,
    warning_count: c_int = 0,
    pub const m3g_scene_free = __root.m3g_scene_free;
    pub const free = __root.m3g_scene_free;
};
pub const m3g_scene_ir = struct_m3g_scene_ir;
pub extern fn m3g_scene_free(s: [*c]m3g_scene_ir) void;
pub const struct_m3g_decode_options = extern struct {
    pattern_path: [*c]const u8 = null,
};
pub const m3g_decode_options = struct_m3g_decode_options;
pub const struct_m3g_decoded = extern struct {
    source_path: [*c]u8 = null,
    file: m3g_file = @import("std").mem.zeroes(m3g_file),
    scene: m3g_scene_ir = @import("std").mem.zeroes(m3g_scene_ir),
    pub const m3g_decoded_free = __root.m3g_decoded_free;
    pub const free = __root.m3g_decoded_free;
};
pub const m3g_decoded = struct_m3g_decoded;
pub extern fn m3g_decode_file(path: [*c]const u8, options: [*c]const m3g_decode_options, out: [*c]m3g_decoded) m3g_result;
pub extern fn m3g_decode_bytes(bytes: [*c]const u8, len: usize, source_path: [*c]const u8, options: [*c]const m3g_decode_options, out: [*c]m3g_decoded) m3g_result;
pub extern fn m3g_decoded_free(d: [*c]m3g_decoded) void;
pub extern fn m3g_mat4_identity(m: [*c]f32) void;
pub extern fn m3g_mat4_mul(a: [*c]const f32, b: [*c]const f32, out: [*c]f32) void;
pub extern fn m3g_mat4_is_identity(m: [*c]const f32, epsilon: f32) c_int;
pub extern fn m3g_mat4_translation(x: f32, y: f32, z: f32, out: [*c]f32) void;
pub extern fn m3g_mat4_scale(x: f32, y: f32, z: f32, out: [*c]f32) void;
pub extern fn m3g_mat4_rotation_axis(angle_rad: f32, ax: f32, ay: f32, az: f32, out: [*c]f32) void;
pub extern fn m3g_vec3_length(x: f32, y: f32, z: f32) f32;
pub extern fn m3g_quat_from_axis_angle_deg(angle_deg: f32, ax: f32, ay: f32, az: f32, q_xyzw: [*c]f32) void;
pub extern fn m3g_node_matrix(node: [*c]const m3g_node_meta, out: [*c]f32) void;
pub extern fn m3g_decompose_trs(m: [*c]const f32, t: [*c]f32, q_xyzw: [*c]f32, s: [*c]f32) void;

pub const __VERSION__ = "Aro aro-zig";
pub const __Aro__ = "";
pub const __STDC__ = @as(c_int, 1);
pub const __STDC_HOSTED__ = @as(c_int, 1);
pub const __STDC_UTF_16__ = @as(c_int, 1);
pub const __STDC_UTF_32__ = @as(c_int, 1);
pub const __STDC_EMBED_NOT_FOUND__ = @as(c_int, 0);
pub const __STDC_EMBED_FOUND__ = @as(c_int, 1);
pub const __STDC_EMBED_EMPTY__ = @as(c_int, 2);
pub const __STDC_VERSION__ = @as(c_long, 201710);
pub const __GNUC__ = @as(c_int, 7);
pub const __GNUC_MINOR__ = @as(c_int, 1);
pub const __GNUC_PATCHLEVEL__ = @as(c_int, 0);
pub const __ARO_EMULATE_NO__ = @as(c_int, 0);
pub const __ARO_EMULATE_CLANG__ = @as(c_int, 1);
pub const __ARO_EMULATE_GCC__ = @as(c_int, 2);
pub const __ARO_EMULATE_MSVC__ = @as(c_int, 3);
pub const __ARO_EMULATE__ = __ARO_EMULATE_GCC__;
pub inline fn __building_module(x: anytype) @TypeOf(@as(c_int, 0)) {
    _ = &x;
    return @as(c_int, 0);
}
pub const linux = @as(c_int, 1);
pub const __linux = @as(c_int, 1);
pub const __linux__ = @as(c_int, 1);
pub const unix = @as(c_int, 1);
pub const __unix = @as(c_int, 1);
pub const __unix__ = @as(c_int, 1);
pub const __code_model_small__ = @as(c_int, 1);
pub const __amd64__ = @as(c_int, 1);
pub const __amd64 = @as(c_int, 1);
pub const __x86_64__ = @as(c_int, 1);
pub const __x86_64 = @as(c_int, 1);
pub const __SEG_GS = @as(c_int, 1);
pub const __SEG_FS = @as(c_int, 1);
pub const __seg_gs = @compileError("unable to translate macro: undefined identifier `address_space`");
// <builtin>:33:9
pub const __seg_fs = @compileError("unable to translate macro: undefined identifier `address_space`");
// <builtin>:34:9
pub const __LAHF_SAHF__ = @as(c_int, 1);
pub const __AES__ = @as(c_int, 1);
pub const __VAES__ = @as(c_int, 1);
pub const __PCLMUL__ = @as(c_int, 1);
pub const __VPCLMULQDQ__ = @as(c_int, 1);
pub const __LZCNT__ = @as(c_int, 1);
pub const __RDRND__ = @as(c_int, 1);
pub const __FSGSBASE__ = @as(c_int, 1);
pub const __BMI__ = @as(c_int, 1);
pub const __BMI2__ = @as(c_int, 1);
pub const __POPCNT__ = @as(c_int, 1);
pub const __PRFCHW__ = @as(c_int, 1);
pub const __RDSEED__ = @as(c_int, 1);
pub const __ADX__ = @as(c_int, 1);
pub const __MWAITX__ = @as(c_int, 1);
pub const __MOVBE__ = @as(c_int, 1);
pub const __SSE4A__ = @as(c_int, 1);
pub const __FMA__ = @as(c_int, 1);
pub const __F16C__ = @as(c_int, 1);
pub const __GFNI__ = @as(c_int, 1);
pub const __AVX512CD__ = @as(c_int, 1);
pub const __AVX512VPOPCNTDQ__ = @as(c_int, 1);
pub const __AVX512VNNI__ = @as(c_int, 1);
pub const __AVX512BF16__ = @as(c_int, 1);
pub const __AVX512DQ__ = @as(c_int, 1);
pub const __AVX512BITALG__ = @as(c_int, 1);
pub const __AVX512BW__ = @as(c_int, 1);
pub const __AVX512VL__ = @as(c_int, 1);
pub const __EVEX256__ = @as(c_int, 1);
pub const __AVX512VBMI__ = @as(c_int, 1);
pub const __AVX512VBMI2__ = @as(c_int, 1);
pub const __AVX512IFMA__ = @as(c_int, 1);
pub const __AVX512VP2INTERSECT__ = @as(c_int, 1);
pub const __SHA__ = @as(c_int, 1);
pub const __FXSR__ = @as(c_int, 1);
pub const __XSAVE__ = @as(c_int, 1);
pub const __XSAVEOPT__ = @as(c_int, 1);
pub const __XSAVEC__ = @as(c_int, 1);
pub const __XSAVES__ = @as(c_int, 1);
pub const __PKU__ = @as(c_int, 1);
pub const __CLFLUSHOPT__ = @as(c_int, 1);
pub const __CLWB__ = @as(c_int, 1);
pub const __WBNOINVD__ = @as(c_int, 1);
pub const __SHSTK__ = @as(c_int, 1);
pub const __PREFETCHI__ = @as(c_int, 1);
pub const __CLZERO__ = @as(c_int, 1);
pub const __RDPID__ = @as(c_int, 1);
pub const __RDPRU__ = @as(c_int, 1);
pub const __MOVDIRI__ = @as(c_int, 1);
pub const __MOVDIR64B__ = @as(c_int, 1);
pub const __INVPCID__ = @as(c_int, 1);
pub const __AVXVNNI__ = @as(c_int, 1);
pub const __CRC32__ = @as(c_int, 1);
pub const __AVX512F__ = @as(c_int, 1);
pub const __AVX2__ = @as(c_int, 1);
pub const __AVX__ = @as(c_int, 1);
pub const __SSE4_2__ = @as(c_int, 1);
pub const __SSE4_1__ = @as(c_int, 1);
pub const __SSSE3__ = @as(c_int, 1);
pub const __SSE3__ = @as(c_int, 1);
pub const __SSE2__ = @as(c_int, 1);
pub const __SSE__ = @as(c_int, 1);
pub const __SSE_MATH__ = @as(c_int, 1);
pub const __MMX__ = @as(c_int, 1);
pub const __GCC_HAVE_SYNC_COMPARE_AND_SWAP_8 = @as(c_int, 1);
pub const __SIZEOF_FLOAT128__ = @as(c_int, 16);
pub const _LP64 = @as(c_int, 1);
pub const __LP64__ = @as(c_int, 1);
pub const __FLOAT128__ = @as(c_int, 1);
pub const __ORDER_LITTLE_ENDIAN__ = @as(c_int, 1234);
pub const __ORDER_BIG_ENDIAN__ = @as(c_int, 4321);
pub const __ORDER_PDP_ENDIAN__ = @as(c_int, 3412);
pub const __BYTE_ORDER__ = __ORDER_LITTLE_ENDIAN__;
pub const __LITTLE_ENDIAN__ = @as(c_int, 1);
pub const __ELF__ = @as(c_int, 1);
pub const __ATOMIC_RELAXED = @as(c_int, 0);
pub const __ATOMIC_CONSUME = @as(c_int, 1);
pub const __ATOMIC_ACQUIRE = @as(c_int, 2);
pub const __ATOMIC_RELEASE = @as(c_int, 3);
pub const __ATOMIC_ACQ_REL = @as(c_int, 4);
pub const __ATOMIC_SEQ_CST = @as(c_int, 5);
pub const __ATOMIC_BOOL_LOCK_FREE = @as(c_int, 1);
pub const __ATOMIC_CHAR_LOCK_FREE = @as(c_int, 1);
pub const __ATOMIC_CHAR16_T_LOCK_FREE = @as(c_int, 1);
pub const __ATOMIC_CHAR32_T_LOCK_FREE = @as(c_int, 1);
pub const __ATOMIC_WCHAR_T_LOCK_FREE = @as(c_int, 1);
pub const __ATOMIC_WINT_T_LOCK_FREE = @as(c_int, 1);
pub const __ATOMIC_SHORT_LOCK_FREE = @as(c_int, 1);
pub const __ATOMIC_INT_LOCK_FREE = @as(c_int, 1);
pub const __ATOMIC_LONG_LOCK_FREE = @as(c_int, 1);
pub const __ATOMIC_LLONG_LOCK_FREE = @as(c_int, 1);
pub const __ATOMIC_POINTER_LOCK_FREE = @as(c_int, 1);
pub const __WINT_UNSIGNED__ = @as(c_int, 1);
pub const __CHAR_BIT__ = @as(c_int, 8);
pub const __BOOL_WIDTH__ = @as(c_int, 8);
pub const __SCHAR_MAX__ = @as(c_int, 127);
pub const __SCHAR_WIDTH__ = @as(c_int, 8);
pub const __SHRT_MAX__ = @as(c_int, 32767);
pub const __SHRT_WIDTH__ = @as(c_int, 16);
pub const __INT_MAX__ = __helpers.promoteIntLiteral(c_int, 2147483647, .decimal);
pub const __INT_WIDTH__ = @as(c_int, 32);
pub const __LONG_MAX__ = __helpers.promoteIntLiteral(c_long, 9223372036854775807, .decimal);
pub const __LONG_WIDTH__ = @as(c_int, 64);
pub const __LONG_LONG_MAX__ = @as(c_longlong, 9223372036854775807);
pub const __LONG_LONG_WIDTH__ = @as(c_int, 64);
pub const __WCHAR_MAX__ = __helpers.promoteIntLiteral(c_int, 2147483647, .decimal);
pub const __WCHAR_WIDTH__ = @as(c_int, 32);
pub const __WINT_MAX__ = __helpers.promoteIntLiteral(c_uint, 4294967295, .decimal);
pub const __WINT_WIDTH__ = @as(c_int, 32);
pub const __INTMAX_MAX__ = __helpers.promoteIntLiteral(c_long, 9223372036854775807, .decimal);
pub const __INTMAX_WIDTH__ = @as(c_int, 64);
pub const __SIZE_MAX__ = __helpers.promoteIntLiteral(c_ulong, 18446744073709551615, .decimal);
pub const __SIZE_WIDTH__ = @as(c_int, 64);
pub const __UINTMAX_MAX__ = __helpers.promoteIntLiteral(c_ulong, 18446744073709551615, .decimal);
pub const __UINTMAX_WIDTH__ = @as(c_int, 64);
pub const __PTRDIFF_MAX__ = __helpers.promoteIntLiteral(c_long, 9223372036854775807, .decimal);
pub const __PTRDIFF_WIDTH__ = @as(c_int, 64);
pub const __INTPTR_MAX__ = __helpers.promoteIntLiteral(c_long, 9223372036854775807, .decimal);
pub const __INTPTR_WIDTH__ = @as(c_int, 64);
pub const __UINTPTR_MAX__ = __helpers.promoteIntLiteral(c_ulong, 18446744073709551615, .decimal);
pub const __UINTPTR_WIDTH__ = @as(c_int, 64);
pub const __SIG_ATOMIC_MAX__ = __helpers.promoteIntLiteral(c_int, 2147483647, .decimal);
pub const __SIG_ATOMIC_WIDTH__ = @as(c_int, 32);
pub const __BITINT_MAXWIDTH__ = __helpers.promoteIntLiteral(c_int, 65535, .decimal);
pub const __SIZEOF_FLOAT__ = @as(c_int, 4);
pub const __SIZEOF_DOUBLE__ = @as(c_int, 8);
pub const __SIZEOF_LONG_DOUBLE__ = @as(c_int, 10);
pub const __SIZEOF_SHORT__ = @as(c_int, 2);
pub const __SIZEOF_INT__ = @as(c_int, 4);
pub const __SIZEOF_LONG__ = @as(c_int, 8);
pub const __SIZEOF_LONG_LONG__ = @as(c_int, 8);
pub const __SIZEOF_POINTER__ = @as(c_int, 8);
pub const __SIZEOF_PTRDIFF_T__ = @as(c_int, 8);
pub const __SIZEOF_SIZE_T__ = @as(c_int, 8);
pub const __SIZEOF_WCHAR_T__ = @as(c_int, 4);
pub const __SIZEOF_WINT_T__ = @as(c_int, 4);
pub const __SIZEOF_INT128__ = @as(c_int, 16);
pub const __INTPTR_TYPE__ = c_long;
pub const __UINTPTR_TYPE__ = c_ulong;
pub const __INTMAX_TYPE__ = c_long;
pub const __INTMAX_C_SUFFIX__ = @compileError("unable to translate macro: undefined identifier `L`");
// <builtin>:176:9
pub const __INTMAX_C = __helpers.L_SUFFIX;
pub const __UINTMAX_TYPE__ = c_ulong;
pub const __UINTMAX_C_SUFFIX__ = @compileError("unable to translate macro: undefined identifier `UL`");
// <builtin>:179:9
pub const __UINTMAX_C = __helpers.UL_SUFFIX;
pub const __PTRDIFF_TYPE__ = c_long;
pub const __SIZE_TYPE__ = c_ulong;
pub const __WCHAR_TYPE__ = c_int;
pub const __WINT_TYPE__ = c_uint;
pub const __CHAR16_TYPE__ = c_ushort;
pub const __CHAR32_TYPE__ = c_uint;
pub const __INT8_TYPE__ = i8;
pub const __INT8_FMTd__ = "hhd";
pub const __INT8_FMTi__ = "hhi";
pub const __INT8_C_SUFFIX__ = "";
pub inline fn __INT8_C(c: anytype) @TypeOf(c) {
    _ = &c;
    return c;
}
pub const __INT16_TYPE__ = c_short;
pub const __INT16_FMTd__ = "hd";
pub const __INT16_FMTi__ = "hi";
pub const __INT16_C_SUFFIX__ = "";
pub inline fn __INT16_C(c: anytype) @TypeOf(c) {
    _ = &c;
    return c;
}
pub const __INT32_TYPE__ = c_int;
pub const __INT32_FMTd__ = "d";
pub const __INT32_FMTi__ = "i";
pub const __INT32_C_SUFFIX__ = "";
pub inline fn __INT32_C(c: anytype) @TypeOf(c) {
    _ = &c;
    return c;
}
pub const __INT64_TYPE__ = c_long;
pub const __INT64_FMTd__ = "ld";
pub const __INT64_FMTi__ = "li";
pub const __INT64_C_SUFFIX__ = @compileError("unable to translate macro: undefined identifier `L`");
// <builtin>:205:9
pub const __UINT8_TYPE__ = u8;
pub const __UINT8_FMTo__ = "hho";
pub const __UINT8_FMTu__ = "hhu";
pub const __UINT8_FMTx__ = "hhx";
pub const __UINT8_FMTX__ = "hhX";
pub const __UINT8_C_SUFFIX__ = "";
pub inline fn __UINT8_C(c: anytype) @TypeOf(c) {
    _ = &c;
    return c;
}
pub const __UINT8_MAX__ = @as(c_int, 255);
pub const __INT8_MAX__ = @as(c_int, 127);
pub const __UINT16_TYPE__ = c_ushort;
pub const __UINT16_FMTo__ = "ho";
pub const __UINT16_FMTu__ = "hu";
pub const __UINT16_FMTx__ = "hx";
pub const __UINT16_FMTX__ = "hX";
pub const __UINT16_C_SUFFIX__ = "";
pub inline fn __UINT16_C(c: anytype) @TypeOf(c) {
    _ = &c;
    return c;
}
pub const __UINT16_MAX__ = __helpers.promoteIntLiteral(c_int, 65535, .decimal);
pub const __INT16_MAX__ = @as(c_int, 32767);
pub const __UINT32_TYPE__ = c_uint;
pub const __UINT32_FMTo__ = "o";
pub const __UINT32_FMTu__ = "u";
pub const __UINT32_FMTx__ = "x";
pub const __UINT32_FMTX__ = "X";
pub const __UINT32_C_SUFFIX__ = @compileError("unable to translate macro: undefined identifier `U`");
// <builtin>:230:9
pub const __UINT32_C = __helpers.U_SUFFIX;
pub const __UINT32_MAX__ = __helpers.promoteIntLiteral(c_uint, 4294967295, .decimal);
pub const __INT32_MAX__ = __helpers.promoteIntLiteral(c_int, 2147483647, .decimal);
pub const __UINT64_TYPE__ = c_ulong;
pub const __UINT64_FMTo__ = "lo";
pub const __UINT64_FMTu__ = "lu";
pub const __UINT64_FMTx__ = "lx";
pub const __UINT64_FMTX__ = "lX";
pub const __UINT64_C_SUFFIX__ = @compileError("unable to translate macro: undefined identifier `UL`");
// <builtin>:239:9
pub const __UINT64_MAX__ = __helpers.promoteIntLiteral(c_ulong, 18446744073709551615, .decimal);
pub const __INT64_MAX__ = __helpers.promoteIntLiteral(c_long, 9223372036854775807, .decimal);
pub const __INT_LEAST8_TYPE__ = i8;
pub const __INT_LEAST8_MAX__ = @as(c_int, 127);
pub const __INT_LEAST8_WIDTH__ = @as(c_int, 8);
pub const INT_LEAST8_FMTd__ = "hhd";
pub const INT_LEAST8_FMTi__ = "hhi";
pub const __UINT_LEAST8_TYPE__ = u8;
pub const __UINT_LEAST8_MAX__ = @as(c_int, 255);
pub const UINT_LEAST8_FMTo__ = "hho";
pub const UINT_LEAST8_FMTu__ = "hhu";
pub const UINT_LEAST8_FMTx__ = "hhx";
pub const UINT_LEAST8_FMTX__ = "hhX";
pub const __INT_FAST8_TYPE__ = i8;
pub const __INT_FAST8_MAX__ = @as(c_int, 127);
pub const __INT_FAST8_WIDTH__ = @as(c_int, 8);
pub const INT_FAST8_FMTd__ = "hhd";
pub const INT_FAST8_FMTi__ = "hhi";
pub const __UINT_FAST8_TYPE__ = u8;
pub const __UINT_FAST8_MAX__ = @as(c_int, 255);
pub const UINT_FAST8_FMTo__ = "hho";
pub const UINT_FAST8_FMTu__ = "hhu";
pub const UINT_FAST8_FMTx__ = "hhx";
pub const UINT_FAST8_FMTX__ = "hhX";
pub const __INT_LEAST16_TYPE__ = c_short;
pub const __INT_LEAST16_MAX__ = @as(c_int, 32767);
pub const __INT_LEAST16_WIDTH__ = @as(c_int, 16);
pub const INT_LEAST16_FMTd__ = "hd";
pub const INT_LEAST16_FMTi__ = "hi";
pub const __UINT_LEAST16_TYPE__ = c_ushort;
pub const __UINT_LEAST16_MAX__ = __helpers.promoteIntLiteral(c_int, 65535, .decimal);
pub const UINT_LEAST16_FMTo__ = "ho";
pub const UINT_LEAST16_FMTu__ = "hu";
pub const UINT_LEAST16_FMTx__ = "hx";
pub const UINT_LEAST16_FMTX__ = "hX";
pub const __INT_FAST16_TYPE__ = c_short;
pub const __INT_FAST16_MAX__ = @as(c_int, 32767);
pub const __INT_FAST16_WIDTH__ = @as(c_int, 16);
pub const INT_FAST16_FMTd__ = "hd";
pub const INT_FAST16_FMTi__ = "hi";
pub const __UINT_FAST16_TYPE__ = c_ushort;
pub const __UINT_FAST16_MAX__ = __helpers.promoteIntLiteral(c_int, 65535, .decimal);
pub const UINT_FAST16_FMTo__ = "ho";
pub const UINT_FAST16_FMTu__ = "hu";
pub const UINT_FAST16_FMTx__ = "hx";
pub const UINT_FAST16_FMTX__ = "hX";
pub const __INT_LEAST32_TYPE__ = c_int;
pub const __INT_LEAST32_MAX__ = __helpers.promoteIntLiteral(c_int, 2147483647, .decimal);
pub const __INT_LEAST32_WIDTH__ = @as(c_int, 32);
pub const INT_LEAST32_FMTd__ = "d";
pub const INT_LEAST32_FMTi__ = "i";
pub const __UINT_LEAST32_TYPE__ = c_uint;
pub const __UINT_LEAST32_MAX__ = __helpers.promoteIntLiteral(c_uint, 4294967295, .decimal);
pub const UINT_LEAST32_FMTo__ = "o";
pub const UINT_LEAST32_FMTu__ = "u";
pub const UINT_LEAST32_FMTx__ = "x";
pub const UINT_LEAST32_FMTX__ = "X";
pub const __INT_FAST32_TYPE__ = c_int;
pub const __INT_FAST32_MAX__ = __helpers.promoteIntLiteral(c_int, 2147483647, .decimal);
pub const __INT_FAST32_WIDTH__ = @as(c_int, 32);
pub const INT_FAST32_FMTd__ = "d";
pub const INT_FAST32_FMTi__ = "i";
pub const __UINT_FAST32_TYPE__ = c_uint;
pub const __UINT_FAST32_MAX__ = __helpers.promoteIntLiteral(c_uint, 4294967295, .decimal);
pub const UINT_FAST32_FMTo__ = "o";
pub const UINT_FAST32_FMTu__ = "u";
pub const UINT_FAST32_FMTx__ = "x";
pub const UINT_FAST32_FMTX__ = "X";
pub const __INT_LEAST64_TYPE__ = c_long;
pub const __INT_LEAST64_MAX__ = __helpers.promoteIntLiteral(c_long, 9223372036854775807, .decimal);
pub const __INT_LEAST64_WIDTH__ = @as(c_int, 64);
pub const INT_LEAST64_FMTd__ = "ld";
pub const INT_LEAST64_FMTi__ = "li";
pub const __UINT_LEAST64_TYPE__ = c_ulong;
pub const __UINT_LEAST64_MAX__ = __helpers.promoteIntLiteral(c_ulong, 18446744073709551615, .decimal);
pub const UINT_LEAST64_FMTo__ = "lo";
pub const UINT_LEAST64_FMTu__ = "lu";
pub const UINT_LEAST64_FMTx__ = "lx";
pub const UINT_LEAST64_FMTX__ = "lX";
pub const __INT_FAST64_TYPE__ = c_long;
pub const __INT_FAST64_MAX__ = __helpers.promoteIntLiteral(c_long, 9223372036854775807, .decimal);
pub const __INT_FAST64_WIDTH__ = @as(c_int, 64);
pub const INT_FAST64_FMTd__ = "ld";
pub const INT_FAST64_FMTi__ = "li";
pub const __UINT_FAST64_TYPE__ = c_ulong;
pub const __UINT_FAST64_MAX__ = __helpers.promoteIntLiteral(c_ulong, 18446744073709551615, .decimal);
pub const UINT_FAST64_FMTo__ = "lo";
pub const UINT_FAST64_FMTu__ = "lu";
pub const UINT_FAST64_FMTx__ = "lx";
pub const UINT_FAST64_FMTX__ = "lX";
pub const __FLT16_DENORM_MIN__ = @as(f16, 5.9604644775390625e-8);
pub const __FLT16_HAS_DENORM__ = "";
pub const __FLT16_DIG__ = @as(c_int, 3);
pub const __FLT16_DECIMAL_DIG__ = @as(c_int, 5);
pub const __FLT16_EPSILON__ = @as(f16, 9.765625e-4);
pub const __FLT16_HAS_INFINITY__ = "";
pub const __FLT16_HAS_QUIET_NAN__ = "";
pub const __FLT16_MANT_DIG__ = @as(c_int, 11);
pub const __FLT16_MAX_10_EXP__ = @as(c_int, 4);
pub const __FLT16_MAX_EXP__ = @as(c_int, 16);
pub const __FLT16_MAX__ = @as(f16, 6.5504e+4);
pub const __FLT16_MIN_10_EXP__ = -@as(c_int, 4);
pub const __FLT16_MIN_EXP__ = -@as(c_int, 13);
pub const __FLT16_MIN__ = @as(f16, 6.103515625e-5);
pub const __FLT_DENORM_MIN__ = @as(f32, 1.40129846e-45);
pub const __FLT_HAS_DENORM__ = "";
pub const __FLT_DIG__ = @as(c_int, 6);
pub const __FLT_DECIMAL_DIG__ = @as(c_int, 9);
pub const __FLT_EPSILON__ = @as(f32, 1.19209290e-7);
pub const __FLT_HAS_INFINITY__ = "";
pub const __FLT_HAS_QUIET_NAN__ = "";
pub const __FLT_MANT_DIG__ = @as(c_int, 24);
pub const __FLT_MAX_10_EXP__ = @as(c_int, 38);
pub const __FLT_MAX_EXP__ = @as(c_int, 128);
pub const __FLT_MAX__ = @as(f32, 3.40282347e+38);
pub const __FLT_MIN_10_EXP__ = -@as(c_int, 37);
pub const __FLT_MIN_EXP__ = -@as(c_int, 125);
pub const __FLT_MIN__ = @as(f32, 1.17549435e-38);
pub const __DBL_DENORM_MIN__ = @as(f64, 4.9406564584124654e-324);
pub const __DBL_HAS_DENORM__ = "";
pub const __DBL_DIG__ = @as(c_int, 15);
pub const __DBL_DECIMAL_DIG__ = @as(c_int, 17);
pub const __DBL_EPSILON__ = @as(f64, 2.2204460492503131e-16);
pub const __DBL_HAS_INFINITY__ = "";
pub const __DBL_HAS_QUIET_NAN__ = "";
pub const __DBL_MANT_DIG__ = @as(c_int, 53);
pub const __DBL_MAX_10_EXP__ = @as(c_int, 308);
pub const __DBL_MAX_EXP__ = @as(c_int, 1024);
pub const __DBL_MAX__ = @as(f64, 1.7976931348623157e+308);
pub const __DBL_MIN_10_EXP__ = -@as(c_int, 307);
pub const __DBL_MIN_EXP__ = -@as(c_int, 1021);
pub const __DBL_MIN__ = @as(f64, 2.2250738585072014e-308);
pub const __LDBL_DENORM_MIN__ = @as(c_longdouble, 3.64519953188247460253e-4951);
pub const __LDBL_HAS_DENORM__ = "";
pub const __LDBL_DIG__ = @as(c_int, 18);
pub const __LDBL_DECIMAL_DIG__ = @as(c_int, 21);
pub const __LDBL_EPSILON__ = @as(c_longdouble, 1.08420217248550443401e-19);
pub const __LDBL_HAS_INFINITY__ = "";
pub const __LDBL_HAS_QUIET_NAN__ = "";
pub const __LDBL_MANT_DIG__ = @as(c_int, 64);
pub const __LDBL_MAX_10_EXP__ = @as(c_int, 4932);
pub const __LDBL_MAX_EXP__ = @as(c_int, 16384);
pub const __LDBL_MAX__ = @as(c_longdouble, 1.18973149535723176502e+4932);
pub const __LDBL_MIN_10_EXP__ = -@as(c_int, 4931);
pub const __LDBL_MIN_EXP__ = -@as(c_int, 16381);
pub const __LDBL_MIN__ = @as(c_longdouble, 3.36210314311209350626e-4932);
pub const __FLT_EVAL_METHOD__ = @as(c_int, 0);
pub const __FLT_RADIX__ = @as(c_int, 2);
pub const __DECIMAL_DIG__ = __LDBL_DECIMAL_DIG__;
pub const M3G_H_INCLUDED = "";
pub const __STDC_VERSION_STDDEF_H__ = @as(c_long, 202311);
pub const NULL = __helpers.cast(?*anyopaque, @as(c_int, 0));
pub const offsetof = @compileError("unable to translate macro: undefined identifier `__builtin_offsetof`");
// /home/fe2o/lang/zig-x86_64-linux-0.17.0/lib/compiler/aro/include/stddef.h:18:9
pub const __CLANG_STDINT_H = "";
pub const _STDINT_H = @as(c_int, 1);
pub const _FEATURES_H = @as(c_int, 1);
pub const __KERNEL_STRICT_NAMES = "";
pub inline fn __GNUC_PREREQ(maj: anytype, min: anytype) @TypeOf(((__GNUC__ << @as(c_int, 16)) + __GNUC_MINOR__) >= ((maj << @as(c_int, 16)) + min)) {
    _ = &maj;
    _ = &min;
    return ((__GNUC__ << @as(c_int, 16)) + __GNUC_MINOR__) >= ((maj << @as(c_int, 16)) + min);
}
pub inline fn __glibc_clang_prereq(maj: anytype, min: anytype) @TypeOf(@as(c_int, 0)) {
    _ = &maj;
    _ = &min;
    return @as(c_int, 0);
}
pub const __GLIBC_USE = @compileError("unable to translate macro: undefined identifier `__GLIBC_USE_`");
// /usr/include/features.h:197:9
pub const _DEFAULT_SOURCE = @as(c_int, 1);
pub const __GLIBC_USE_ISOC2Y = @as(c_int, 0);
pub const __GLIBC_USE_ISOC23 = @as(c_int, 0);
pub const __USE_ISOC11 = @as(c_int, 1);
pub const __USE_POSIX_IMPLICITLY = @as(c_int, 1);
pub const _POSIX_SOURCE = @as(c_int, 1);
pub const _POSIX_C_SOURCE = @as(c_long, 202405);
pub const __USE_POSIX = @as(c_int, 1);
pub const __USE_POSIX2 = @as(c_int, 1);
pub const __USE_POSIX199309 = @as(c_int, 1);
pub const __USE_POSIX199506 = @as(c_int, 1);
pub const __USE_XOPEN2K = @as(c_int, 1);
pub const __USE_ISOC95 = @as(c_int, 1);
pub const __USE_ISOC99 = @as(c_int, 1);
pub const __USE_XOPEN2K8 = @as(c_int, 1);
pub const _ATFILE_SOURCE = @as(c_int, 1);
pub const __USE_XOPEN2K24 = @as(c_int, 1);
pub const __WORDSIZE = @as(c_int, 64);
pub const __WORDSIZE_TIME64_COMPAT32 = @as(c_int, 1);
pub const __SYSCALL_WORDSIZE = @as(c_int, 64);
pub const __TIMESIZE = __WORDSIZE;
pub const __USE_TIME_BITS64 = @as(c_int, 1);
pub const __USE_MISC = @as(c_int, 1);
pub const __USE_ATFILE = @as(c_int, 1);
pub const __USE_FORTIFY_LEVEL = @as(c_int, 0);
pub const __GLIBC_USE_DEPRECATED_GETS = @as(c_int, 0);
pub const __GLIBC_USE_DEPRECATED_SCANF = @as(c_int, 0);
pub const __GLIBC_USE_C23_STRTOL = @as(c_int, 0);
pub const _STDC_PREDEF_H = @as(c_int, 1);
pub const __STDC_IEC_559__ = @as(c_int, 1);
pub const __STDC_IEC_60559_BFP__ = @as(c_long, 201404);
pub const __STDC_IEC_559_COMPLEX__ = @as(c_int, 1);
pub const __STDC_IEC_60559_COMPLEX__ = @as(c_long, 201404);
pub const __STDC_ISO_10646__ = @as(c_long, 201706);
pub const __GNU_LIBRARY__ = @as(c_int, 6);
pub const __GLIBC__ = @as(c_int, 2);
pub const __GLIBC_MINOR__ = @as(c_int, 43);
pub inline fn __GLIBC_PREREQ(maj: anytype, min: anytype) @TypeOf(((__GLIBC__ << @as(c_int, 16)) + __GLIBC_MINOR__) >= ((maj << @as(c_int, 16)) + min)) {
    _ = &maj;
    _ = &min;
    return ((__GLIBC__ << @as(c_int, 16)) + __GLIBC_MINOR__) >= ((maj << @as(c_int, 16)) + min);
}
pub const _SYS_CDEFS_H = @as(c_int, 1);
pub const __glibc_has_attribute = @compileError("unable to translate macro: undefined identifier `__has_attribute`");
// /usr/include/sys/cdefs.h:45:10
pub inline fn __glibc_has_builtin(name: anytype) @TypeOf(__builtin.has_builtin(name)) {
    _ = &name;
    return __builtin.has_builtin(name);
}
pub const __glibc_has_extension = @compileError("unable to translate macro: undefined identifier `__has_extension`");
// /usr/include/sys/cdefs.h:55:10
pub const __LEAF = @compileError("unable to translate macro: undefined identifier `__leaf__`");
// /usr/include/sys/cdefs.h:65:11
pub const __LEAF_ATTR = @compileError("unable to translate macro: undefined identifier `__leaf__`");
// /usr/include/sys/cdefs.h:66:11
pub const __THROW = @compileError("unable to translate macro: undefined identifier `__nothrow__`");
// /usr/include/sys/cdefs.h:79:11
pub const __THROWNL = @compileError("unable to translate macro: undefined identifier `__nothrow__`");
// /usr/include/sys/cdefs.h:80:11
pub const __NTH = @compileError("unable to translate macro: undefined identifier `__nothrow__`");
// /usr/include/sys/cdefs.h:81:11
pub const __NTHNL = @compileError("unable to translate macro: undefined identifier `__nothrow__`");
// /usr/include/sys/cdefs.h:82:11
pub const __COLD = @compileError("unable to translate macro: undefined identifier `__cold__`");
// /usr/include/sys/cdefs.h:102:11
pub inline fn __P(args: anytype) @TypeOf(args) {
    _ = &args;
    return args;
}
pub inline fn __PMT(args: anytype) @TypeOf(args) {
    _ = &args;
    return args;
}
pub const __CONCAT = @compileError("unable to translate C expr: unexpected token '##'");
// /usr/include/sys/cdefs.h:131:9
pub const __STRING = @compileError("unable to translate C expr: unexpected token ''");
// /usr/include/sys/cdefs.h:132:9
pub const __ptr_t = ?*anyopaque;
pub const __BEGIN_DECLS = "";
pub const __END_DECLS = "";
pub const __attribute_overloadable__ = "";
pub inline fn __bos(ptr: anytype) @TypeOf(__builtin.object_size(ptr, __USE_FORTIFY_LEVEL > @as(c_int, 1))) {
    _ = &ptr;
    return __builtin.object_size(ptr, __USE_FORTIFY_LEVEL > @as(c_int, 1));
}
pub inline fn __bos0(ptr: anytype) @TypeOf(__builtin.object_size(ptr, @as(c_int, 0))) {
    _ = &ptr;
    return __builtin.object_size(ptr, @as(c_int, 0));
}
pub inline fn __glibc_objsize0(__o: anytype) @TypeOf(__bos0(__o)) {
    _ = &__o;
    return __bos0(__o);
}
pub inline fn __glibc_objsize(__o: anytype) @TypeOf(__bos(__o)) {
    _ = &__o;
    return __bos(__o);
}
pub const __warnattr = @compileError("unable to translate macro: undefined identifier `__warning__`");
// /usr/include/sys/cdefs.h:366:10
pub const __errordecl = @compileError("unable to translate macro: undefined identifier `__error__`");
// /usr/include/sys/cdefs.h:367:10
pub const __flexarr = @compileError("unable to translate C expr: unexpected token '['");
// /usr/include/sys/cdefs.h:379:10
pub const __glibc_c99_flexarr_available = @as(c_int, 1);
pub const __REDIRECT = @compileError("unable to translate C expr: unexpected token '__asm__'");
// /usr/include/sys/cdefs.h:410:10
pub const __REDIRECT_NTH = @compileError("unable to translate C expr: unexpected token '__asm__'");
// /usr/include/sys/cdefs.h:417:11
pub const __REDIRECT_NTHNL = @compileError("unable to translate C expr: unexpected token '__asm__'");
// /usr/include/sys/cdefs.h:419:11
pub const __ASMNAME = @compileError("unable to translate macro: undefined identifier `__USER_LABEL_PREFIX__`");
// /usr/include/sys/cdefs.h:422:10
pub inline fn __ASMNAME2(prefix: anytype, cname: anytype) @TypeOf(__STRING(prefix) ++ cname) {
    _ = &prefix;
    _ = &cname;
    return __STRING(prefix) ++ cname;
}
pub const __REDIRECT_FORTIFY = __REDIRECT;
pub const __REDIRECT_FORTIFY_NTH = __REDIRECT_NTH;
pub const __attribute_malloc__ = @compileError("unable to translate macro: undefined identifier `__malloc__`");
// /usr/include/sys/cdefs.h:452:10
pub const __attribute_alloc_size__ = @compileError("unable to translate macro: undefined identifier `__alloc_size__`");
// /usr/include/sys/cdefs.h:460:10
pub const __attribute_alloc_align__ = @compileError("unable to translate macro: undefined identifier `__alloc_align__`");
// /usr/include/sys/cdefs.h:469:10
pub const __attribute_pure__ = @compileError("unable to translate macro: undefined identifier `__pure__`");
// /usr/include/sys/cdefs.h:479:10
pub const __attribute_const__ = @compileError("unable to translate C expr: unexpected token '__attribute__'");
// /usr/include/sys/cdefs.h:486:10
pub const __attribute_maybe_unused__ = @compileError("unable to translate macro: undefined identifier `__unused__`");
// /usr/include/sys/cdefs.h:492:10
pub const __attribute_used__ = @compileError("unable to translate macro: undefined identifier `__used__`");
// /usr/include/sys/cdefs.h:501:10
pub const __attribute_noinline__ = @compileError("unable to translate macro: undefined identifier `__noinline__`");
// /usr/include/sys/cdefs.h:502:10
pub const __attribute_deprecated__ = @compileError("unable to translate macro: undefined identifier `__deprecated__`");
// /usr/include/sys/cdefs.h:510:10
pub const __attribute_deprecated_msg__ = @compileError("unable to translate macro: undefined identifier `__deprecated__`");
// /usr/include/sys/cdefs.h:520:10
pub const __attribute_format_arg__ = @compileError("unable to translate macro: undefined identifier `__format_arg__`");
// /usr/include/sys/cdefs.h:533:10
pub const __attribute_format_strfmon__ = @compileError("unable to translate macro: undefined identifier `__format__`");
// /usr/include/sys/cdefs.h:543:10
pub const __attribute_nonnull__ = @compileError("unable to translate macro: undefined identifier `__nonnull__`");
// /usr/include/sys/cdefs.h:555:11
pub inline fn __nonnull(params: anytype) @TypeOf(__attribute_nonnull__(params)) {
    _ = &params;
    return __attribute_nonnull__(params);
}
pub const __returns_nonnull = @compileError("unable to translate macro: undefined identifier `__returns_nonnull__`");
// /usr/include/sys/cdefs.h:568:10
pub const __attribute_warn_unused_result__ = @compileError("unable to translate macro: undefined identifier `__warn_unused_result__`");
// /usr/include/sys/cdefs.h:577:10
pub const __wur = "";
pub const __always_inline = @compileError("unable to translate macro: undefined identifier `__always_inline__`");
// /usr/include/sys/cdefs.h:595:10
pub const __attribute_artificial__ = @compileError("unable to translate macro: undefined identifier `__artificial__`");
// /usr/include/sys/cdefs.h:604:10
pub const __extern_inline = @compileError("unable to translate C expr: unexpected token 'extern'");
// /usr/include/sys/cdefs.h:626:11
pub const __extern_always_inline = @compileError("unable to translate C expr: unexpected token 'extern'");
// /usr/include/sys/cdefs.h:627:11
pub const __fortify_function = __extern_always_inline ++ __attribute_artificial__;
pub const __va_arg_pack = @compileError("unable to translate macro: undefined identifier `__builtin_va_arg_pack`");
// /usr/include/sys/cdefs.h:638:10
pub const __va_arg_pack_len = @compileError("unable to translate macro: undefined identifier `__builtin_va_arg_pack_len`");
// /usr/include/sys/cdefs.h:639:10
pub const __restrict_arr = @compileError("unable to translate C expr: unexpected token '__restrict'");
// /usr/include/sys/cdefs.h:666:10
pub inline fn __glibc_unlikely(cond: anytype) @TypeOf(__builtin.expect(cond, @as(c_int, 0))) {
    _ = &cond;
    return __builtin.expect(cond, @as(c_int, 0));
}
pub inline fn __glibc_likely(cond: anytype) @TypeOf(__builtin.expect(cond, @as(c_int, 1))) {
    _ = &cond;
    return __builtin.expect(cond, @as(c_int, 1));
}
pub const __attribute_nonstring__ = "";
pub inline fn __attribute_copy__(arg: anytype) void {
    _ = &arg;
    return;
}
pub const __LDOUBLE_REDIRECTS_TO_FLOAT128_ABI = @as(c_int, 0);
pub inline fn __LDBL_REDIR1(name: anytype, proto: anytype, alias: anytype) @TypeOf(name ++ proto) {
    _ = &name;
    _ = &proto;
    _ = &alias;
    return name ++ proto;
}
pub inline fn __LDBL_REDIR(name: anytype, proto: anytype) @TypeOf(name ++ proto) {
    _ = &name;
    _ = &proto;
    return name ++ proto;
}
pub inline fn __LDBL_REDIR1_NTH(name: anytype, proto: anytype, alias: anytype) @TypeOf(name ++ proto ++ __THROW) {
    _ = &name;
    _ = &proto;
    _ = &alias;
    return name ++ proto ++ __THROW;
}
pub inline fn __LDBL_REDIR_NTH(name: anytype, proto: anytype) @TypeOf(name ++ proto ++ __THROW) {
    _ = &name;
    _ = &proto;
    return name ++ proto ++ __THROW;
}
pub inline fn __LDBL_REDIR2_DECL(name: anytype) void {
    _ = &name;
    return;
}
pub inline fn __LDBL_REDIR_DECL(name: anytype) void {
    _ = &name;
    return;
}
pub inline fn __REDIRECT_LDBL(name: anytype, proto: anytype, alias: anytype) @TypeOf(__REDIRECT(name, proto, alias)) {
    _ = &name;
    _ = &proto;
    _ = &alias;
    return __REDIRECT(name, proto, alias);
}
pub inline fn __REDIRECT_NTH_LDBL(name: anytype, proto: anytype, alias: anytype) @TypeOf(__REDIRECT_NTH(name, proto, alias)) {
    _ = &name;
    _ = &proto;
    _ = &alias;
    return __REDIRECT_NTH(name, proto, alias);
}
pub const __glibc_macro_warning1 = @compileError("unable to translate macro: undefined identifier `_Pragma`");
// /usr/include/sys/cdefs.h:807:10
pub const __glibc_macro_warning = @compileError("unable to translate macro: undefined identifier `GCC`");
// /usr/include/sys/cdefs.h:808:10
pub const __HAVE_GENERIC_SELECTION = @as(c_int, 1);
pub const __glibc_const_generic = @compileError("unable to translate C expr: expected type instead got 'const'");
// /usr/include/sys/cdefs.h:837:10
pub inline fn __fortified_attr_access(a: anytype, o: anytype, s: anytype) void {
    _ = &a;
    _ = &o;
    _ = &s;
    return;
}
pub inline fn __attr_access(x: anytype) void {
    _ = &x;
    return;
}
pub inline fn __attr_access_none(argno: anytype) void {
    _ = &argno;
    return;
}
pub inline fn __attr_dealloc(dealloc: anytype, argno: anytype) void {
    _ = &dealloc;
    _ = &argno;
    return;
}
pub const __attr_dealloc_free = "";
pub const __attribute_returns_twice__ = @compileError("unable to translate macro: undefined identifier `__returns_twice__`");
// /usr/include/sys/cdefs.h:884:10
pub const __attribute_struct_may_alias__ = @compileError("unable to translate macro: undefined identifier `__may_alias__`");
// /usr/include/sys/cdefs.h:893:10
pub const __stub___compat_bdflush = "";
pub const __stub_chflags = "";
pub const __stub_fchflags = "";
pub const __stub_gtty = "";
pub const __stub_revoke = "";
pub const __stub_setlogin = "";
pub const __stub_sigreturn = "";
pub const __stub_stty = "";
pub const _BITS_TYPES_H = @as(c_int, 1);
pub const __S16_TYPE = c_short;
pub const __U16_TYPE = c_ushort;
pub const __S32_TYPE = c_int;
pub const __U32_TYPE = c_uint;
pub const __SLONGWORD_TYPE = c_long;
pub const __ULONGWORD_TYPE = c_ulong;
pub const __SQUAD_TYPE = c_long;
pub const __UQUAD_TYPE = c_ulong;
pub const __SWORD_TYPE = c_long;
pub const __UWORD_TYPE = c_ulong;
pub const __SLONG32_TYPE = c_int;
pub const __ULONG32_TYPE = c_uint;
pub const __S64_TYPE = c_long;
pub const __U64_TYPE = c_ulong;
pub const _BITS_TYPESIZES_H = @as(c_int, 1);
pub const __SYSCALL_SLONG_TYPE = __SLONGWORD_TYPE;
pub const __SYSCALL_ULONG_TYPE = __ULONGWORD_TYPE;
pub const __DEV_T_TYPE = __UQUAD_TYPE;
pub const __UID_T_TYPE = __U32_TYPE;
pub const __GID_T_TYPE = __U32_TYPE;
pub const __INO_T_TYPE = __SYSCALL_ULONG_TYPE;
pub const __INO64_T_TYPE = __UQUAD_TYPE;
pub const __MODE_T_TYPE = __U32_TYPE;
pub const __NLINK_T_TYPE = __SYSCALL_ULONG_TYPE;
pub const __FSWORD_T_TYPE = __SYSCALL_SLONG_TYPE;
pub const __OFF_T_TYPE = __SYSCALL_SLONG_TYPE;
pub const __OFF64_T_TYPE = __SQUAD_TYPE;
pub const __PID_T_TYPE = __S32_TYPE;
pub const __RLIM_T_TYPE = __SYSCALL_ULONG_TYPE;
pub const __RLIM64_T_TYPE = __UQUAD_TYPE;
pub const __BLKCNT_T_TYPE = __SYSCALL_SLONG_TYPE;
pub const __BLKCNT64_T_TYPE = __SQUAD_TYPE;
pub const __FSBLKCNT_T_TYPE = __SYSCALL_ULONG_TYPE;
pub const __FSBLKCNT64_T_TYPE = __UQUAD_TYPE;
pub const __FSFILCNT_T_TYPE = __SYSCALL_ULONG_TYPE;
pub const __FSFILCNT64_T_TYPE = __UQUAD_TYPE;
pub const __ID_T_TYPE = __U32_TYPE;
pub const __CLOCK_T_TYPE = __SYSCALL_SLONG_TYPE;
pub const __TIME_T_TYPE = __SYSCALL_SLONG_TYPE;
pub const __USECONDS_T_TYPE = __U32_TYPE;
pub const __SUSECONDS_T_TYPE = __SYSCALL_SLONG_TYPE;
pub const __SUSECONDS64_T_TYPE = __SQUAD_TYPE;
pub const __DADDR_T_TYPE = __S32_TYPE;
pub const __KEY_T_TYPE = __S32_TYPE;
pub const __CLOCKID_T_TYPE = __S32_TYPE;
pub const __TIMER_T_TYPE = ?*anyopaque;
pub const __BLKSIZE_T_TYPE = __SYSCALL_SLONG_TYPE;
pub const __FSID_T_TYPE = @compileError("unable to translate macro: undefined identifier `__val`");
// /usr/include/bits/typesizes.h:73:9
pub const __SSIZE_T_TYPE = __SWORD_TYPE;
pub const __CPU_MASK_TYPE = __SYSCALL_ULONG_TYPE;
pub const __OFF_T_MATCHES_OFF64_T = @as(c_int, 1);
pub const __INO_T_MATCHES_INO64_T = @as(c_int, 1);
pub const __RLIM_T_MATCHES_RLIM64_T = @as(c_int, 1);
pub const __STATFS_MATCHES_STATFS64 = @as(c_int, 1);
pub const __KERNEL_OLD_TIMEVAL_MATCHES_TIMEVAL64 = @as(c_int, 1);
pub const __FD_SETSIZE = @as(c_int, 1024);
pub const _BITS_TIME64_H = @as(c_int, 1);
pub const __TIME64_T_TYPE = __TIME_T_TYPE;
pub const _BITS_WCHAR_H = @as(c_int, 1);
pub const __WCHAR_MAX = __WCHAR_MAX__;
pub const __WCHAR_MIN = -__WCHAR_MAX - @as(c_int, 1);
pub const _BITS_STDINT_INTN_H = @as(c_int, 1);
pub const _BITS_STDINT_UINTN_H = @as(c_int, 1);
pub const _BITS_STDINT_LEAST_H = @as(c_int, 1);
pub const __intptr_t_defined = "";
pub const __INT64_C = __helpers.L_SUFFIX;
pub const __UINT64_C = __helpers.UL_SUFFIX;
pub const INT8_MIN = -@as(c_int, 128);
pub const INT16_MIN = -@as(c_int, 32767) - @as(c_int, 1);
pub const INT32_MIN = -__helpers.promoteIntLiteral(c_int, 2147483647, .decimal) - @as(c_int, 1);
pub const INT64_MIN = -__INT64_C(__helpers.promoteIntLiteral(c_int, 9223372036854775807, .decimal)) - @as(c_int, 1);
pub const INT8_MAX = @as(c_int, 127);
pub const INT16_MAX = @as(c_int, 32767);
pub const INT32_MAX = __helpers.promoteIntLiteral(c_int, 2147483647, .decimal);
pub const INT64_MAX = __INT64_C(__helpers.promoteIntLiteral(c_int, 9223372036854775807, .decimal));
pub const UINT8_MAX = @as(c_int, 255);
pub const UINT16_MAX = __helpers.promoteIntLiteral(c_int, 65535, .decimal);
pub const UINT32_MAX = __helpers.promoteIntLiteral(c_uint, 4294967295, .decimal);
pub const UINT64_MAX = __UINT64_C(__helpers.promoteIntLiteral(c_int, 18446744073709551615, .decimal));
pub const INT_LEAST8_MIN = -@as(c_int, 128);
pub const INT_LEAST16_MIN = -@as(c_int, 32767) - @as(c_int, 1);
pub const INT_LEAST32_MIN = -__helpers.promoteIntLiteral(c_int, 2147483647, .decimal) - @as(c_int, 1);
pub const INT_LEAST64_MIN = -__INT64_C(__helpers.promoteIntLiteral(c_int, 9223372036854775807, .decimal)) - @as(c_int, 1);
pub const INT_LEAST8_MAX = @as(c_int, 127);
pub const INT_LEAST16_MAX = @as(c_int, 32767);
pub const INT_LEAST32_MAX = __helpers.promoteIntLiteral(c_int, 2147483647, .decimal);
pub const INT_LEAST64_MAX = __INT64_C(__helpers.promoteIntLiteral(c_int, 9223372036854775807, .decimal));
pub const UINT_LEAST8_MAX = @as(c_int, 255);
pub const UINT_LEAST16_MAX = __helpers.promoteIntLiteral(c_int, 65535, .decimal);
pub const UINT_LEAST32_MAX = __helpers.promoteIntLiteral(c_uint, 4294967295, .decimal);
pub const UINT_LEAST64_MAX = __UINT64_C(__helpers.promoteIntLiteral(c_int, 18446744073709551615, .decimal));
pub const INT_FAST8_MIN = -@as(c_int, 128);
pub const INT_FAST16_MIN = -__helpers.promoteIntLiteral(c_long, 9223372036854775807, .decimal) - @as(c_int, 1);
pub const INT_FAST32_MIN = -__helpers.promoteIntLiteral(c_long, 9223372036854775807, .decimal) - @as(c_int, 1);
pub const INT_FAST64_MIN = -__INT64_C(__helpers.promoteIntLiteral(c_int, 9223372036854775807, .decimal)) - @as(c_int, 1);
pub const INT_FAST8_MAX = @as(c_int, 127);
pub const INT_FAST16_MAX = __helpers.promoteIntLiteral(c_long, 9223372036854775807, .decimal);
pub const INT_FAST32_MAX = __helpers.promoteIntLiteral(c_long, 9223372036854775807, .decimal);
pub const INT_FAST64_MAX = __INT64_C(__helpers.promoteIntLiteral(c_int, 9223372036854775807, .decimal));
pub const UINT_FAST8_MAX = @as(c_int, 255);
pub const UINT_FAST16_MAX = __helpers.promoteIntLiteral(c_ulong, 18446744073709551615, .decimal);
pub const UINT_FAST32_MAX = __helpers.promoteIntLiteral(c_ulong, 18446744073709551615, .decimal);
pub const UINT_FAST64_MAX = __UINT64_C(__helpers.promoteIntLiteral(c_int, 18446744073709551615, .decimal));
pub const INTPTR_MIN = -__helpers.promoteIntLiteral(c_long, 9223372036854775807, .decimal) - @as(c_int, 1);
pub const INTPTR_MAX = __helpers.promoteIntLiteral(c_long, 9223372036854775807, .decimal);
pub const UINTPTR_MAX = __helpers.promoteIntLiteral(c_ulong, 18446744073709551615, .decimal);
pub const INTMAX_MIN = -__INT64_C(__helpers.promoteIntLiteral(c_int, 9223372036854775807, .decimal)) - @as(c_int, 1);
pub const INTMAX_MAX = __INT64_C(__helpers.promoteIntLiteral(c_int, 9223372036854775807, .decimal));
pub const UINTMAX_MAX = __UINT64_C(__helpers.promoteIntLiteral(c_int, 18446744073709551615, .decimal));
pub const PTRDIFF_MIN = -__helpers.promoteIntLiteral(c_long, 9223372036854775807, .decimal) - @as(c_int, 1);
pub const PTRDIFF_MAX = __helpers.promoteIntLiteral(c_long, 9223372036854775807, .decimal);
pub const SIG_ATOMIC_MIN = -__helpers.promoteIntLiteral(c_int, 2147483647, .decimal) - @as(c_int, 1);
pub const SIG_ATOMIC_MAX = __helpers.promoteIntLiteral(c_int, 2147483647, .decimal);
pub const SIZE_MAX = __helpers.promoteIntLiteral(c_ulong, 18446744073709551615, .decimal);
pub const WCHAR_MIN = __WCHAR_MIN;
pub const WCHAR_MAX = __WCHAR_MAX;
pub const WINT_MIN = @as(c_uint, 0);
pub const WINT_MAX = __helpers.promoteIntLiteral(c_uint, 4294967295, .decimal);
pub inline fn INT8_C(c: anytype) @TypeOf(c) {
    _ = &c;
    return c;
}
pub inline fn INT16_C(c: anytype) @TypeOf(c) {
    _ = &c;
    return c;
}
pub inline fn INT32_C(c: anytype) @TypeOf(c) {
    _ = &c;
    return c;
}
pub const INT64_C = __helpers.L_SUFFIX;
pub inline fn UINT8_C(c: anytype) @TypeOf(c) {
    _ = &c;
    return c;
}
pub inline fn UINT16_C(c: anytype) @TypeOf(c) {
    _ = &c;
    return c;
}
pub const UINT32_C = __helpers.U_SUFFIX;
pub const UINT64_C = __helpers.UL_SUFFIX;
pub const INTMAX_C = __helpers.L_SUFFIX;
pub const UINTMAX_C = __helpers.UL_SUFFIX;
pub const _STRING_H = @as(c_int, 1);
pub const __GLIBC_USE_LIB_EXT2 = @as(c_int, 0);
pub const __GLIBC_USE_IEC_60559_BFP_EXT = @as(c_int, 0);
pub const __GLIBC_USE_IEC_60559_BFP_EXT_C23 = @as(c_int, 0);
pub const __GLIBC_USE_IEC_60559_EXT = @as(c_int, 0);
pub const __GLIBC_USE_IEC_60559_FUNCS_EXT = @as(c_int, 0);
pub const __GLIBC_USE_IEC_60559_FUNCS_EXT_C23 = @as(c_int, 0);
pub const __GLIBC_USE_IEC_60559_TYPES_EXT = @as(c_int, 0);
pub const __need_size_t = "";
pub const __need_NULL = "";
pub const _BITS_TYPES_LOCALE_T_H = @as(c_int, 1);
pub const _BITS_TYPES___LOCALE_T_H = @as(c_int, 1);
pub const _STRINGS_H = @as(c_int, 1);
pub const M3G_API = "";
pub const M3G_VERSION_MAJOR = @as(c_int, 0);
pub const M3G_VERSION_MINOR = @as(c_int, 2);
pub const M3G_VERSION_PATCH = @as(c_int, 0);
pub const M3G_IDENTIFIER_LEN = @as(c_int, 12);
pub const __locale_struct = struct___locale_struct;
