/* Single TU for header-only C vendor implementations used by the app. */
/* cgltf_write.h includes cgltf.h; define IMPLEMENTATION only around the first
 * inclusion so the out-of-guard impl body is not compiled twice. */
#define CGLTF_IMPLEMENTATION
#include "cgltf/cgltf.h"
#undef CGLTF_IMPLEMENTATION

#define CGLTF_WRITE_IMPLEMENTATION
#include "cgltf/cgltf_write.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"
