/* test_c_math.c */
#define M3G_IMPLEMENTATION
#include "m3g.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define PI 3.14159265358979323846f

static int feq(float a, float b, float eps) { return fabsf(a - b) <= eps; }

static int mat_eq(const float *a, const float *b, float eps) {
    int i;
    for (i = 0; i < 16; ++i)
        if (!feq(a[i], b[i], eps)) return 0;
    return 1;
}

/* A wrapper backend that adds 1 to a global counter per call and delegates
 * to the portable implementation. */
static int backend_calls;
static void counting_identity(float out[16], void *user) {
    (void)user; backend_calls++;
    m3g_install_portable_math_io();
    m3g_mat4_identity(out);
}

int main(void) {
    float m[16], a[16], b[16], out[16];
    float t[3], q[4], s[3];

    m3g_install_portable_math_io();

    /* ---- 1. identity ------------------------------------------------------ */
    m3g_mat4_identity(m);
    {
        static const float I[16] = {
            1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1
        };
        assert(mat_eq(m, I, 0.f));
        assert(m3g_mat4_is_identity(m, 1e-6f) == 1);
    }
    m[0] = 2.f;
    assert(m3g_mat4_is_identity(m, 1e-6f) == 0);

    /* ---- 2. translation & scale (row-major, translation in col 3) -------- */
    m3g_mat4_translation(1.f, 2.f, 3.f, m);
    assert(feq(m[3],  1.f, 1e-6f));
    assert(feq(m[7],  2.f, 1e-6f));
    assert(feq(m[11], 3.f, 1e-6f));
    assert(feq(m[0], 1.f, 1e-6f) && feq(m[15], 1.f, 1e-6f));

    m3g_mat4_scale(2.f, 3.f, 4.f, m);
    assert(feq(m[0], 2.f, 1e-6f));
    assert(feq(m[5], 3.f, 1e-6f));
    assert(feq(m[10], 4.f, 1e-6f));

    /* ---- 3. matrix multiply: T * S == scale-then-translate ---------------- */
    m3g_mat4_translation(10.f, 20.f, 30.f, a);
    m3g_mat4_scale(2.f, 2.f, 2.f, b);
    m3g_mat4_mul(a, b, out);
    /* out * (1,0,0,1)^T should be (12, 20, 30, 1) */
    assert(feq(out[0] * 1.f + out[3],  12.f, 1e-4f));
    assert(feq(out[4] * 1.f + out[7],  20.f, 1e-4f));
    assert(feq(out[8] * 1.f + out[11], 30.f, 1e-4f));

    /* ---- 4. rotation about Z by 90 deg ------------------------------------ */
    m3g_mat4_rotation_axis(PI * 0.5f, 0.f, 0.f, 1.f, m);
    /* R * (1,0,0,1) -> (0,1,0,1) */
    assert(feq(m[0], 0.f, 1e-5f));
    assert(feq(m[4], 1.f, 1e-5f));
    /* orthonormal: upper-left 3x3 has unit columns */
    assert(feq(m3g_vec3_length(m[0], m[4], m[8]),  1.f, 1e-5f));
    assert(feq(m3g_vec3_length(m[1], m[5], m[9]),  1.f, 1e-5f));
    assert(feq(m3g_vec3_length(m[2], m[6], m[10]), 1.f, 1e-5f));

    /* rotation about a zero axis is identity (documented fallback) */
    m3g_mat4_rotation_axis(PI, 0.f, 0.f, 0.f, m);
    assert(m3g_mat4_is_identity(m, 1e-6f) == 1);

    /* ---- 5. decompose TRS round-trip ------------------------------------- */
    /* Compose T * Rz(90) * S(2,3,4) and decompose. */
    {
        float T[16], R[16], S[16], RS[16];
        m3g_mat4_translation(5.f, -6.f, 7.f, T);
        m3g_mat4_rotation_axis(PI * 0.5f, 0.f, 0.f, 1.f, R);
        m3g_mat4_scale(2.f, 3.f, 4.f, S);
        m3g_mat4_mul(R, S, RS);
        m3g_mat4_mul(T, RS, m);

        m3g_decompose_trs(m, t, q, s);
        assert(feq(t[0],  5.f, 1e-5f));
        assert(feq(t[1], -6.f, 1e-5f));
        assert(feq(t[2],  7.f, 1e-5f));
        assert(feq(s[0],  2.f, 1e-5f));
        assert(feq(s[1],  3.f, 1e-5f));
        assert(feq(s[2],  4.f, 1e-5f));
        /* quaternion is unit length */
        assert(feq(sqrtf(q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3]),
                   1.f, 1e-5f));
    }

    /* ---- 6. quaternion from axis-angle degrees --------------------------- */
    m3g_quat_from_axis_angle_deg(90.f, 0.f, 0.f, 1.f, q);
    /* 90 deg about Z: w = cos(45) = sqrt(2)/2, z = sin(45) = sqrt(2)/2 */
    {
        float h = (float)sqrt(2.0) * 0.5f;
        assert(feq(q[0], 0.f, 1e-5f));
        assert(feq(q[1], 0.f, 1e-5f));
        assert(feq(q[2], h,   1e-5f));   /* z component (xyzw) */
        assert(feq(q[3], h,   1e-5f));   /* w */
    }
    /* zero axis -> identity quaternion */
    m3g_quat_from_axis_angle_deg(45.f, 0.f, 0.f, 0.f, q);
    assert(feq(q[0],0.f,1e-6f) && feq(q[1],0.f,1e-6f) &&
           feq(q[2],0.f,1e-6f) && feq(q[3],1.f,1e-6f));

    /* ---- 7. m3g_node_matrix: identity / component / general --------------- */
    {
        m3g_node_meta node;
        memset(&node, 0, sizeof(node));

        /* no transform -> identity */
        m3g_node_matrix(&node, m);
        assert(m3g_mat4_is_identity(m, 1e-6f) == 1);

        /* component transform: T(1,2,3) * S(2,2,2) * Rz(0) */
        node.transformable.component.present = 1;
        node.transformable.component.translation[0] = 1.f;
        node.transformable.component.translation[1] = 2.f;
        node.transformable.component.translation[2] = 3.f;
        node.transformable.component.scale[0] = 2.f;
        node.transformable.component.scale[1] = 2.f;
        node.transformable.component.scale[2] = 2.f;
        node.transformable.component.orientation_angle = 0.f;
        node.transformable.component.orientation_axis[2] = 1.f;
        m3g_node_matrix(&node, m);
        assert(feq(m[3],  1.f, 1e-5f));
        assert(feq(m[7],  2.f, 1e-5f));
        assert(feq(m[11], 3.f, 1e-5f));
        assert(feq(m[0],  2.f, 1e-5f));

        /* general transform takes precedence */
        node.transformable.has_general_transform = 1;
        {
            int i;
            for (i = 0; i < 16; ++i) node.transformable.general_transform[i] = 0.f;
            node.transformable.general_transform[0]  = 9.f;
            node.transformable.general_transform[5]  = 9.f;
            node.transformable.general_transform[10] = 9.f;
            node.transformable.general_transform[15] = 1.f;
        }
        m3g_node_matrix(&node, m);
        assert(feq(m[0], 9.f, 1e-6f));
        assert(feq(m[3], 0.f, 1e-6f));   /* general wins: no translation */
    }

    /* ---- 8. color conversion --------------------------------------------- */
    {
        m3g_rgb rgb = { 255, 128, 0 };
        m3g_rgba rgba = { 0, 64, 255, 128 };
        float cf[4];

        m3g_rgb_to_float(&rgb, cf);
        assert(feq(cf[0], 1.f, 1e-6f));
        assert(feq(cf[1], 128.f / 255.f, 1e-6f));
        assert(feq(cf[2], 0.f, 1e-6f));

        m3g_rgba_to_float(&rgba, cf);
        assert(feq(cf[0], 0.f, 1e-6f));
        assert(feq(cf[1], 64.f / 255.f, 1e-6f));
        assert(feq(cf[2], 1.f, 1e-6f));
        assert(feq(cf[3], 128.f / 255.f, 1e-6f));
    }

    /* ---- 9. custom backend is actually consulted ------------------------- */
    {
        m3g_math_io io;
        float x[16];

        backend_calls = 0;
        memset(&io, 0, sizeof(io));
        io.mat4_identity = counting_identity;
        io.user = NULL;
        m3g_set_math_io(&io);
        assert(m3g_get_math_io() == &io);

        /* We must supply a complete table for the delegate to work; the
         * public m3g_mat4_identity should hit our counter. */
        m3g_mat4_identity(x);
        assert(backend_calls >= 1);

        /* Re-install portable; counter must stop moving. */
        m3g_install_portable_math_io();
        backend_calls = 0;
        m3g_mat4_identity(x);
        assert(backend_calls == 0);
        assert(m3g_mat4_is_identity(x, 1e-6f) == 1);
    }

    printf("test_c_math: OK\n");
    return 0;
}
