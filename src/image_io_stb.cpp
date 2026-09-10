/* Optional stb_image backend for m3g::ImageIo. */
#include "m3g.hpp"

#include "stb/stb_image.h"

namespace m3g {
namespace {

unsigned char *stb_load_file(char const *filename, int *width, int *height, int *channels_in_file, int req_comp,
                             void * /*user*/) {
    return stbi_load(filename, width, height, channels_in_file, req_comp);
}

unsigned char *stb_load_memory(unsigned char const *buffer, int len, int *width, int *height, int *channels_in_file,
                               int req_comp, void * /*user*/) {
    return stbi_load_from_memory(buffer, len, width, height, channels_in_file, req_comp);
}

void stb_free_pixels(void *pixels, void * /*user*/) { stbi_image_free(pixels); }

} // namespace

void install_stb_image_io(void) {
    ImageIo io{};
    io.load_file = stb_load_file;
    io.load_memory = stb_load_memory;
    io.free_pixels = stb_free_pixels;
    io.user = nullptr;
    set_image_io(&io);
}

namespace {
struct StbImageIoAutoInstall {
    StbImageIoAutoInstall() { install_stb_image_io(); }
};
/* Link this TU to get stb-backed image I/O by default. */
static StbImageIoAutoInstall g_m3g_stb_image_io_auto_install;
} // namespace

} // namespace m3g
