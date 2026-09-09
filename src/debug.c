/*
 * src/debug.c — simple M3G viewer (sokol_app + sokol_gfx + sokol_gl + slop decode).
 *
 * Inspired by sokol-samples cgltf-sapp, but loads M3G via the public decode API
 * and draws with sokol_gl (no basisu / shdc / dbgui).
 *
 * Build:  make view
 * Run:    ./out/slop-view assets/90.m3g
 *
 * Controls:
 *   LMB drag  — orbit
 *   wheel     — zoom
 *   Esc       — quit
 *
 * Compiled as C++ (g++ -x c++) so we can use <slop/decode.hpp>.
 */
#define SOKOL_IMPL
#define SOKOL_GLCORE
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_app.h"
#include "sokol/sokol_log.h"
#include "sokol/sokol_glue.h"
#define SOKOL_GL_IMPL
#include "sokol/util/sokol_gl.h"

#include <slop/decode.hpp>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

struct Vec3 {
    float x = 0, y = 0, z = 0;
};

struct Mat4 {
    float m[16] = {
        1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1,
    };
};

Mat4 mat4_mul(const Mat4 &a, const Mat4 &b) {
    Mat4 r{};
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            float s = 0.f;
            for (int k = 0; k < 4; ++k) {
                s += a.m[row * 4 + k] * b.m[k * 4 + col];
            }
            r.m[row * 4 + col] = s;
        }
    }
    return r;
}

Mat4 mat4_from_row_major(const std::vector<float> &rm) {
    Mat4 out{};
    if (rm.size() == 16) {
        for (int i = 0; i < 16; ++i) {
            out.m[i] = rm[static_cast<std::size_t>(i)];
        }
    }
    return out;
}

Vec3 mat4_transform_point(const Mat4 &m, float x, float y, float z) {
    return Vec3{
        m.m[0] * x + m.m[1] * y + m.m[2] * z + m.m[3],
        m.m[4] * x + m.m[5] * y + m.m[6] * z + m.m[7],
        m.m[8] * x + m.m[9] * y + m.m[10] * z + m.m[11],
    };
}

Vec3 mat4_transform_dir(const Mat4 &m, float x, float y, float z) {
    return Vec3{
        m.m[0] * x + m.m[1] * y + m.m[2] * z,
        m.m[4] * x + m.m[5] * y + m.m[6] * z,
        m.m[8] * x + m.m[9] * y + m.m[10] * z,
    };
}

Vec3 vec3_normalize(Vec3 v) {
    const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len < 1e-8f) {
        return Vec3{0, 1, 0};
    }
    return Vec3{v.x / len, v.y / len, v.z / len};
}

struct TriVertex {
    Vec3 pos;
    Vec3 nrm;
    float r = 0.8f, g = 0.8f, b = 0.85f;
};

struct App {
    bool failed = false;
    std::string path;
    std::string status;
    slop::decode::Decoded decoded;
    std::vector<TriVertex> tris; // expanded triangle list (3 verts each)

    float lat = 20.f;
    float lon = 35.f;
    float dist = 0.f;
    Vec3 center{};
    float radius = 1.f;

    bool dragging = false;
    float last_x = 0, last_y = 0;

    sg_pass_action pass_action{};
};

App g;

void compute_bounds(const std::vector<TriVertex> &tris, Vec3 *out_center, float *out_radius) {
    if (tris.empty()) {
        *out_center = {};
        *out_radius = 1.f;
        return;
    }
    Vec3 mn{1e30f, 1e30f, 1e30f};
    Vec3 mx{-1e30f, -1e30f, -1e30f};
    for (const auto &v : tris) {
        mn.x = std::min(mn.x, v.pos.x);
        mn.y = std::min(mn.y, v.pos.y);
        mn.z = std::min(mn.z, v.pos.z);
        mx.x = std::max(mx.x, v.pos.x);
        mx.y = std::max(mx.y, v.pos.y);
        mx.z = std::max(mx.z, v.pos.z);
    }
    out_center->x = 0.5f * (mn.x + mx.x);
    out_center->y = 0.5f * (mn.y + mx.y);
    out_center->z = 0.5f * (mn.z + mx.z);
    const float dx = mx.x - mn.x;
    const float dy = mx.y - mn.y;
    const float dz = mx.z - mn.z;
    *out_radius = 0.5f * std::sqrt(dx * dx + dy * dy + dz * dz);
    if (*out_radius < 1e-3f) {
        *out_radius = 1.f;
    }
}

void append_mesh(const slop::scene::SceneIr &scene, int mesh_index, const Mat4 &world,
                 const float base_color[3], std::vector<TriVertex> *out) {
    if (mesh_index < 0 || mesh_index >= static_cast<int>(scene.meshes.size())) {
        return;
    }
    const auto &mesh = scene.meshes[static_cast<std::size_t>(mesh_index)];
    for (const auto &prim : mesh.primitives) {
        float r = base_color[0], g = base_color[1], b = base_color[2];
        if (prim.material_index && *prim.material_index >= 0 &&
            *prim.material_index < static_cast<int>(scene.materials.size())) {
            const auto &mat = scene.materials[static_cast<std::size_t>(*prim.material_index)];
            if (mat.base_color_factor.size() >= 3) {
                r = mat.base_color_factor[0];
                g = mat.base_color_factor[1];
                b = mat.base_color_factor[2];
            }
        }
        const int nidx = static_cast<int>(prim.indices.size());
        for (int i = 0; i + 2 < nidx; i += 3) {
            TriVertex tv[3];
            for (int k = 0; k < 3; ++k) {
                const int vi = prim.indices[static_cast<std::size_t>(i + k)];
                const float px = prim.positions[static_cast<std::size_t>(vi * 3 + 0)];
                const float py = prim.positions[static_cast<std::size_t>(vi * 3 + 1)];
                const float pz = prim.positions[static_cast<std::size_t>(vi * 3 + 2)];
                tv[k].pos = mat4_transform_point(world, px, py, pz);
                if (prim.normals &&
                    static_cast<std::size_t>(vi * 3 + 2) < prim.normals->size()) {
                    const float nx = (*prim.normals)[static_cast<std::size_t>(vi * 3 + 0)];
                    const float ny = (*prim.normals)[static_cast<std::size_t>(vi * 3 + 1)];
                    const float nz = (*prim.normals)[static_cast<std::size_t>(vi * 3 + 2)];
                    tv[k].nrm = vec3_normalize(mat4_transform_dir(world, nx, ny, nz));
                } else {
                    tv[k].nrm = {0, 1, 0};
                }
                tv[k].r = r;
                tv[k].g = g;
                tv[k].b = b;
            }
            // flat normal fallback if missing
            if (!prim.normals) {
                const float e1x = tv[1].pos.x - tv[0].pos.x;
                const float e1y = tv[1].pos.y - tv[0].pos.y;
                const float e1z = tv[1].pos.z - tv[0].pos.z;
                const float e2x = tv[2].pos.x - tv[0].pos.x;
                const float e2y = tv[2].pos.y - tv[0].pos.y;
                const float e2z = tv[2].pos.z - tv[0].pos.z;
                Vec3 fn{e1y * e2z - e1z * e2y, e1z * e2x - e1x * e2z, e1x * e2y - e1y * e2x};
                fn = vec3_normalize(fn);
                tv[0].nrm = tv[1].nrm = tv[2].nrm = fn;
            }
            out->push_back(tv[0]);
            out->push_back(tv[1]);
            out->push_back(tv[2]);
        }
    }
}

void walk_node(const slop::scene::SceneIr &scene, int node_index, const Mat4 &parent,
               std::vector<TriVertex> *out) {
    if (node_index < 0 || node_index >= static_cast<int>(scene.nodes.size())) {
        return;
    }
    const auto &node = scene.nodes[static_cast<std::size_t>(node_index)];
    Mat4 local = node.matrix ? mat4_from_row_major(*node.matrix) : Mat4{};
    Mat4 world = mat4_mul(parent, local);
    if (node.mesh_index) {
        const float defc[3] = {0.75f, 0.78f, 0.85f};
        append_mesh(scene, *node.mesh_index, world, defc, out);
    }
    for (int child : node.children) {
        walk_node(scene, child, world, out);
    }
}

void build_draw_list(void) {
    g.tris.clear();
    const auto &scene = g.decoded.scene_ir;
    Mat4 identity{};
    if (!scene.root_node_indices.empty()) {
        for (int root : scene.root_node_indices) {
            walk_node(scene, root, identity, &g.tris);
        }
    } else {
        for (int i = 0; i < static_cast<int>(scene.nodes.size()); ++i) {
            walk_node(scene, i, identity, &g.tris);
        }
    }
    compute_bounds(g.tris, &g.center, &g.radius);
    g.dist = g.radius * 2.8f;
    char buf[256];
    std::snprintf(buf, sizeof(buf),
                  "%s\ntris=%zu  nodes=%zu  meshes=%zu  mats=%zu\nLMB orbit  wheel zoom  Esc quit",
                  g.path.c_str(), g.tris.size() / 3, g.decoded.node_count(), g.decoded.mesh_count(),
                  g.decoded.material_count());
    g.status = buf;
}

bool load_m3g(const char *path) {
    g.path = path ? path : "";
    try {
        g.decoded = slop::decode::Decoder{}.decode_file(g.path);
        build_draw_list();
        g.failed = false;
        return true;
    } catch (const std::exception &ex) {
        g.failed = true;
        g.status = std::string("load failed: ") + ex.what();
        g.tris.clear();
        return false;
    }
}

void init(void) {
    sg_desc sgdesc = {};
    sgdesc.environment = sglue_environment();
    sgdesc.logger.func = slog_func;
    sg_setup(&sgdesc);

    sgl_desc_t sgldesc = {};
    sgldesc.max_vertices = 1 << 20;
    sgldesc.max_commands = 1 << 16;
    sgldesc.logger.func = slog_func;
    sgl_setup(&sgldesc);

    g.pass_action = {};
    g.pass_action.colors[0].load_action = SG_LOADACTION_CLEAR;
    g.pass_action.colors[0].clear_value = {0.12f, 0.14f, 0.18f, 1.0f};

    if (!g.path.empty()) {
        load_m3g(g.path.c_str());
    } else {
        g.failed = true;
        g.status = "Usage: slop-view <file.m3g>";
    }
}

void frame(void) {
    const float w = sapp_widthf();
    const float h = sapp_heightf();
    const float aspect = h > 1.f ? w / h : 1.f;

    const float lat_r = g.lat * 0.01745329252f;
    const float lon_r = g.lon * 0.01745329252f;
    const float cp = std::cos(lat_r);
    const float sp = std::sin(lat_r);
    const float cy = std::cos(lon_r);
    const float sy = std::sin(lon_r);
    const Vec3 eye{
        g.center.x + g.dist * cp * sy,
        g.center.y + g.dist * sp,
        g.center.z + g.dist * cp * cy,
    };

    // simple look-at / perspective via sgl
    sgl_defaults();
    sgl_matrix_mode_projection();
    sgl_perspective(sgl_rad(50.f), aspect, g.radius * 0.01f, g.radius * 100.f);
    sgl_matrix_mode_modelview();
    sgl_lookat(eye.x, eye.y, eye.z, g.center.x, g.center.y, g.center.z, 0.f, 1.f, 0.f);

    const Vec3 light = vec3_normalize(Vec3{0.35f, 0.85f, 0.4f});

    sgl_begin_triangles();
    for (const auto &v : g.tris) {
        float ndl = v.nrm.x * light.x + v.nrm.y * light.y + v.nrm.z * light.z;
        ndl = std::max(0.15f, ndl);
        sgl_c3f(v.r * ndl, v.g * ndl, v.b * ndl);
        sgl_v3f(v.pos.x, v.pos.y, v.pos.z);
    }
    sgl_end();

    sg_pass pass = {};
    pass.action = g.pass_action;
    pass.swapchain = sglue_swapchain();
    sg_begin_pass(&pass);
    sgl_draw();
    sg_end_pass();
    sg_commit();

    (void)g.status; // status available for future debugtext
}

void cleanup(void) {
    sgl_shutdown();
    sg_shutdown();
}

void event(const sapp_event *ev) {
    switch (ev->type) {
    case SAPP_EVENTTYPE_KEY_DOWN:
        if (ev->key_code == SAPP_KEYCODE_ESCAPE) {
            sapp_request_quit();
        }
        break;
    case SAPP_EVENTTYPE_MOUSE_DOWN:
        if (ev->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
            g.dragging = true;
            g.last_x = ev->mouse_x;
            g.last_y = ev->mouse_y;
        }
        break;
    case SAPP_EVENTTYPE_MOUSE_UP:
        if (ev->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
            g.dragging = false;
        }
        break;
    case SAPP_EVENTTYPE_MOUSE_MOVE:
        if (g.dragging) {
            const float dx = ev->mouse_x - g.last_x;
            const float dy = ev->mouse_y - g.last_y;
            g.lon += dx * 0.4f;
            g.lat += dy * 0.4f;
            if (g.lat > 89.f) {
                g.lat = 89.f;
            }
            if (g.lat < -89.f) {
                g.lat = -89.f;
            }
            g.last_x = ev->mouse_x;
            g.last_y = ev->mouse_y;
        }
        break;
    case SAPP_EVENTTYPE_MOUSE_SCROLL:
        g.dist *= (ev->scroll_y > 0.f) ? 0.9f : 1.1f;
        if (g.dist < g.radius * 0.2f) {
            g.dist = g.radius * 0.2f;
        }
        if (g.dist > g.radius * 50.f) {
            g.dist = g.radius * 50.f;
        }
        break;
    default:
        break;
    }
}

} // namespace

extern "C" sapp_desc sokol_main(int argc, char *argv[]) {
    if (argc >= 2) {
        g.path = argv[1];
    }
    sapp_desc desc = {};
    desc.init_cb = init;
    desc.frame_cb = frame;
    desc.cleanup_cb = cleanup;
    desc.event_cb = event;
    desc.width = 1024;
    desc.height = 720;
    desc.sample_count = 4;
    desc.window_title = "slop M3G viewer";
    desc.icon.sokol_default = true;
    desc.logger.func = slog_func;
    return desc;
}
