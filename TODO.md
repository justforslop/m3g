# TODO

## High priority

- ~~Add embedded documentation in the header file (doxygen)~~ → public API in
  `include/m3g.hpp` (`@file`, `@defgroup`, decode/export/I/O, model/scene).
- ~~Fix project naming~~ → project/binary/namespace `m3g` (was `slop`).
- Add BUILD.md for instructing dependencies required to install for build.

## Mid priority

- ~~merge the headers file into single header~~ → `include/m3g.hpp` (+ `m3g.h` C
  stub).
- ~~STB-style `M3G_IMPL` + `#undef M3G_IMPL` for decode~~ (`include/m3g.hpp`,
  `src/decode/decoder.cpp`). Convert/export still classic `.cpp` until
  `M3G_CONVERT_IMPL`.

- add support for compressed image texture

## Low priority

- ~~Make this library into single header library~~ (public API + decode impl in
  `m3g.hpp` via `M3G_IMPL`; convert/export still in `src/`).
- ~~Portable C99 single-header~~ → `include/m3g.h` (`M3G_IMPLEMENTATION`) +
  `vendors/libs/vecmath.h` for TRS/quat math. C++ `m3g.hpp` kept for export.
