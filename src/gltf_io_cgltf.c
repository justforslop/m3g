/* Optional cgltf backend — C callbacks; C++ GltfIo installed via io_adapters_cxx.cpp. */
#include "cgltf/cgltf.h"
#include "cgltf/cgltf_write.h"

#include <stddef.h>
#include <string.h>

#ifndef M3G_GLTF_IO_OK
#define M3G_GLTF_IO_OK 0
#endif
#ifndef M3G_GLTF_FILE_GLB
#define M3G_GLTF_FILE_GLB 1
#endif

int m3g_cgltf_write_file(char const *path, void const *data, int kind, void *user) {
    cgltf_options options;
    cgltf_result rc;
    (void)user;
    memset(&options, 0, sizeof(options));
    options.type = (kind == M3G_GLTF_FILE_GLB) ? cgltf_file_type_glb : cgltf_file_type_gltf;
    rc = cgltf_write_file(&options, path, (cgltf_data const *)data);
    return (rc == cgltf_result_success) ? M3G_GLTF_IO_OK : (int)rc;
}

int m3g_cgltf_parse_file(char const *path, void **out_data, void *user) {
    cgltf_options options;
    cgltf_data *data = NULL;
    cgltf_result rc;
    (void)user;
    if (!out_data) {
        return -1;
    }
    *out_data = NULL;
    memset(&options, 0, sizeof(options));
    rc = cgltf_parse_file(&options, path, &data);
    if (rc != cgltf_result_success) {
        return (int)rc;
    }
    *out_data = data;
    return M3G_GLTF_IO_OK;
}

int m3g_cgltf_validate(void *data, void *user) {
    cgltf_result rc;
    (void)user;
    if (!data) {
        return -1;
    }
    rc = cgltf_validate((cgltf_data *)data);
    return (rc == cgltf_result_success) ? M3G_GLTF_IO_OK : (int)rc;
}

void m3g_cgltf_free_data(void *data, void *user) {
    (void)user;
    if (data) {
        cgltf_free((cgltf_data *)data);
    }
}
