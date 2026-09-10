/* Optional cgltf backend for m3g::GltfIo (write / parse / validate). */
#include "m3g.hpp"

#include "cgltf/cgltf.h"
#include "cgltf/cgltf_write.h"

namespace m3g {
namespace {

int cgltf_io_write_file(char const *path, void const *data, int kind, void * /*user*/) {
    cgltf_options options{};
    options.type = (kind == GLTF_FILE_GLB) ? cgltf_file_type_glb : cgltf_file_type_gltf;
    const cgltf_result rc = cgltf_write_file(&options, path, static_cast<cgltf_data const *>(data));
    return (rc == cgltf_result_success) ? GLTF_IO_OK : static_cast<int>(rc);
}

int cgltf_io_parse_file(char const *path, void **out_data, void * /*user*/) {
    if (!out_data) {
        return -1;
    }
    *out_data = nullptr;
    cgltf_options options{};
    cgltf_data *data = nullptr;
    const cgltf_result rc = cgltf_parse_file(&options, path, &data);
    if (rc != cgltf_result_success) {
        return static_cast<int>(rc);
    }
    *out_data = data;
    return GLTF_IO_OK;
}

int cgltf_io_validate(void *data, void * /*user*/) {
    if (!data) {
        return -1;
    }
    const cgltf_result rc = cgltf_validate(static_cast<cgltf_data *>(data));
    return (rc == cgltf_result_success) ? GLTF_IO_OK : static_cast<int>(rc);
}

void cgltf_io_free_data(void *data, void * /*user*/) {
    if (data) {
        cgltf_free(static_cast<cgltf_data *>(data));
    }
}

} // namespace

void install_cgltf_gltf_io(void) {
    GltfIo io{};
    io.write_file = cgltf_io_write_file;
    io.parse_file = cgltf_io_parse_file;
    io.validate = cgltf_io_validate;
    io.free_data = cgltf_io_free_data;
    io.user = nullptr;
    set_gltf_io(&io);
}

namespace {
struct CgltfGltfIoAutoInstall {
    CgltfGltfIoAutoInstall() { install_cgltf_gltf_io(); }
};
static CgltfGltfIoAutoInstall g_m3g_cgltf_gltf_io_auto_install;
} // namespace

} // namespace m3g
