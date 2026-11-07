/* test_c_handmade_file.c */
#define M3G_IMPLEMENTATION
#include "m3g.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Little-endian writers matching the reader in m3g.h. */
static void wr_u8 (unsigned char **p, unsigned v) { *(*p)++ = (unsigned char)v; }
static void wr_u16(unsigned char **p, unsigned v) {
    *(*p)++ = (unsigned char)(v & 0xFF);
    *(*p)++ = (unsigned char)((v >> 8) & 0xFF);
}
static void wr_u32(unsigned char **p, uint32_t v) {
    *(*p)++ = (unsigned char)(v & 0xFF);
    *(*p)++ = (unsigned char)((v >> 8) & 0xFF);
    *(*p)++ = (unsigned char)((v >> 16) & 0xFF);
    *(*p)++ = (unsigned char)((v >> 24) & 0xFF);
}
static void wr_f32(unsigned char **p, float f) {
    uint32_t bits; memcpy(&bits, &f, 4); wr_u32(p, bits);
}
static void wr_cstr(unsigned char **p, const char *s) {
    size_t n = strlen(s) + 1;
    memcpy(*p, s, n);
    *p += n;
}

/* Append one object (type, length, payload) into a section buffer. */
typedef struct { unsigned char *data; size_t len, cap; } Buf;
static void buf_init(Buf *b) { b->data = NULL; b->len = b->cap = 0; }
static void buf_append(Buf *b, const void *p, size_t n) {
    if (b->len + n > b->cap) {
        size_t nc = b->cap ? b->cap * 2 : 256;
        while (nc < b->len + n) nc *= 2;
        b->data = (unsigned char *)realloc(b->data, nc);
        b->cap = nc;
    }
    memcpy(b->data + b->len, p, n);
    b->len += n;
}
static void buf_free(Buf *b) { free(b->data); b->data = NULL; b->len = b->cap = 0; }

/* Build a minimal but complete file:
 *   object 1: Header
 *   object 2: World  (0 children, no camera, no background)
 *   object 3: Camera (PERSPECTIVE, 60 deg / 4:3 / 0.1..100)
 */
static Buf build_minimal_m3g(void) {
    /* --- payload buffers for each object --- */
    unsigned char hdr[256]; unsigned char *p = hdr;
    /* header object payload */
    wr_u8 (&p, 1);            /* version_major */
    wr_u8 (&p, 1);            /* version_minor */
    wr_u8 (&p, 0);            /* has_external_references = false */
    wr_u32(&p, 0);            /* total_file_size (decoder doesn't validate) */
    wr_u32(&p, 0);            /* approximate_content_size */
    wr_cstr(&p, "handmade-test");
    size_t hdr_len = (size_t)(p - hdr);

    unsigned char world[256]; p = world;
    /* node meta: transformable + object3d (user_id=0, 0 tracks, 0 params),
     * no component transform, no general transform */
    wr_u32(&p, 0);            /* object3d.user_id */
    wr_u32(&p, 0);            /* animation_track_count */
    wr_u32(&p, 0);            /* user_parameter_count */
    wr_u8 (&p, 0);            /* has_component = false */
    wr_u8 (&p, 0);            /* has_general_transform = false */
    wr_u8 (&p, 1);            /* enable_rendering */
    wr_u8 (&p, 1);            /* enable_picking */
    wr_u8 (&p, 255);          /* alpha_factor */
    wr_u32(&p, 0xFFFFFFFF);   /* scope */
    wr_u8 (&p, 0);            /* has_alignment = false */
    wr_u32(&p, 0);            /* child_count = 0 */
    wr_u32(&p, 0);            /* active_camera_id = 0 */
    wr_u32(&p, 0);            /* background_id = 0 */
    size_t world_len = (size_t)(p - world);

    unsigned char cam[256]; p = cam;
    /* node meta (same skeleton as world) */
    wr_u32(&p, 0);            /* user_id */
    wr_u32(&p, 0);            /* anim tracks */
    wr_u32(&p, 0);            /* user params */
    wr_u8 (&p, 0); wr_u8(&p, 0);       /* no component / general */
    wr_u8 (&p, 1); wr_u8(&p, 1);       /* rendering/picking */
    wr_u8 (&p, 255);                   /* alpha */
    wr_u32(&p, 0xFFFFFFFF);            /* scope */
    wr_u8 (&p, 0);                     /* no alignment */
    /* camera body */
    wr_u8 (&p, 50);                    /* PERSPECTIVE */
    wr_f32(&p, 60.f);                  /* fov degrees */
    wr_f32(&p, 4.f / 3.f);             /* aspect */
    wr_f32(&p, 0.1f);                  /* near */
    wr_f32(&p, 100.f);                 /* far */
    size_t cam_len = (size_t)(p - cam);

    /* --- section payload: concatenated objects --- */
    Buf sec; buf_init(&sec);
    {
        unsigned char head[5];
        unsigned char *q = head;
        wr_u8 (&q, M3G_OBJ_HEADER); wr_u32(&q, (uint32_t)hdr_len);
        buf_append(&sec, head, 5); buf_append(&sec, hdr, hdr_len);
    }
    {
        unsigned char head[5]; unsigned char *q = head;
        wr_u8 (&q, M3G_OBJ_WORLD); wr_u32(&q, (uint32_t)world_len);
        buf_append(&sec, head, 5); buf_append(&sec, world, world_len);
    }
    {
        unsigned char head[5]; unsigned char *q = head;
        wr_u8 (&q, M3G_OBJ_CAMERA); wr_u32(&q, (uint32_t)cam_len);
        buf_append(&sec, head, 5); buf_append(&sec, cam, cam_len);
    }

    /* --- file: ident + one uncompressed section --- */
    Buf file; buf_init(&file);
    {
        static const unsigned char ident[12] = {
            0xAB,'J','S','R','I','1','8','4',0xBB,0x0D,0x0A,0x1A
        };
        buf_append(&file, ident, 12);
    }
    {
        unsigned char schdr[9];
        unsigned char *q = schdr;
        wr_u8 (&q, 0);                                  /* scheme = none */
        wr_u32(&q, (uint32_t)(sec.len + 13));           /* total_section_length */
        wr_u32(&q, (uint32_t)sec.len);                  /* uncompressed_length */
        buf_append(&file, schdr, 9);
        buf_append(&file, sec.data, sec.len);
    }
    {
        /* checksum: adler32 over [scheme u8][total u32][uncomp u32][payload] */
        size_t cin_len = 9 + sec.len;
        unsigned char *cin = (unsigned char *)malloc(cin_len);
        unsigned char *q = cin;
        wr_u8 (&q, 0);
        wr_u32(&q, (uint32_t)(sec.len + 13));
        wr_u32(&q, (uint32_t)sec.len);
        memcpy(q, sec.data, sec.len);
        uint32_t sum = m3g_deflate_adler32((uint32_t)M3G_ADLER32_INIT, cin, cin_len);
        free(cin);
        unsigned char tail[4];
        q = tail; wr_u32(&q, sum);
        buf_append(&file, tail, 4);
    }

    buf_free(&sec);
    return file;
}

int main(void) {
    m3g_decoded d;
    Buf f;
    int i;

    m3g_install_miniz_deflate_io();   /* needed for the section checksum */

    f = build_minimal_m3g();
    assert(f.len > 12);

    if (m3g_decode_bytes(f.data, f.len, "handmade.m3g", NULL, &d) != M3G_OK) {
        fprintf(stderr, "decode failed: %s\n", m3g_last_error());
        buf_free(&f);
        return 1;
    }

    /* --- object count: header + world + camera = 3 ------------------------ */
    assert(d.file.object_count == 3);
    assert(d.file.section_count == 1);
    assert(d.file.sections[0].compression_scheme == 0);

    /* --- recover each object by type -------------------------------------- */
    {
        m3g_object *h = NULL, *w = NULL, *c = NULL;
        for (i = 1; i <= d.file.object_count; ++i) {
            m3g_object *o = d.file.objects[i];
            if (o->object_type == M3G_OBJ_HEADER) h = o;
            else if (o->object_type == M3G_OBJ_WORLD) w = o;
            else if (o->object_type == M3G_OBJ_CAMERA) c = o;
        }
        assert(h && w && c);

        assert(h->u.header.version_major == 1);
        assert(h->u.header.version_minor == 1);
        assert(strcmp(h->u.header.authoring_field, "handmade-test") == 0);

        assert(w->u.group.child_count == 0);
        assert(w->u.group.active_camera_id == 0);
        assert(w->u.group.background_id == 0);

        assert(c->u.camera.projection_type == 50);
        assert(c->u.camera.has_perspective);
        assert(fabsf(c->u.camera.fov_degrees - 60.f) < 1e-3f);
        assert(fabsf(c->u.camera.near_distance - 0.1f) < 1e-4f);
        assert(fabsf(c->u.camera.far_distance - 100.f) < 1e-3f);
    }

    /* --- scene IR: the World is the single root, camera not attached ------ */
    assert(d.scene.root_count == 1);
    assert(d.scene.root_node_indices[0] >= 0);
    assert(d.scene.root_node_indices[0] < d.scene.node_count);
    /* The camera object exists but is not a child of the world we wrote,
     * so it should not appear as a child of the root node. */
    {
        int root = d.scene.root_node_indices[0];
        assert(d.scene.nodes[root].child_count == 0);
    }

    printf("handmade round-trip OK (%zu bytes, %d objects)\n",
           f.len, d.file.object_count);

    m3g_decoded_free(&d);
    buf_free(&f);
    return 0;
}
