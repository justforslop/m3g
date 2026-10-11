/*
 * src/debug.cpp — M3G viewer (sokol_app + sokol_gfx + sokol_gl + ImGui + m3g.hpp).
 *
 * Loads via m3g::Loader, flattens scene::SceneIr into lit triangles, draws with
 * sokol_gl. Optional mesh toggles, textures, and a simple directional light.
 *
 *   make debug
 *   ./build/debug assets/90.m3g
 *
 * Controls: LMB orbit · wheel zoom · Esc quit
 */
#define SOKOL_IMPL
#define SOKOL_TIME_IMPL
#define SOKOL_TRACE_HOOKS
#if defined(_WIN32)
#define SOKOL_D3D11
#define SOKOL_WIN32_FORCE_MAIN
#else
#define SOKOL_GLCORE
#endif

#include "sokol/sokol_gfx.h"
#include "sokol/sokol_app.h"
#include "sokol/sokol_log.h"
#include "sokol/sokol_glue.h"
#define SOKOL_GL_IMPL
#include "sokol/util/sokol_gl.h"
#include "sokol/sokol_time.h"

#include "imgui.h"
#define SOKOL_IMGUI_IMPL
#include "sokol/util/sokol_imgui.h"
#define SOKOL_GFX_IMGUI_IMPL
#include "sokol/util/sokol_gfx_imgui.h"
#define SOKOL_APP_IMGUI_IMPL
#include "sokol/util/sokol_app_imgui.h"

#include <m3g.hpp>

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kDeg = kPi / 180.f;
constexpr float kRad = 180.f / kPi;

/* ---- math ---- */

struct Vec3 {
    float x = 0, y = 0, z = 0;
};

struct Mat4 {
    float m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
};

float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

Vec3 vec3_normalize(Vec3 v) {
    const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len < 1e-8f) return {0, 1, 0};
    return {v.x / len, v.y / len, v.z / len};
}

Vec3 light_dir_from_latlon(float lat_deg, float lon_deg) {
    const float lat = lat_deg * kDeg;
    const float lon = lon_deg * kDeg;
    return vec3_normalize({std::cos(lat) * std::sin(lon), std::sin(lat), std::cos(lat) * std::cos(lon)});
}

Mat4 mat4_mul(const Mat4 &a, const Mat4 &b) {
    Mat4 r{};
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col)
            r.m[row * 4 + col] = a.m[row * 4 + 0] * b.m[0 * 4 + col] + a.m[row * 4 + 1] * b.m[1 * 4 + col] +
                                 a.m[row * 4 + 2] * b.m[2 * 4 + col] + a.m[row * 4 + 3] * b.m[3 * 4 + col];
    return r;
}

Mat4 mat4_from_row_major(float const *rm) {
    Mat4 out{};
    if (rm) std::memcpy(out.m, rm, 16 * sizeof(float));
    return out;
}

Vec3 mat4_xform_point(const Mat4 &m, float x, float y, float z) {
    return {m.m[0] * x + m.m[1] * y + m.m[2] * z + m.m[3], m.m[4] * x + m.m[5] * y + m.m[6] * z + m.m[7],
            m.m[8] * x + m.m[9] * y + m.m[10] * z + m.m[11]};
}

Vec3 mat4_xform_dir(const Mat4 &m, float x, float y, float z) {
    return {m.m[0] * x + m.m[1] * y + m.m[2] * z, m.m[4] * x + m.m[5] * y + m.m[6] * z,
            m.m[8] * x + m.m[9] * y + m.m[10] * z};
}

Mat4 mat4_from_trs(const std::optional<std::vector<float>> &t, const std::optional<std::vector<float>> &r,
                   const std::optional<std::vector<float>> &s) {
    const float tx = t && t->size() >= 3 ? (*t)[0] : 0.f;
    const float ty = t && t->size() >= 3 ? (*t)[1] : 0.f;
    const float tz = t && t->size() >= 3 ? (*t)[2] : 0.f;
    const float sx = s && s->size() >= 3 ? (*s)[0] : 1.f;
    const float sy = s && s->size() >= 3 ? (*s)[1] : 1.f;
    const float sz = s && s->size() >= 3 ? (*s)[2] : 1.f;
    const float qx = r && r->size() >= 4 ? (*r)[0] : 0.f;
    const float qy = r && r->size() >= 4 ? (*r)[1] : 0.f;
    const float qz = r && r->size() >= 4 ? (*r)[2] : 0.f;
    const float qw = r && r->size() >= 4 ? (*r)[3] : 1.f;
    const float xx = qx * qx, yy = qy * qy, zz = qz * qz;
    const float xy = qx * qy, xz = qx * qz, yz = qy * qz;
    const float wx = qw * qx, wy = qw * qy, wz = qw * qz;
    Mat4 local{};
    local.m[0] = (1.f - 2.f * (yy + zz)) * sx;
    local.m[1] = (2.f * (xy - wz)) * sy;
    local.m[2] = (2.f * (xz + wy)) * sz;
    local.m[3] = tx;
    local.m[4] = (2.f * (xy + wz)) * sx;
    local.m[5] = (1.f - 2.f * (xx + zz)) * sy;
    local.m[6] = (2.f * (yz - wx)) * sz;
    local.m[7] = ty;
    local.m[8] = (2.f * (xz - wy)) * sx;
    local.m[9] = (2.f * (yz + wx)) * sy;
    local.m[10] = (1.f - 2.f * (xx + yy)) * sz;
    local.m[11] = tz;
    return local;
}

/* ---- app state ---- */

struct TriVertex {
    Vec3 pos{};
    Vec3 nrm{0, 1, 0};
    float u = 0, v = 0;
    float r = 0.8f, g = 0.8f, b = 0.85f;
    int tex = -1; /* scene image index, or -1 */
};

struct GpuImage {
    sg_image img{};
    sg_view view{};
};

struct App {
    bool failed = false;
    char path[1024]{};
    char status[512]{};

    std::optional<m3g::decode::Decoded> decoded;
    std::vector<TriVertex> tris;
    std::vector<GpuImage> gpu_images;
    std::vector<char> mesh_visible; /* 1 = draw */

    sg_sampler sampler{};
    sgl_pipeline pip{};
    sg_pass_action pass_action{};

    /* orbit camera */
    float lat = 20.f, lon = 35.f, dist = 0.f;
    Vec3 center{};
    float radius = 1.f;
    float side_x = 1.f, side_y = 1.f, side_z = 1.f;
    float orig_lat = 20.f, orig_lon = 35.f, orig_dist = 0.f;
    Vec3 orig_center{};

    /* lighting / draw */
    float ambient = 0.35f;
    bool draw_enabled = true;
    bool draw_textures = true;
    bool two_sided_light = true;
    bool light_enabled = true;
    bool light_dbg_draw = true;
    float light_lat = 45.f, light_lon = -45.f, light_intensity = 1.f;
    float light_color[3] = {1.f, 1.f, 1.f};

    bool dragging = false;
    float last_x = 0, last_y = 0;
};

App g;

void set_status(char const *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g.status, sizeof(g.status), fmt, ap);
    va_end(ap);
}

void set_path(char const *p) { snprintf(g.path, sizeof(g.path), "%s", p ? p : ""); }

bool path_readable(char const *p) {
    if (!p || !p[0]) return false;
    FILE *f = std::fopen(p, "rb");
    if (!f) return false;
    std::fclose(f);
    return true;
}

void resolve_input_path(char const *in, char *out, size_t out_n) {
    if (!in || !in[0]) {
        out[0] = 0;
        return;
    }
    if (path_readable(in)) {
        snprintf(out, out_n, "%s", in);
        return;
    }
    char alt[1024];
    snprintf(alt, sizeof(alt), "../%s", in);
    if (path_readable(alt)) {
        snprintf(out, out_n, "%s", alt);
        return;
    }
    snprintf(out, out_n, "%s", in);
}

float camera_pos_span() {
    return std::max(std::max(g.side_x, g.side_y), g.side_z) * 3.f;
}

/* ---- scene → triangles ---- */

int material_image_index(const m3g::scene::SceneIr &scene, const m3g::scene::ScenePrimitiveIr &prim) {
    if (!prim.material_index) return -1;
    const int mi = *prim.material_index;
    if (mi < 0 || static_cast<std::size_t>(mi) >= scene.materials.size()) return -1;
    auto const &mat = scene.materials[static_cast<std::size_t>(mi)];
    if (!mat.base_color_texture_index) return -1;
    const int ti = *mat.base_color_texture_index;
    if (ti < 0 || static_cast<std::size_t>(ti) >= scene.textures.size()) return -1;
    const int ii = scene.textures[static_cast<std::size_t>(ti)].image_index;
    if (ii < 0 || static_cast<std::size_t>(ii) >= scene.images.size()) return -1;
    return ii;
}

void append_mesh(const m3g::scene::SceneIr &scene, int mesh_index, const Mat4 &world) {
    if (mesh_index < 0 || static_cast<std::size_t>(mesh_index) >= scene.meshes.size()) return;
    auto const &mesh = scene.meshes[static_cast<std::size_t>(mesh_index)];

    for (auto const &prim : mesh.primitives) {
        float cr = 0.75f, cg = 0.78f, cb = 0.85f;
        if (prim.material_index) {
            const int mi = *prim.material_index;
            if (mi >= 0 && static_cast<std::size_t>(mi) < scene.materials.size()) {
                auto const &bc = scene.materials[static_cast<std::size_t>(mi)].base_color_factor;
                if (bc.size() >= 3) {
                    cr = bc[0];
                    cg = bc[1];
                    cb = bc[2];
                }
            }
        }
        const int tex = material_image_index(scene, prim);
        const int nverts = static_cast<int>(prim.positions.size() / 3);
        const int nidx = static_cast<int>(prim.indices.size());

        for (int i = 0; i + 2 < nidx; i += 3) {
            TriVertex tv[3];
            bool ok = true;
            for (int k = 0; k < 3; ++k) {
                const int vi = prim.indices[static_cast<std::size_t>(i + k)];
                if (vi < 0 || vi >= nverts) {
                    ok = false;
                    break;
                }
                const float px = prim.positions[static_cast<std::size_t>(vi * 3 + 0)];
                const float py = prim.positions[static_cast<std::size_t>(vi * 3 + 1)];
                const float pz = prim.positions[static_cast<std::size_t>(vi * 3 + 2)];
                tv[k].pos = mat4_xform_point(world, px, py, pz);

                if (prim.normals && static_cast<int>(prim.normals->size()) >= vi * 3 + 3) {
                    const float nx = (*prim.normals)[static_cast<std::size_t>(vi * 3 + 0)];
                    const float ny = (*prim.normals)[static_cast<std::size_t>(vi * 3 + 1)];
                    const float nz = (*prim.normals)[static_cast<std::size_t>(vi * 3 + 2)];
                    tv[k].nrm = vec3_normalize(mat4_xform_dir(world, nx, ny, nz));
                }

                float vr = cr, vg = cg, vb = cb;
                if (prim.vertex_colors && !prim.vertex_colors->empty()) {
                    const int vcc = static_cast<int>(prim.vertex_colors->size());
                    const int comps = (vcc == nverts * 4) ? 4 : 3;
                    if (vi * comps + 2 < vcc) {
                        vr = (*prim.vertex_colors)[static_cast<std::size_t>(vi * comps + 0)];
                        vg = (*prim.vertex_colors)[static_cast<std::size_t>(vi * comps + 1)];
                        vb = (*prim.vertex_colors)[static_cast<std::size_t>(vi * comps + 2)];
                    }
                }
                tv[k].r = vr;
                tv[k].g = vg;
                tv[k].b = vb;
                tv[k].tex = tex;
                if (prim.tex_coords0 && static_cast<int>(prim.tex_coords0->size()) >= vi * 2 + 2) {
                    tv[k].u = (*prim.tex_coords0)[static_cast<std::size_t>(vi * 2 + 0)];
                    tv[k].v = (*prim.tex_coords0)[static_cast<std::size_t>(vi * 2 + 1)];
                }
            }
            if (!ok) continue;

            if (!prim.normals) {
                const float e1x = tv[1].pos.x - tv[0].pos.x, e1y = tv[1].pos.y - tv[0].pos.y,
                            e1z = tv[1].pos.z - tv[0].pos.z;
                const float e2x = tv[2].pos.x - tv[0].pos.x, e2y = tv[2].pos.y - tv[0].pos.y,
                            e2z = tv[2].pos.z - tv[0].pos.z;
                Vec3 fn{e1y * e2z - e1z * e2y, e1z * e2x - e1x * e2z, e1x * e2y - e1y * e2x};
                fn = vec3_normalize(fn);
                tv[0].nrm = tv[1].nrm = tv[2].nrm = fn;
            }
            g.tris.push_back(tv[0]);
            g.tris.push_back(tv[1]);
            g.tris.push_back(tv[2]);
        }
    }
}

void walk_node(const m3g::scene::SceneIr &scene, int node_index, const Mat4 &parent) {
    if (node_index < 0 || static_cast<std::size_t>(node_index) >= scene.nodes.size()) return;
    auto const &node = scene.nodes[static_cast<std::size_t>(node_index)];

    Mat4 local{};
    if (node.matrix.has_value() && node.matrix->size() >= 16)
        local = mat4_from_row_major(node.matrix->data());
    else if (node.translation.has_value() || node.rotation.has_value() || node.scale.has_value())
        local = mat4_from_trs(node.translation, node.rotation, node.scale);

    const Mat4 world = mat4_mul(parent, local);

    if (node.mesh_index.has_value()) {
        const int mi = *node.mesh_index;
        const bool visible =
            mi < 0 || mi >= static_cast<int>(g.mesh_visible.size()) || g.mesh_visible[static_cast<std::size_t>(mi)];
        if (visible) append_mesh(scene, mi, world);
    }
    for (int ci : node.children) {
        if (ci < 0 || static_cast<std::size_t>(ci) >= scene.nodes.size()) continue;
        walk_node(scene, ci, world);
    }
}

void compute_bounds() {
    if (g.tris.empty()) {
        g.center = {};
        g.radius = 1.f;
        g.side_x = g.side_y = g.side_z = 1.f;
        return;
    }
    Vec3 mn{1e30f, 1e30f, 1e30f}, mx{-1e30f, -1e30f, -1e30f};
    for (auto const &v : g.tris) {
        mn.x = std::min(mn.x, v.pos.x);
        mn.y = std::min(mn.y, v.pos.y);
        mn.z = std::min(mn.z, v.pos.z);
        mx.x = std::max(mx.x, v.pos.x);
        mx.y = std::max(mx.y, v.pos.y);
        mx.z = std::max(mx.z, v.pos.z);
    }
    g.center = {0.5f * (mn.x + mx.x), 0.5f * (mn.y + mx.y), 0.5f * (mn.z + mx.z)};
    const float dx = mx.x - mn.x, dy = mx.y - mn.y, dz = mx.z - mn.z;
    g.side_x = std::max(dx, 1e-3f);
    g.side_y = std::max(dy, 1e-3f);
    g.side_z = std::max(dz, 1e-3f);
    g.radius = 0.5f * std::sqrt(dx * dx + dy * dy + dz * dz);
    if (g.radius < 1e-3f) g.radius = 1.f;
}

void build_draw_list() {
    g.tris.clear();
    if (!g.decoded) return;

    auto const &scene = g.decoded->scene_ir;
    g.mesh_visible.resize(scene.meshes.size(), 1);

    Mat4 identity{};
    if (!scene.root_node_indices.empty()) {
        for (int ri : scene.root_node_indices) {
            if (ri < 0 || static_cast<std::size_t>(ri) >= scene.nodes.size()) continue;
            walk_node(scene, ri, identity);
        }
    } else {
        for (int i = 0; i < static_cast<int>(scene.nodes.size()); ++i) walk_node(scene, i, identity);
    }

    compute_bounds();
    if (g.orig_dist <= 0.f) {
        g.dist = g.radius * 2.8f;
        g.lat = 20.f;
        g.lon = 35.f;
        g.orig_lat = g.lat;
        g.orig_lon = g.lon;
        g.orig_dist = g.dist;
        g.orig_center = g.center;
    }
    set_status("%s\ntris=%d  nodes=%zu  meshes=%zu  mats=%zu\nLMB orbit  wheel zoom  Esc quit", g.path,
               static_cast<int>(g.tris.size() / 3), scene.nodes.size(), scene.meshes.size(), scene.materials.size());
}

/* ---- GPU textures ---- */

void destroy_gpu_images() {
    for (auto &gpu : g.gpu_images) {
        if (gpu.view.id) sg_destroy_view(gpu.view);
        if (gpu.img.id) sg_destroy_image(gpu.img);
        gpu = {};
    }
    g.gpu_images.clear();
    if (g.sampler.id) {
        sg_destroy_sampler(g.sampler);
        g.sampler = {};
    }
}

void create_gpu_images() {
    destroy_gpu_images();
    sg_sampler_desc smp = {};
    smp.min_filter = SG_FILTER_LINEAR;
    smp.mag_filter = SG_FILTER_LINEAR;
    smp.wrap_u = SG_WRAP_REPEAT;
    smp.wrap_v = SG_WRAP_REPEAT;
    g.sampler = sg_make_sampler(&smp);

    if (!g.decoded) return;
    auto const &scene = g.decoded->scene_ir;
    g.gpu_images.resize(scene.images.size());
    for (std::size_t i = 0; i < scene.images.size(); ++i) {
        auto const &src = scene.images[i];
        if (!src.embedded) continue;
        auto const &emb = *src.embedded;
        if (emb.width <= 0 || emb.height <= 0 ||
            static_cast<int>(emb.pixels.size()) != emb.width * emb.height * 4)
            continue;

        sg_image_desc desc = {};
        desc.width = emb.width;
        desc.height = emb.height;
        desc.pixel_format = SG_PIXELFORMAT_RGBA8;
        desc.data.mip_levels[0].ptr = emb.pixels.data();
        desc.data.mip_levels[0].size = emb.pixels.size();
        g.gpu_images[i].img = sg_make_image(&desc);

        sg_view_desc view = {};
        view.texture.image = g.gpu_images[i].img;
        g.gpu_images[i].view = sg_make_view(&view);
    }
}

/* ---- load ---- */

bool load_m3g(char const *path) {
    char resolved[1024];
    resolve_input_path(path, resolved, sizeof(resolved));
    set_path(resolved);
    g.decoded.reset();
    g.tris.clear();
    destroy_gpu_images();

    if (!path_readable(g.path)) {
        g.failed = true;
        set_status("failed to open %s (try path from repo root or ../assets/...)", path && path[0] ? path : "(empty)");
        return false;
    }

    try {
        m3g::install_miniz_deflate_io();
        m3g::install_stb_image_io();
        g.decoded = m3g::Loader::load(g.path);
    } catch (std::exception const &ex) {
        g.failed = true;
        set_status("load failed: %s", ex.what());
        g.decoded.reset();
        return false;
    }

    g.orig_dist = 0.f;
    build_draw_list();
    if (sg_isvalid()) create_gpu_images();
    g.failed = false;
    return true;
}

/* ---- UI ---- */

void draw_ui() {
    sappimgui_track_frame();
    if (ImGui::BeginMainMenuBar()) {
        sgimgui_draw_menu("sokol-gfx");
        sappimgui_draw_menu("sokol-app");
        ImGui::EndMainMenuBar();
    }
    sgimgui_draw();
    sappimgui_draw();

    ImGui::SetNextWindowPos(ImVec2(20, 28), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(320, 420), ImGuiCond_Once);
    ImGui::SetNextWindowBgAlpha(0.4f);
    if (!ImGui::Begin("m3g debug", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    if (g.failed || !g.decoded) {
        ImGui::TextWrapped("%s", g.status);
        ImGui::End();
        return;
    }

    auto const &scene = g.decoded->scene_ir;
    ImGui::TextUnformatted(g.path);
    ImGui::Text("tris %d  nodes %zu  meshes %zu", static_cast<int>(g.tris.size() / 3), scene.nodes.size(),
                scene.meshes.size());
    ImGui::Text("mats %zu  images %zu  anims %zu", scene.materials.size(), scene.images.size(),
                scene.animations.size());
    ImGui::Text("frame %.2f ms", sapp_frame_duration() * 1000.0);
    ImGui::Separator();

    ImGui::Checkbox("Draw mesh", &g.draw_enabled);
    ImGui::Checkbox("Draw textures", &g.draw_textures);
    ImGui::Checkbox("Two-sided light", &g.two_sided_light);
    ImGui::SliderFloat("Ambient", &g.ambient, 0.f, 1.f, "%.2f");
    ImGui::Separator();

    ImGui::PushStyleColor(ImGuiCol_CheckMark, IM_COL32(0, 255, 0, 255));
    ImGui::Checkbox("Enable Lighting", &g.light_enabled);
    ImGui::PopStyleColor();
    if (g.light_enabled) {
        ImGui::Checkbox("Draw Light Vector", &g.light_dbg_draw);
        ImGui::SliderFloat("Light Lat", &g.light_lat, -85.f, 85.f, "%.1f");
        ImGui::SliderFloat("Light Lon", &g.light_lon, 0.f, 360.f, "%.1f");
        ImGui::SliderFloat("Intensity", &g.light_intensity, 0.f, 10.f, "%.1f");
        ImGui::ColorEdit3("Light Color", g.light_color);
    }
    ImGui::Separator();

    ImGui::Text("Camera (LMB orbit, wheel zoom)");
    ImGui::SliderFloat("Distance", &g.dist, g.radius * 0.2f, g.radius * 50.f, "%.1f");
    ImGui::SliderFloat("Latitude", &g.lat, -89.f, 89.f, "%.1f");
    ImGui::SliderFloat("Longitude", &g.lon, -360.f, 360.f, "%.1f");
    ImGui::Separator();

    {
        const float span = std::max(camera_pos_span(), 1e-3f);
        const Vec3 pivot = (g.orig_dist > 0.f) ? g.orig_center : g.center;
        const float clat = g.lat * kDeg;
        const float clon = g.lon * kDeg;
        float pos[3] = {
            g.center.x + g.dist * std::cos(clat) * std::sin(clon),
            g.center.y + g.dist * std::sin(clat),
            g.center.z + g.dist * std::cos(clat) * std::cos(clon),
        };
        const bool moved = ImGui::SliderFloat("Pos X", &pos[0], pivot.x - span, pivot.x + span, "%.2f") ||
                           ImGui::SliderFloat("Pos Y", &pos[1], pivot.y - span, pivot.y + span, "%.2f") ||
                           ImGui::SliderFloat("Pos Z", &pos[2], pivot.z - span, pivot.z + span, "%.2f");
        if (moved) {
            const float dx = pos[0] - g.center.x, dy = pos[1] - g.center.y, dz = pos[2] - g.center.z;
            float d = std::sqrt(dx * dx + dy * dy + dz * dz);
            d = std::max(d, g.radius * 0.2f);
            g.dist = d;
            g.lat = std::asin(clampf(dy / d, -1.f, 1.f)) * kRad;
            g.lon = std::atan2(dx, dz) * kRad;
        }
        ImGui::Separator();
        ImGui::SliderFloat("Look X", &g.center.x, pivot.x - span, pivot.x + span, "%.2f");
        ImGui::SliderFloat("Look Y", &g.center.y, pivot.y - span, pivot.y + span, "%.2f");
        ImGui::SliderFloat("Look Z", &g.center.z, pivot.z - span, pivot.z + span, "%.2f");
        if (ImGui::Button("Restore original position")) {
            g.lat = g.orig_lat;
            g.lon = g.orig_lon;
            g.dist = g.orig_dist;
            g.center = g.orig_center;
        }
    }
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Meshes", ImGuiTreeNodeFlags_DefaultOpen)) {
        bool changed = false;
        for (std::size_t i = 0; i < scene.meshes.size() && i < g.mesh_visible.size(); ++i) {
            bool vis = g.mesh_visible[i] != 0;
            char label[256];
            char const *name = scene.meshes[i].name.empty() ? "mesh" : scene.meshes[i].name.c_str();
            snprintf(label, sizeof(label), "%s##mesh%zu", name, i);
            if (ImGui::Checkbox(label, &vis)) {
                g.mesh_visible[i] = vis ? 1 : 0;
                changed = true;
            }
        }
        if (changed) build_draw_list();
    }

    if (ImGui::CollapsingHeader("Images")) {
        for (std::size_t i = 0; i < g.gpu_images.size(); ++i) {
            if (!g.gpu_images[i].view.id) continue;
            char const *iname = "image";
            if (i < scene.images.size() && !scene.images[i].name.empty()) iname = scene.images[i].name.c_str();
            ImGui::Text("%zu %s", i, iname);
            ImGui::Image(ImTextureRef(simgui_imtextureid(g.gpu_images[i].view)), ImVec2(128, 128));
        }
    }

    if (!g.decoded->warnings().empty() && ImGui::CollapsingHeader("Warnings")) {
        for (auto const &w : g.decoded->warnings()) ImGui::TextWrapped("- %s", w.message.c_str());
    }

    ImGui::End();
}

/* ---- draw ---- */

void draw_light_debug() {
    if (!g.light_enabled || !g.light_dbg_draw) return;
    const Vec3 dir = light_dir_from_latlon(g.light_lat, g.light_lon);
    const float len = g.radius * 0.75f;
    sgl_disable_texture();
    sgl_c3f(g.light_color[0], g.light_color[1], g.light_color[2]);
    sgl_begin_lines();
    sgl_v3f(g.center.x, g.center.y, g.center.z);
    sgl_v3f(g.center.x + dir.x * len, g.center.y + dir.y * len, g.center.z + dir.z * len);
    sgl_end();
}

void emit_lit_vertex(TriVertex const &v, Vec3 const &light, bool textured) {
    float ndl = 1.f, lr = 1.f, lg = 1.f, lb = 1.f;
    if (g.light_enabled) {
        ndl = v.nrm.x * light.x + v.nrm.y * light.y + v.nrm.z * light.z;
        if (g.two_sided_light) ndl = std::fabs(ndl);
        ndl = std::max(g.ambient, ndl) * g.light_intensity;
        lr = g.light_color[0];
        lg = g.light_color[1];
        lb = g.light_color[2];
    }
    const float cr = v.r * ndl * lr, cg = v.g * ndl * lg, cb = v.b * ndl * lb;
    if (textured)
        sgl_v3f_t2f_c3f(v.pos.x, v.pos.y, v.pos.z, v.u, v.v, cr, cg, cb);
    else
        sgl_v3f_c3f(v.pos.x, v.pos.y, v.pos.z, cr, cg, cb);
}

void draw_scene() {
    if (!g.draw_enabled) return;
    const Vec3 light = light_dir_from_latlon(g.light_lat, g.light_lon);

    sgl_disable_texture();
    sgl_begin_triangles();
    for (auto const &v : g.tris)
        if (v.tex < 0 || !g.draw_textures) emit_lit_vertex(v, light, false);
    sgl_end();

    if (!g.draw_textures) return;
    for (int tex = 0; tex < static_cast<int>(g.gpu_images.size()); ++tex) {
        if (!g.gpu_images[static_cast<std::size_t>(tex)].view.id) continue;
        bool any = false;
        for (auto const &v : g.tris)
            if (v.tex == tex) {
                any = true;
                break;
            }
        if (!any) continue;

        sgl_enable_texture();
        sgl_texture(g.gpu_images[static_cast<std::size_t>(tex)].view, g.sampler);
        sgl_begin_triangles();
        for (auto const &v : g.tris)
            if (v.tex == tex) emit_lit_vertex(v, light, true);
        sgl_end();
    }
    sgl_disable_texture();
}

/* ---- sokol callbacks ---- */

void init() {
    m3g::install_miniz_deflate_io();
    m3g::install_stb_image_io();

    sg_desc sgdesc = {};
    sgdesc.environment = sglue_environment();
    sgdesc.logger.func = slog_func;
    sg_setup(&sgdesc);
    stm_setup();

    sgl_desc_t sgldesc = {};
    sgldesc.max_vertices = 1 << 20;
    sgldesc.max_commands = 1 << 16;
    sgldesc.logger.func = slog_func;
    sgl_setup(&sgldesc);

    sg_pipeline_desc pip = {};
    pip.cull_mode = SG_CULLMODE_NONE;
    pip.face_winding = SG_FACEWINDING_CCW;
    pip.depth.write_enabled = true;
    pip.depth.compare = SG_COMPAREFUNC_LESS_EQUAL;
    g.pip = sgl_make_pipeline(&pip);

    sappimgui_setup();
    sgimgui_desc_t sgimgui_desc = {};
    sgimgui_setup(&sgimgui_desc);
    simgui_desc_t imdesc = {};
    imdesc.logger.func = slog_func;
    simgui_setup(&imdesc);

    g.pass_action = {};
    g.pass_action.colors[0].load_action = SG_LOADACTION_CLEAR;
    g.pass_action.colors[0].clear_value = {0.12f, 0.14f, 0.18f, 1.0f};

    if (g.path[0])
        load_m3g(g.path);
    else {
        g.failed = true;
        set_status("Usage: m3g-view <file.m3g>");
    }
}

void frame() {
    const float aspect = sapp_heightf() > 1.f ? sapp_widthf() / sapp_heightf() : 1.f;
    const float lat_r = g.lat * kDeg;
    const float lon_r = g.lon * kDeg;
    const Vec3 eye{
        g.center.x + g.dist * std::cos(lat_r) * std::sin(lon_r),
        g.center.y + g.dist * std::sin(lat_r),
        g.center.z + g.dist * std::cos(lat_r) * std::cos(lon_r),
    };

    sgl_defaults();
    if (g.pip.id) sgl_load_pipeline(g.pip);
    sgl_matrix_mode_projection();
    sgl_perspective(sgl_rad(50.f), aspect, g.radius * 0.01f, g.radius * 100.f);
    sgl_matrix_mode_modelview();
    sgl_lookat(eye.x, eye.y, eye.z, g.center.x, g.center.y, g.center.z, 0.f, 1.f, 0.f);

    draw_scene();
    draw_light_debug();

    simgui_frame_desc_t fd = {};
    fd.width = sapp_width();
    fd.height = sapp_height();
    fd.delta_time = sapp_frame_duration();
    fd.dpi_scale = sapp_dpi_scale();
    simgui_new_frame(&fd);
    draw_ui();
    simgui_flush();

    sg_pass pass = {};
    pass.action = g.pass_action;
    pass.swapchain = sglue_swapchain();
    sg_begin_pass(&pass);
    sgl_draw();
    simgui_draw();
    sg_end_pass();
    sg_commit();
}

void cleanup() {
    destroy_gpu_images();
    g.tris.clear();
    g.mesh_visible.clear();
    g.decoded.reset();
    if (g.pip.id) {
        sgl_destroy_pipeline(g.pip);
        g.pip = {};
    }
    sappimgui_shutdown();
    sgimgui_shutdown();
    simgui_shutdown();
    sgl_shutdown();
    sg_shutdown();
}

void event(const sapp_event *ev) {
    sappimgui_track_event(ev);
    if (simgui_handle_event(ev)) return;

    switch (ev->type) {
    case SAPP_EVENTTYPE_KEY_DOWN:
        if (ev->key_code == SAPP_KEYCODE_ESCAPE) sapp_request_quit();
        break;
    case SAPP_EVENTTYPE_MOUSE_DOWN:
        if (ev->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
            g.dragging = true;
            g.last_x = ev->mouse_x;
            g.last_y = ev->mouse_y;
        }
        break;
    case SAPP_EVENTTYPE_MOUSE_UP:
        if (ev->mouse_button == SAPP_MOUSEBUTTON_LEFT) g.dragging = false;
        break;
    case SAPP_EVENTTYPE_MOUSE_MOVE:
        if (g.dragging) {
            g.lon += (ev->mouse_x - g.last_x) * 0.4f;
            g.lat = clampf(g.lat + (ev->mouse_y - g.last_y) * 0.4f, -89.f, 89.f);
            g.last_x = ev->mouse_x;
            g.last_y = ev->mouse_y;
        }
        break;
    case SAPP_EVENTTYPE_MOUSE_SCROLL:
        g.dist *= (ev->scroll_y > 0.f) ? 0.9f : 1.1f;
        g.dist = clampf(g.dist, g.radius * 0.2f, g.radius * 50.f);
        break;
    default:
        break;
    }
}

} // namespace

extern "C" sapp_desc sokol_main(int argc, char *argv[]) {
    g = App{};
    if (argc >= 2) set_path(argv[1]);

    sapp_desc desc = {};
    desc.init_cb = init;
    desc.frame_cb = frame;
    desc.cleanup_cb = cleanup;
    desc.event_cb = event;
    desc.width = 1024;
    desc.height = 720;
#if defined(_WIN32)
    desc.sample_count = 1;
#else
    desc.sample_count = 4;
#endif
    desc.window_title = "m3g M3G viewer";
    desc.icon.sokol_default = true;
    desc.logger.func = slog_func;
#if defined(SOKOL_GLCORE)
    desc.gl.major_version = 3;
    desc.gl.minor_version = 3;
#endif
    return desc;
}
