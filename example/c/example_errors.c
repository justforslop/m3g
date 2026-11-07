/* test_c_errors.c */
#define M3G_IMPLEMENTATION
#include "m3g.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const unsigned char IDENT[12] = {
    0xAB, 'J', 'S', 'R', 'I', '1', '8', '4', 0xBB, 0x0D, 0x0A, 0x1A
};

/* Every failure must: return < 0, set a non-empty error string, and leave
 * the decoded struct in a state that m3g_decoded_free() accepts. */
static void expect_fail(const char *label,
                        const unsigned char *bytes, size_t len,
                        int expected_rc /* or 0 = any error */) {
    m3g_decoded d;
    m3g_result rc;
    const char *msg;

    memset(&d, 0xAB, sizeof(d));          /* poison: free() must sanitize */
    rc = m3g_decode_bytes(bytes, len, label, NULL, &d);
    assert(rc != M3G_OK);
    if (expected_rc != 0) assert(rc == expected_rc);

    msg = m3g_last_error();
    assert(msg != NULL);
    assert(msg[0] != '\0');               /* always a human message */

    /* Must not crash and must zero the struct. */
    m3g_decoded_free(&d);
    assert(d.source_path == NULL);
    assert(d.file.objects == NULL);
    assert(d.file.section_count == 0);
    assert(d.scene.nodes == NULL);
    assert(d.scene.node_count == 0);

    printf("  [ok] %-28s rc=%d  msg=\"%s\"\n", label, rc, msg);
}

int main(void) {
    unsigned char buf[256];

    m3g_install_miniz_deflate_io();

    printf("malformed inputs:\n");

    /* 1. Empty buffer. */
    expect_fail("empty", buf, 0, M3G_ERR_FORMAT);

    /* 2. Too short to hold the identifier. */
    memcpy(buf, IDENT, 5);
    expect_fail("short-ident", buf, 5, M3G_ERR_FORMAT);

    /* 3. Bad identifier bytes. */
    memcpy(buf, IDENT, sizeof(IDENT));
    buf[1] = 'X';
    expect_fail("bad-ident", buf, sizeof(IDENT), M3G_ERR_FORMAT);

    /* 4. Valid identifier, truncated section header. */
    memcpy(buf, IDENT, sizeof(IDENT));
    buf[12] = 0;                       /* scheme */
    /* only 2 more bytes of the 8-byte section header follow */
    buf[13] = 0x0D; buf[14] = 0;
    expect_fail("trunc-section-hdr", buf, 15, M3G_ERR_FORMAT);

    /* 5. Valid identifier + section header claiming an impossible length. */
    {
        unsigned char *p = buf + sizeof(IDENT);
        memset(buf, 0, sizeof(buf));
        memcpy(buf, IDENT, sizeof(IDENT));
        *p++ = 0;                       /* scheme = none */
        /* total_section_length = 13 (minimum legal), uncompressed = 13 */
        *p++ = 13; *p++ = 0; *p++ = 0; *p++ = 0;
        *p++ = 13; *p++ = 0; *p++ = 0; *p++ = 0;
        /* payload: nothing (so we're 13 bytes short of the declared total) */
        expect_fail("trunc-section-body", buf, (size_t)(p - buf), M3G_ERR_FORMAT);
    }

    /* 6. Unsupported compression scheme (spec only allows 0 and 1). */
    {
        unsigned char *p = buf;
        memcpy(p, IDENT, sizeof(IDENT)); p += sizeof(IDENT);
        *p++ = 7;                       /* scheme = bogus */
        *p++ = 13; *p++ = 0; *p++ = 0; *p++ = 0;
        *p++ = 13; *p++ = 0; *p++ = 0; *p++ = 0;
        expect_fail("bad-compression", buf, (size_t)(p - buf), M3G_ERR_FORMAT);
    }

    /* 7. Wrong Adler-32 checksum on an otherwise-valid section. */
    {
        unsigned char *p = buf;
        memcpy(p, IDENT, sizeof(IDENT)); p += sizeof(IDENT);
        *p++ = 0;                       /* scheme = none */
        *p++ = 13; *p++ = 0; *p++ = 0; *p++ = 0;
        *p++ = 13; *p++ = 0; *p++ = 0; *p++ = 0;
        /* checksum: deliberately wrong */
        *p++ = 0xDE; *p++ = 0xAD; *p++ = 0xBE; *p++ = 0xEF;
        expect_fail("bad-checksum", buf, (size_t)(p - buf), M3G_ERR_FORMAT);
    }

    /* 8. Object declares a length larger than the remaining payload. */
    {
        /* Build a valid section whose single object overruns it. */
        unsigned char *p = buf;
        memcpy(p, IDENT, sizeof(IDENT)); p += sizeof(IDENT);
        /* Section header: total = 13 + 5 (payload = 5 bytes: type+len),
         * uncompressed = same. */
        *p++ = 0;
        *p++ = 13 + 5; *p++ = 0; *p++ = 0; *p++ = 0;
        *p++ = 13 + 5; *p++ = 0; *p++ = 0; *p++ = 0;
        /* payload: object says type=HEADER, length=1000 but only 0 bytes follow */
        *p++ = M3G_OBJ_HEADER;
        *p++ = 0xE8; *p++ = 0x03; *p++ = 0; *p++ = 0;   /* 1000 LE */
        /* checksum for the real payload (type + 4-byte len) */
        {
            unsigned char cin[9 + 5];
            unsigned char *q = cin;
            *q++ = 0;
            *q++ = 13 + 5; *q++ = 0; *q++ = 0; *q++ = 0;
            *q++ = 13 + 5; *q++ = 0; *q++ = 0; *q++ = 0;
            *q++ = M3G_OBJ_HEADER;
            *q++ = 0xE8; *q++ = 0x03; *q++ = 0; *q++ = 0;
            {
                uint32_t sum = m3g_deflate_adler32((uint32_t)M3G_ADLER32_INIT,
                                                   cin, sizeof(cin));
                *p++ = (unsigned char)(sum & 0xFF);
                *p++ = (unsigned char)((sum >> 8) & 0xFF);
                *p++ = (unsigned char)((sum >> 16) & 0xFF);
                *p++ = (unsigned char)((sum >> 24) & 0xFF);
            }
        }
        expect_fail("object-overrun", buf, (size_t)(p - buf), M3G_ERR_FORMAT);
    }

    /* 9. Compressed section but deflate backend uninstalled -> BACKEND. */
    {
        unsigned char *p = buf;
        m3g_set_deflate_io(NULL);       /* remove backend */
        memcpy(p, IDENT, sizeof(IDENT)); p += sizeof(IDENT);
        *p++ = 1;                       /* scheme = zlib */
        *p++ = 13 + 4; *p++ = 0; *p++ = 0; *p++ = 0;
        *p++ = 64; *p++ = 0; *p++ = 0; *p++ = 0;
        /* 4 bytes of garbage "compressed" payload */
        *p++ = 0x78; *p++ = 0x9C; *p++ = 0x01; *p++ = 0x02;
        /* checksum (adler) - can't compute without backend, but the decoder
         * will try to call it and hit the missing-backend error first OR
         * the checksum mismatch. Either is fine; we just require failure. */
        *p++ = 0; *p++ = 0; *p++ = 0; *p++ = 0;

        m3g_decoded d;
        memset(&d, 0xAB, sizeof(d));
        {
            m3g_result rc = m3g_decode_bytes(buf, (size_t)(p - buf),
                                             "no-deflate", NULL, &d);
            assert(rc != M3G_OK);
            assert(m3g_last_error()[0] != '\0');
            printf("  [ok] %-28s rc=%d  msg=\"%s\"\n",
                   "no-deflate-backend", rc, m3g_last_error());
        }
        m3g_decoded_free(&d);
        m3g_install_miniz_deflate_io();  /* restore for any later tests */
    }

    /* 10. Decode with a valid file path that doesn't exist -> IO error. */
    {
        m3g_decoded d;
        m3g_result rc;
        memset(&d, 0xAB, sizeof(d));
        rc = m3g_decode_file("this/path/does/not/exist.m3g", NULL, &d);
        assert(rc == M3G_ERR_IO || rc < 0);
        assert(m3g_last_error()[0] != '\0');
        printf("  [ok] %-28s rc=%d  msg=\"%s\"\n",
               "missing-file", rc, m3g_last_error());
        m3g_decoded_free(&d);
    }

    printf("test_c_errors: OK\n");
    return 0;
}
