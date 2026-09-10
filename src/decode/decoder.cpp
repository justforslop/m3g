/* Decode implementation TU (stb / sokol style).
 *
 * Exactly one translation unit in the final link must define M3G_IMPL
 * (or M3G_DECODE_IMPL) before including m3g.hpp. This file is that unit for
 * the normal Make/Ninja builds.
 */
#define M3G_IMPL
#include "m3g.hpp"
