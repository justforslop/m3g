/* Optional stb_image backend for m3g_image_io (C). */
#include "m3g.h"

#include "stb/stb_image.h"

unsigned char *m3g_stb_load_file(char const *filename, int *width, int *height, int *channels_in_file, int req_comp,
                                 void *user) {
    (void)user;
    return stbi_load(filename, width, height, channels_in_file, req_comp);
}

unsigned char *m3g_stb_load_memory(unsigned char const *buffer, int len, int *width, int *height,
                                   int *channels_in_file, int req_comp, void *user) {
    (void)user;
    return stbi_load_from_memory(buffer, len, width, height, channels_in_file, req_comp);
}

void m3g_stb_free_pixels(void *pixels, void *user) {
    (void)user;
    stbi_image_free(pixels);
}

void m3g_install_stb_image_io(void) {
    m3g_image_io io;
    io.load_file = m3g_stb_load_file;
    io.load_memory = m3g_stb_load_memory;
    io.free_pixels = m3g_stb_free_pixels;
    io.user = NULL;
    m3g_set_image_io(&io);
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((constructor)) static void m3g__stb_image_ctor(void) {
    m3g_install_stb_image_io();
}
#endif
