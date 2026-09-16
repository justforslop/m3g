/* Optional vecmath.h backend for m3g_math_io (same pattern as deflate_io_miniz). */
#include "m3g.h"

#include "vecmath.h"

#include <math.h>
#include <string.h>

static float vm_vec3_length(float x, float y, float z, void *user) {
    (void)user;
    return vec3_length(vec3(x, y, z));
}

static void vm_mat4_identity(float out[16], void *user) {
    (void)user;
    memset(out, 0, 16 * sizeof(float));
    out[0] = out[5] = out[10] = out[15] = 1.f;
}

static void vm_mat4_mul(float const a[16], float const b[16], float out[16], void *user) {
    int row, col, k;
    float r[16];
    (void)user;
    for (row = 0; row < 4; ++row) {
        for (col = 0; col < 4; ++col) {
            float sum = 0.f;
            for (k = 0; k < 4; ++k)
                sum += a[row * 4 + k] * b[k * 4 + col];
            r[row * 4 + col] = sum;
        }
    }
    memcpy(out, r, sizeof(r));
}

static int vm_mat4_is_identity(float const m[16], float epsilon, void *user) {
    static float const id[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    int i;
    (void)user;
    for (i = 0; i < 16; ++i)
        if (fabsf(m[i] - id[i]) > epsilon)
            return 0;
    return 1;
}

static void vm_mat4_translation(float x, float y, float z, float out[16], void *user) {
    vm_mat4_identity(out, user);
    out[3] = x;
    out[7] = y;
    out[11] = z;
}

static void vm_mat4_scale(float x, float y, float z, float out[16], void *user) {
    (void)user;
    memset(out, 0, 16 * sizeof(float));
    out[0] = x;
    out[5] = y;
    out[10] = z;
    out[15] = 1.f;
}

/*
 * vecmath mat44_rotation_axis is row-vector form; M3G stores column-vector
 * row-major (translation in last column). Transpose the 3x3 rotation block.
 */
static void vm_mat4_rotation_axis(float angle_rad, float ax, float ay, float az, float out[16], void *user) {
    mat44_t R;
    float len = vec3_length(vec3(ax, ay, az));
    (void)user;
    if (len < 1e-6f || angle_rad == 0.f) {
        vm_mat4_identity(out, user);
        return;
    }
    R = mat44_rotation_axis(vec3(ax / len, ay / len, az / len), angle_rad);
    out[0] = R.x.x;
    out[1] = R.y.x;
    out[2] = R.z.x;
    out[3] = 0.f;
    out[4] = R.x.y;
    out[5] = R.y.y;
    out[6] = R.z.y;
    out[7] = 0.f;
    out[8] = R.x.z;
    out[9] = R.y.z;
    out[10] = R.z.z;
    out[11] = 0.f;
    out[12] = 0.f;
    out[13] = 0.f;
    out[14] = 0.f;
    out[15] = 1.f;
}

static void vm_decompose_trs(float const matrix[16], float t[3], float q_xyzw[4], float s[3], void *user) {
    float sx, sy, sz, det;
    mat33_t rot;
    vec4_t q;
    (void)user;
    t[0] = matrix[3];
    t[1] = matrix[7];
    t[2] = matrix[11];
    sx = vec3_length(vec3(matrix[0], matrix[4], matrix[8]));
    sy = vec3_length(vec3(matrix[1], matrix[5], matrix[9]));
    sz = vec3_length(vec3(matrix[2], matrix[6], matrix[10]));
    det = matrix[0] * (matrix[5] * matrix[10] - matrix[6] * matrix[9]) -
          matrix[1] * (matrix[4] * matrix[10] - matrix[6] * matrix[8]) +
          matrix[2] * (matrix[4] * matrix[9] - matrix[5] * matrix[8]);
    if (det < 0.f)
        sx = -sx;
    if (sx == 0.f)
        sx = 1e-8f;
    if (sy == 0.f)
        sy = 1e-8f;
    if (sz == 0.f)
        sz = 1e-8f;
    s[0] = sx;
    s[1] = sy;
    s[2] = sz;
    /* rows of rotation from M3G row-major columns / scale */
    rot = mat33(vec3(matrix[0] / sx, matrix[1] / sy, matrix[2] / sz),
                vec3(matrix[4] / sx, matrix[5] / sy, matrix[6] / sz),
                vec3(matrix[8] / sx, matrix[9] / sy, matrix[10] / sz));
    q = quat_from_mat33(rot);
    q_xyzw[0] = q.x;
    q_xyzw[1] = q.y;
    q_xyzw[2] = q.z;
    q_xyzw[3] = q.w;
}

static void vm_quat_from_axis_angle_deg(float angle_deg, float ax, float ay, float az, float q_xyzw[4], void *user) {
    float rad = angle_deg * (float)VECMATH_PI / 180.f;
    float len = vec3_length(vec3(ax, ay, az));
    vec4_t q;
    (void)user;
    if (len < 1e-8f || fabsf(angle_deg) < 1e-8f) {
        q_xyzw[0] = 0.f;
        q_xyzw[1] = 0.f;
        q_xyzw[2] = 0.f;
        q_xyzw[3] = 1.f;
        return;
    }
    q = quat_rotation_axis(vec3(ax / len, ay / len, az / len), rad);
    q_xyzw[0] = q.x;
    q_xyzw[1] = q.y;
    q_xyzw[2] = q.z;
    q_xyzw[3] = q.w;
}

void m3g_install_vecmath_math_io(void) {
    m3g_math_io io;
    memset(&io, 0, sizeof(io));
    io.vec3_length = vm_vec3_length;
    io.mat4_identity = vm_mat4_identity;
    io.mat4_mul = vm_mat4_mul;
    io.mat4_is_identity = vm_mat4_is_identity;
    io.mat4_translation = vm_mat4_translation;
    io.mat4_scale = vm_mat4_scale;
    io.mat4_rotation_axis = vm_mat4_rotation_axis;
    io.decompose_trs = vm_decompose_trs;
    io.quat_from_axis_angle_deg = vm_quat_from_axis_angle_deg;
    io.user = NULL;
    m3g_set_math_io(&io);
}

#ifdef __cplusplus
namespace {
struct VecmathMathIoAutoInstall {
    VecmathMathIoAutoInstall() { m3g_install_vecmath_math_io(); }
};
static VecmathMathIoAutoInstall g_m3g_vecmath_math_io_auto_install;
} // namespace
#else
/* C: optional constructor attribute (GCC/Clang). No-op on other compilers. */
#if defined(__GNUC__) || defined(__clang__)
__attribute__((constructor)) static void m3g__vecmath_math_io_ctor(void) {
    m3g_install_vecmath_math_io();
}
#endif
#endif
