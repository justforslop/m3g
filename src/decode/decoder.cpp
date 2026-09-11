/* Decode implementation TU (stb / sokol style).
 *
 * Exactly one translation unit in the final link must define M3G_IMPL
 * (or M3G_DECODE_IMPL) before including m3g.hpp. This file is that unit for
 * the normal Make/Ninja/Zig/CMake builds.
 *
 * Optional default vendor IO adapters: compile with -DM3G_HAS_*_BACKEND
 * (CMake M3G_BUNDLE_* / build.zig). Static archives drop constructor-only
 * objects, so registration is touched from this always-linked TU.
 */
#define M3G_IMPL
#include "m3g.hpp"

namespace m3g {
namespace {

struct DefaultBackendTouch {
    DefaultBackendTouch() {
#if defined(M3G_HAS_MINIZ_BACKEND)
        install_miniz_deflate_io();
#endif
#if defined(M3G_HAS_STB_BACKEND)
        install_stb_image_io();
#endif
#if defined(M3G_HAS_CJSON_BACKEND)
        install_cjson_json_io();
#endif
#if defined(M3G_HAS_CGLTF_BACKEND)
        install_cgltf_gltf_io();
#endif
    }
};

#if defined(M3G_HAS_MINIZ_BACKEND) || defined(M3G_HAS_STB_BACKEND) || \
    defined(M3G_HAS_CJSON_BACKEND) || defined(M3G_HAS_CGLTF_BACKEND)
static DefaultBackendTouch g_m3g_default_backend_touch;
#endif

} // namespace
} // namespace m3g
