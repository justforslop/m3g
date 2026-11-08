//! Raw FFI declarations mirroring `m3g.h`.
#![allow(non_camel_case_types, non_snake_case, dead_code)]

use libc::{c_char, c_float, c_int, c_void, size_t};

pub const M3G_VERSION_MAJOR: u32 = 0;
pub const M3G_VERSION_MINOR: u32 = 2;
pub const M3G_VERSION_PATCH: u32 = 0;
pub const M3G_IDENTIFIER_LEN: usize = 12;

// ---------- error codes ----------
pub const M3G_OK: c_int = 0;
pub const M3G_ERR: c_int = -1;
pub const M3G_ERR_IO: c_int = -2;
pub const M3G_ERR_FORMAT: c_int = -3;
pub const M3G_ERR_NOMEM: c_int = -4;
pub const M3G_ERR_BACKEND: c_int = -5;
pub const M3G_ERR_RANGE: c_int = -6;

pub type m3g_result = c_int;

#[repr(C)]
pub struct m3g_error {
    pub message: *const c_char,
    pub offset: size_t,
}

pub const M3G_DEFLATE_OK: c_int = 0;
pub const M3G_ADLER32_INIT: u32 = 1;

// ---------- I/O backends ----------
#[repr(C)]
pub struct m3g_deflate_io {
    pub adler32: Option<
        unsafe extern "C" fn(u32, *const u8, size_t, *mut c_void) -> u32,
    >,
    pub uncompress: Option<
        unsafe extern "C" fn(*mut u8, *mut size_t, *const u8, size_t, *mut c_void) -> c_int,
    >,
    pub uncompress_to_heap: Option<
        unsafe extern "C" fn(*const u8, size_t, *mut size_t, c_int, *mut c_void) -> *mut c_void,
    >,
    pub free_mem: Option<unsafe extern "C" fn(*mut c_void, *mut c_void)>,
    pub user: *mut c_void,
}

#[repr(C)]
pub struct m3g_image_io {
    pub load_file: Option<
        unsafe extern "C" fn(
            *const c_char,
            *mut c_int,
            *mut c_int,
            *mut c_int,
            c_int,
            *mut c_void,
        ) -> *mut u8,
    >,
    pub load_memory: Option<
        unsafe extern "C" fn(
            *const u8,
            c_int,
            *mut c_int,
            *mut c_int,
            *mut c_int,
            c_int,
            *mut c_void,
        ) -> *mut u8,
    >,
    pub free_pixels: Option<unsafe extern "C" fn(*mut c_void, *mut c_void)>,
    pub user: *mut c_void,
}

#[repr(C)]
pub struct m3g_math_io {
    pub vec3_length: Option<unsafe extern "C" fn(f32, f32, f32, *mut c_void) -> f32>,
    pub mat4_identity: Option<unsafe extern "C" fn(*mut f32, *mut c_void)>,
    pub mat4_mul: Option<unsafe extern "C" fn(*const f32, *const f32, *mut f32, *mut c_void)>,
    pub mat4_is_identity: Option<unsafe extern "C" fn(*const f32, f32, *mut c_void) -> c_int>,
    pub mat4_translation: Option<unsafe extern "C" fn(f32, f32, f32, *mut f32, *mut c_void)>,
    pub mat4_scale: Option<unsafe extern "C" fn(f32, f32, f32, *mut f32, *mut c_void)>,
    pub mat4_rotation_axis:
        Option<unsafe extern "C" fn(f32, f32, f32, f32, *mut f32, *mut c_void)>,
    pub decompose_trs:
        Option<unsafe extern "C" fn(*const f32, *mut f32, *mut f32, *mut f32, *mut c_void)>,
    pub quat_from_axis_angle_deg:
        Option<unsafe extern "C" fn(f32, f32, f32, f32, *mut f32, *mut c_void)>,
    pub user: *mut c_void,
}

// ---------- object types ----------
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
pub const M3G_OBJ_EXTERNAL_REFERENCE: c_int = 0xFF;

// ---------- color / small structs ----------
#[repr(C)]
#[derive(Copy, Clone, Default, Debug)]
pub struct m3g_rgb {
    pub red: c_int,
    pub green: c_int,
    pub blue: c_int,
}

#[repr(C)]
#[derive(Copy, Clone, Default, Debug)]
pub struct m3g_rgba {
    pub red: c_int,
    pub green: c_int,
    pub blue: c_int,
    pub alpha: c_int,
}

#[repr(C)]
#[derive(Copy, Clone, Default)]
pub struct m3g_section_info {
    pub index: c_int,
    pub compression_scheme: c_int,
    pub total_section_length: c_int,
    pub uncompressed_length: c_int,
}

#[repr(C)]
pub struct m3g_user_parameter {
    pub parameter_id: c_int,
    pub value: *mut u8,
    pub value_len: c_int,
}

#[repr(C)]
pub struct m3g_object3d_meta {
    pub user_id: c_int,
    pub animation_track_ids: *mut c_int,
    pub animation_track_count: c_int,
    pub user_parameters: *mut m3g_user_parameter,
    pub user_parameter_count: c_int,
}

#[repr(C)]
#[derive(Copy, Clone, Default)]
pub struct m3g_component_transform {
    pub translation: [f32; 3],
    pub scale: [f32; 3],
    pub orientation_angle: f32,
    pub orientation_axis: [f32; 3],
    pub present: c_int,
}

#[repr(C)]
pub struct m3g_transformable_meta {
    pub object3d: m3g_object3d_meta,
    pub component: m3g_component_transform,
    pub general_transform: [f32; 16],
    pub has_general_transform: c_int,
}

#[repr(C)]
#[derive(Copy, Clone, Default)]
pub struct m3g_alignment {
    pub z_target: c_int,
    pub y_target: c_int,
    pub z_reference_id: c_int,
    pub y_reference_id: c_int,
    pub present: c_int,
}

#[repr(C)]
pub struct m3g_node_meta {
    pub transformable: m3g_transformable_meta,
    pub enable_rendering: c_int,
    pub enable_picking: c_int,
    pub alpha_factor: c_int,
    pub scope: u32,
    pub alignment: m3g_alignment,
}

#[repr(C)]
#[derive(Copy, Clone, Default)]
pub struct m3g_tex_coord_binding {
    pub vertex_array_id: c_int,
    pub bias: [f32; 3],
    pub scale: f32,
}

#[repr(C)]
#[derive(Copy, Clone, Default)]
pub struct m3g_submesh_ref {
    pub index_buffer_id: c_int,
    pub appearance_id: c_int,
}

#[repr(C)]
#[derive(Copy, Clone, Default)]
pub struct m3g_bone_transform {
    pub transform_node_id: c_int,
    pub first_vertex: c_int,
    pub vertex_count: c_int,
    pub weight: c_int,
}

#[repr(C)]
pub struct m3g_keyframe {
    pub time: c_int,
    pub values: *mut f32,
    pub value_count: c_int,
}

#[repr(C)]
pub struct m3g_header_data {
    pub version_major: c_int,
    pub version_minor: c_int,
    pub has_external_references: c_int,
    pub total_file_size: u32,
    pub approximate_content_size: u32,
    pub authoring_field: *mut c_char,
}

#[repr(C)]
pub struct m3g_external_ref_data {
    pub uri: *mut c_char,
}

#[repr(C)]
pub struct m3g_group_data {
    pub node: m3g_node_meta,
    pub child_ids: *mut c_int,
    pub child_count: c_int,
    pub active_camera_id: c_int,
    pub background_id: c_int,
    pub is_world: c_int,
}

#[repr(C)]
pub struct m3g_camera_data {
    pub node: m3g_node_meta,
    pub projection_type: c_int,
    pub fov_degrees: f32,
    pub aspect_ratio: f32,
    pub near_distance: f32,
    pub far_distance: f32,
    pub generic_values: [f32; 16],
    pub has_perspective: c_int,
    pub has_generic16: c_int,
}

#[repr(C)]
pub struct m3g_light_data {
    pub node: m3g_node_meta,
    pub attenuation_constant: f32,
    pub attenuation_linear: f32,
    pub attenuation_quadratic: f32,
    pub color: m3g_rgb,
    pub mode: c_int,
    pub intensity: f32,
    pub spot_angle: f32,
    pub spot_exponent: f32,
}

#[repr(C)]
pub struct m3g_background_data {
    pub object3d: m3g_object3d_meta,
    pub background_color: m3g_rgba,
    pub background_image_id: c_int,
    pub image_mode_x: c_int,
    pub image_mode_y: c_int,
    pub crop_x: c_int,
    pub crop_y: c_int,
    pub crop_width: c_int,
    pub crop_height: c_int,
    pub depth_clear_enabled: c_int,
    pub color_clear_enabled: c_int,
}

#[repr(C)]
pub struct m3g_fog_data {
    pub object3d: m3g_object3d_meta,
    pub color: m3g_rgb,
    pub mode: c_int,
    pub density: f32,
    pub near_distance: f32,
    pub far_distance: f32,
}

#[repr(C)]
pub struct m3g_polygon_mode_data {
    pub object3d: m3g_object3d_meta,
    pub culling: c_int,
    pub shading: c_int,
    pub winding: c_int,
    pub two_sided_lighting_enabled: c_int,
    pub local_camera_lighting_enabled: c_int,
    pub perspective_correction_enabled: c_int,
}

#[repr(C)]
pub struct m3g_material_data {
    pub object3d: m3g_object3d_meta,
    pub ambient_color: m3g_rgb,
    pub diffuse_color: m3g_rgba,
    pub emissive_color: m3g_rgb,
    pub specular_color: m3g_rgb,
    pub shininess: f32,
    pub vertex_color_tracking_enabled: c_int,
}

#[repr(C)]
pub struct m3g_vertex_array_data {
    pub object3d: m3g_object3d_meta,
    pub component_size: c_int,
    pub component_count: c_int,
    pub encoding: c_int,
    pub vertex_count: c_int,
    pub components: *mut c_int,
    pub component_total: c_int,
}

#[repr(C)]
pub struct m3g_vertex_buffer_data {
    pub object3d: m3g_object3d_meta,
    pub default_color: m3g_rgba,
    pub positions_id: c_int,
    pub normals_id: c_int,
    pub colors_id: c_int,
    pub position_bias: [f32; 3],
    pub position_scale: f32,
    pub tex_coord_bindings: *mut m3g_tex_coord_binding,
    pub tex_coord_binding_count: c_int,
}

#[repr(C)]
pub struct m3g_triangle_strip_data {
    pub object3d: m3g_object3d_meta,
    pub encoding: c_int,
    pub indices: *mut c_int,
    pub index_count: c_int,
    pub strip_lengths: *mut c_int,
    pub strip_count: c_int,
}

#[repr(C)]
pub struct m3g_appearance_data {
    pub object3d: m3g_object3d_meta,
    pub layer: c_int,
    pub compositing_mode_id: c_int,
    pub fog_id: c_int,
    pub polygon_mode_id: c_int,
    pub material_id: c_int,
    pub texture_ids: *mut c_int,
    pub texture_count: c_int,
}

#[repr(C)]
pub struct m3g_texture2d_data {
    pub transformable: m3g_transformable_meta,
    pub image_id: c_int,
    pub blend_color: m3g_rgb,
    pub blending: c_int,
    pub wrapping_s: c_int,
    pub wrapping_t: c_int,
    pub level_filter: c_int,
    pub image_filter: c_int,
}

#[repr(C)]
pub struct m3g_image2d_data {
    pub object3d: m3g_object3d_meta,
    pub format: c_int,
    pub is_mutable: c_int,
    pub width: c_int,
    pub height: c_int,
    pub palette: *mut u8,
    pub palette_len: c_int,
    pub pixels: *mut u8,
    pub pixels_len: c_int,
    pub has_pixels: c_int,
}

#[repr(C)]
pub struct m3g_mesh_data {
    pub node: m3g_node_meta,
    pub vertex_buffer_id: c_int,
    pub submeshes: *mut m3g_submesh_ref,
    pub submesh_count: c_int,
    pub skeleton_id: c_int,
    pub bone_transforms: *mut m3g_bone_transform,
    pub bone_transform_count: c_int,
    pub is_skinned: c_int,
    pub morph_target_ids: *mut c_int,
    pub morph_weights: *mut f32,
    pub morph_target_count: c_int,
    pub is_morphing: c_int,
}

#[repr(C)]
pub struct m3g_compositing_mode_data {
    pub object3d: m3g_object3d_meta,
    pub depth_test_enabled: c_int,
    pub depth_write_enabled: c_int,
    pub color_write_enabled: c_int,
    pub alpha_write_enabled: c_int,
    pub blending: c_int,
    pub alpha_threshold: c_int,
    pub depth_offset_factor: f32,
    pub depth_offset_units: f32,
}

#[repr(C)]
pub struct m3g_sprite3d_data {
    pub node: m3g_node_meta,
    pub image_id: c_int,
    pub appearance_id: c_int,
    pub is_scaled: c_int,
    pub crop_x: c_int,
    pub crop_y: c_int,
    pub crop_width: c_int,
    pub crop_height: c_int,
}

#[repr(C)]
pub struct m3g_anim_controller_data {
    pub object3d: m3g_object3d_meta,
    pub speed: f32,
    pub weight: f32,
    pub active_interval_start: c_int,
    pub active_interval_end: c_int,
    pub reference_sequence_time: f32,
    pub reference_world_time: c_int,
}

#[repr(C)]
pub struct m3g_anim_track_data {
    pub object3d: m3g_object3d_meta,
    pub keyframe_sequence_id: c_int,
    pub animation_controller_id: c_int,
    pub property_id: c_int,
}

#[repr(C)]
pub struct m3g_keyframe_seq_data {
    pub object3d: m3g_object3d_meta,
    pub interpolation: c_int,
    pub repeat_mode: c_int,
    pub encoding: c_int,
    pub duration: c_int,
    pub valid_range_first: c_int,
    pub valid_range_last: c_int,
    pub component_count: c_int,
    pub keyframes: *mut m3g_keyframe,
    pub keyframe_count: c_int,
}

#[repr(C)]
pub struct m3g_unknown_data {
    pub raw_data: *mut u8,
    pub raw_len: c_int,
}

#[repr(C)]
pub union m3g_object_union {
    pub header: std::mem::ManuallyDrop<m3g_header_data>,
    pub external: std::mem::ManuallyDrop<m3g_external_ref_data>,
    pub group: std::mem::ManuallyDrop<m3g_group_data>,
    pub camera: std::mem::ManuallyDrop<m3g_camera_data>,
    pub light: std::mem::ManuallyDrop<m3g_light_data>,
    pub background: std::mem::ManuallyDrop<m3g_background_data>,
    pub fog: std::mem::ManuallyDrop<m3g_fog_data>,
    pub polygon_mode: std::mem::ManuallyDrop<m3g_polygon_mode_data>,
    pub material: std::mem::ManuallyDrop<m3g_material_data>,
    pub vertex_array: std::mem::ManuallyDrop<m3g_vertex_array_data>,
    pub vertex_buffer: std::mem::ManuallyDrop<m3g_vertex_buffer_data>,
    pub triangle_strip: std::mem::ManuallyDrop<m3g_triangle_strip_data>,
    pub appearance: std::mem::ManuallyDrop<m3g_appearance_data>,
    pub texture2d: std::mem::ManuallyDrop<m3g_texture2d_data>,
    pub image2d: std::mem::ManuallyDrop<m3g_image2d_data>,
    pub mesh: std::mem::ManuallyDrop<m3g_mesh_data>,
    pub compositing_mode: std::mem::ManuallyDrop<m3g_compositing_mode_data>,
    pub sprite3d: std::mem::ManuallyDrop<m3g_sprite3d_data>,
    pub anim_controller: std::mem::ManuallyDrop<m3g_anim_controller_data>,
    pub anim_track: std::mem::ManuallyDrop<m3g_anim_track_data>,
    pub keyframe_seq: std::mem::ManuallyDrop<m3g_keyframe_seq_data>,
    pub unknown: std::mem::ManuallyDrop<m3g_unknown_data>,
}

#[repr(C)]
pub struct m3g_object {
    pub object_id: c_int,
    pub object_type: c_int,
    pub raw_length: c_int,
    pub u: m3g_object_union,
}

#[repr(C)]
pub struct m3g_file {
    pub header: *mut m3g_object,
    pub sections: *mut m3g_section_info,
    pub section_count: c_int,
    pub objects: *mut *mut m3g_object,
    pub object_count: c_int,
    pub object_capacity: c_int,
}

// ---------- scene IR ----------
#[repr(C)]
pub struct m3g_warning {
    pub code: *mut c_char,
    pub message: *mut c_char,
}

#[repr(C)]
pub struct m3g_scene_image {
    pub name: *mut c_char,
    pub object_id: c_int,
    pub is_embedded: c_int,
    pub width: c_int,
    pub height: c_int,
    pub pixels: *mut u8,
    pub pixels_len: c_int,
    pub source_path: *mut c_char,
}

#[repr(C)]
#[derive(Copy, Clone, Default)]
pub struct m3g_scene_sampler {
    pub mag_filter: c_int,
    pub min_filter: c_int,
    pub wrap_s: c_int,
    pub wrap_t: c_int,
}

#[repr(C)]
pub struct m3g_scene_texture {
    pub name: *mut c_char,
    pub image_index: c_int,
    pub sampler_index: c_int,
}

#[repr(C)]
pub struct m3g_scene_material {
    pub name: *mut c_char,
    pub base_color_factor: [f32; 4],
    pub base_color_texture_index: c_int,
    pub emissive_factor: [f32; 3],
    pub roughness_factor: f32,
    pub metallic_factor: f32,
    pub double_sided: c_int,
    pub alpha_mode: *mut c_char,
}

#[repr(C)]
pub struct m3g_scene_primitive {
    pub name: *mut c_char,
    pub positions: *mut f32,
    pub position_count: c_int,
    pub normals: *mut f32,
    pub normal_count: c_int,
    pub tex_coords0: *mut f32,
    pub tex_coord_count: c_int,
    pub vertex_colors: *mut f32,
    pub vertex_color_count: c_int,
    pub indices: *mut c_int,
    pub index_count: c_int,
    pub material_index: c_int,
}

#[repr(C)]
pub struct m3g_scene_mesh {
    pub name: *mut c_char,
    pub primitives: *mut m3g_scene_primitive,
    pub primitive_count: c_int,
}

#[repr(C)]
pub struct m3g_scene_camera {
    pub name: *mut c_char,
    pub has_perspective: c_int,
    pub yfov_radians: f32,
    pub aspect_ratio: f32,
    pub znear: f32,
    pub zfar: f32,
}

#[repr(C)]
pub struct m3g_scene_node {
    pub name: *mut c_char,
    pub matrix: [f32; 16],
    pub has_matrix: c_int,
    pub translation: [f32; 3],
    pub rotation: [f32; 4],
    pub scale: [f32; 3],
    pub has_trs: c_int,
    pub mesh_index: c_int,
    pub camera_index: c_int,
    pub children: *mut c_int,
    pub child_count: c_int,
}

#[repr(C)]
pub struct m3g_scene_anim_sampler {
    pub times: *mut f32,
    pub time_count: c_int,
    pub values: *mut f32,
    pub value_count: c_int,
    pub interpolation: [c_char; 16],
    pub component_count: c_int,
}

#[repr(C)]
pub struct m3g_scene_anim_channel {
    pub sampler_index: c_int,
    pub node_index: c_int,
    pub path: [c_char; 32],
}

#[repr(C)]
pub struct m3g_scene_animation {
    pub name: *mut c_char,
    pub samplers: *mut m3g_scene_anim_sampler,
    pub sampler_count: c_int,
    pub channels: *mut m3g_scene_anim_channel,
    pub channel_count: c_int,
}

#[repr(C)]
pub struct m3g_scene_ir {
    pub nodes: *mut m3g_scene_node,
    pub node_count: c_int,
    pub root_node_indices: *mut c_int,
    pub root_count: c_int,
    pub meshes: *mut m3g_scene_mesh,
    pub mesh_count: c_int,
    pub materials: *mut m3g_scene_material,
    pub material_count: c_int,
    pub textures: *mut m3g_scene_texture,
    pub texture_count: c_int,
    pub images: *mut m3g_scene_image,
    pub image_count: c_int,
    pub samplers: *mut m3g_scene_sampler,
    pub sampler_count: c_int,
    pub cameras: *mut m3g_scene_camera,
    pub camera_count: c_int,
    pub animations: *mut m3g_scene_animation,
    pub animation_count: c_int,
    pub warnings: *mut m3g_warning,
    pub warning_count: c_int,
}

#[repr(C)]
pub struct m3g_decode_options {
    pub pattern_path: *const c_char,
}

#[repr(C)]
pub struct m3g_decoded {
    pub source_path: *mut c_char,
    pub file: m3g_file,
    pub scene: m3g_scene_ir,
}

// ---------- extern C declarations ----------
extern "C" {
    pub fn m3g_last_error() -> *const c_char;

    pub fn m3g_set_deflate_io(io: *const m3g_deflate_io);
    pub fn m3g_get_deflate_io() -> *const m3g_deflate_io;
    pub fn m3g_set_image_io(io: *const m3g_image_io);
    pub fn m3g_get_image_io() -> *const m3g_image_io;
    pub fn m3g_set_math_io(io: *const m3g_math_io);
    pub fn m3g_get_math_io() -> *const m3g_math_io;

    pub fn m3g_install_portable_math_io();
    pub fn m3g_install_vecmath_math_io();
    pub fn m3g_install_miniz_deflate_io();
    pub fn m3g_install_stb_image_io();

    pub fn m3g_type_name(object_type: c_int) -> *const c_char;
    pub fn m3g_rgb_to_float(c: *const m3g_rgb, out: *mut f32);
    pub fn m3g_rgba_to_float(c: *const m3g_rgba, out: *mut f32);

    pub fn m3g_file_free(f: *mut m3g_file);
    pub fn m3g_scene_free(s: *mut m3g_scene_ir);
    pub fn m3g_decoded_free(d: *mut m3g_decoded);

    pub fn m3g_decode_file(
        path: *const c_char,
        options: *const m3g_decode_options,
        out: *mut m3g_decoded,
    ) -> m3g_result;

    pub fn m3g_decode_bytes(
        bytes: *const u8,
        len: size_t,
        source_path: *const c_char,
        options: *const m3g_decode_options,
        out: *mut m3g_decoded,
    ) -> m3g_result;

    pub fn m3g_mat4_identity(m: *mut f32);
    pub fn m3g_mat4_mul(a: *const f32, b: *const f32, out: *mut f32);
    pub fn m3g_mat4_is_identity(m: *const f32, epsilon: f32) -> c_int;
    pub fn m3g_mat4_translation(x: f32, y: f32, z: f32, out: *mut f32);
    pub fn m3g_mat4_scale(x: f32, y: f32, z: f32, out: *mut f32);
    pub fn m3g_mat4_rotation_axis(angle_rad: f32, ax: f32, ay: f32, az: f32, out: *mut f32);
    pub fn m3g_vec3_length(x: f32, y: f32, z: f32) -> f32;
    pub fn m3g_quat_from_axis_angle_deg(
        angle_deg: f32,
        ax: f32,
        ay: f32,
        az: f32,
        q_xyzw: *mut f32,
    );
    pub fn m3g_node_matrix(node: *const m3g_node_meta, out: *mut f32);
    pub fn m3g_decompose_trs(m: *const f32, t: *mut f32, q_xyzw: *mut f32, s: *mut f32);
}
