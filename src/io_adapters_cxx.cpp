/* C++ m3g:: install_* bridges over C I/O adapter callbacks. */
#include "m3g.hpp"
#include "m3g.h"

extern "C" {
uint32_t m3g_miniz_adler32(uint32_t adler, unsigned char const *ptr, size_t buf_len, void *user);
int m3g_miniz_uncompress(unsigned char *dest, size_t *dest_len, unsigned char const *source, size_t source_len,
                         void *user);
void *m3g_miniz_uncompress_to_heap(unsigned char const *source, size_t source_len, size_t *out_len, int zlib_header,
                                   void *user);
void m3g_miniz_free_mem(void *p, void *user);
void m3g_install_miniz_deflate_io(void);

unsigned char *m3g_stb_load_file(char const *filename, int *width, int *height, int *channels_in_file, int req_comp,
                                 void *user);
unsigned char *m3g_stb_load_memory(unsigned char const *buffer, int len, int *width, int *height, int *channels_in_file,
                                   int req_comp, void *user);
void m3g_stb_free_pixels(void *pixels, void *user);
void m3g_install_stb_image_io(void);

void *m3g_cjson_create_object(void *user);
void *m3g_cjson_create_array(void *user);
void *m3g_cjson_create_string(char const *s, void *user);
void *m3g_cjson_create_number(double v, void *user);
void *m3g_cjson_create_bool(int v, void *user);
void m3g_cjson_add_item_to_object(void *object, char const *key, void *item, void *user);
void m3g_cjson_add_item_to_array(void *array, void *item, void *user);
int m3g_cjson_get_array_size(void const *array, void *user);
void m3g_cjson_delete_node(void *node, void *user);
char *m3g_cjson_print_unformatted(void *node, void *user);
char *m3g_cjson_print_formatted(void *node, void *user);
void m3g_cjson_free_print(char *printed, void *user);

int m3g_cgltf_write_file(char const *path, void const *data, int kind, void *user);
int m3g_cgltf_parse_file(char const *path, void **out_data, void *user);
int m3g_cgltf_validate(void *data, void *user);
void m3g_cgltf_free_data(void *data, void *user);
}

namespace m3g {

void install_miniz_deflate_io(void) {
    m3g_install_miniz_deflate_io();
    DeflateIo io{};
    io.adler32 = m3g_miniz_adler32;
    io.uncompress = m3g_miniz_uncompress;
    io.uncompress_to_heap = m3g_miniz_uncompress_to_heap;
    io.free_mem = m3g_miniz_free_mem;
    io.user = nullptr;
    set_deflate_io(&io);
}

void install_stb_image_io(void) {
    m3g_install_stb_image_io();
    ImageIo io{};
    io.load_file = m3g_stb_load_file;
    io.load_memory = m3g_stb_load_memory;
    io.free_pixels = m3g_stb_free_pixels;
    io.user = nullptr;
    set_image_io(&io);
}

void install_cjson_json_io(void) {
    JsonIo io{};
    io.create_object = m3g_cjson_create_object;
    io.create_array = m3g_cjson_create_array;
    io.create_string = m3g_cjson_create_string;
    io.create_number = m3g_cjson_create_number;
    io.create_bool = m3g_cjson_create_bool;
    io.add_item_to_object = m3g_cjson_add_item_to_object;
    io.add_item_to_array = m3g_cjson_add_item_to_array;
    io.get_array_size = m3g_cjson_get_array_size;
    io.delete_node = m3g_cjson_delete_node;
    io.print_unformatted = m3g_cjson_print_unformatted;
    io.print_formatted = m3g_cjson_print_formatted;
    io.free_print = m3g_cjson_free_print;
    io.user = nullptr;
    set_json_io(&io);
}

void install_cgltf_gltf_io(void) {
    GltfIo io{};
    io.write_file = m3g_cgltf_write_file;
    io.parse_file = m3g_cgltf_parse_file;
    io.validate = m3g_cgltf_validate;
    io.free_data = m3g_cgltf_free_data;
    io.user = nullptr;
    set_gltf_io(&io);
}

} // namespace m3g
