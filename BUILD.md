# Build

`m3g` is built with **C++17** for the **viewer** (`debug`) and **CLI**
(`release`) builds. Use either **Make** (`Makefile`, Linux/POSIX) or **Zig**
(`build.zig` / `build.zig.zon`, **Linux and Windows**, plus cross targets) —
same logical outputs. Zig also exposes an installable **package library**
(`artifact "m3g"`).\
**Make or Zig** is enough to compile, test, or convert M3G files. **CMake** is
optional for packaging (`find_package`), `ctest`, and LSP `compile_commands`.\
Most third-party code lives under `vendors/` and is fetched with the `setup`
targets (Make or Zig).

```bash
# Make (Linux / POSIX) — apps + installable package
make debug      # Sokol + Dear ImGui viewer → build/debug  (default `make`)
make release    # CLI converter             → build/m3g
make lib        # static package            → build/lib/libm3g.a (+ headers, m3g.pc)
make install PREFIX=/usr/local   # lib + headers + pkg-config (+ CLI unless INSTALL_CLI=0)
make test       # unit tests under tests/bin/

# Zig — portable driver + package (build.zig / build.zig.zon)
zig build -p build           # viewer → build/debug[.exe]
zig build -p build release   # CLI    → build/m3g[.exe]
zig build -p build lib       # static libm3g + headers → build/lib, build/include
zig build test               # unit tests under tests/bin/
```

## Requirements

**Host:**

| Host              | Driver          | Notes                                                                                             |
| ----------------- | --------------- | ------------------------------------------------------------------------------------------------- |
| **Linux** (POSIX) | Make and/or Zig | `_DEFAULT_SOURCE` set; viewer needs X11 + GL                                                      |
| **Windows**       | **Zig**         | No separate Ninja/MinGW required; `zig cc`/`c++` compile C/C++. Viewer links `opengl32` + `gdi32` |
| **Cross**         | Zig `-Dtarget=` | e.g. `x86_64-windows-gnu`, `aarch64-linux-gnu`                                                    |

MSVC `cl.exe` is not the default path. Use **CMake** if you need MSVC. **Make**
remains Linux/POSIX-oriented.

**Toolchain (required for build / release / tests):**

| Tool          | Role                                                                 |
| ------------- | -------------------------------------------------------------------- |
| `g++` / `c++` | Make: C++17 compiler (CLI + viewer; `debug.cpp` uses `m3g.hpp`)    |
| `gcc`         | Make: C99 compiler (app C sources, cJSON, tests)                     |
| `make`        | Build driver (`Makefile`) — Linux/POSIX — **or**                     |
| `zig`         | Build driver (`build.zig`) — **Linux, Windows, cross** (ships clang) |
| Python 3      | Zig `setup` / `test` / `doc` (`scripts/*.py`; auto-detect on host)   |
| `7z`          | Unpack miniz / imgui zip archives (`setup-miniz`, `setup-imgui`)     |

You need **either** `make` **or** `zig` on Linux (or both). On Windows use
**zig**. Make still uses `curl` for `setup`; Zig `setup` uses Python (`urllib`)
plus `7z` for zips.

**Not required** for debug / release / test builds:

| Tool   | Role if present                                                       |
| ------ | --------------------------------------------------------------------- |
| CMake  | Optional: installable package, `ctest`, `compile_commands.json`, MSVC |
| `curl` | Make `setup` only (Zig setup uses Python)                             |
| Ninja  | Removed; use `build.zig` instead                                      |

**System libraries** (headers + link libs):

| Library                               | Platform               | Used for                  |
| ------------------------------------- | ---------------------- | ------------------------- |
| libm (`-lm`)                          | Linux (and some MinGW) | Math                      |
| OpenGL (`libGL`)                      | Linux                  | Viewer (Sokol GLCORE)     |
| X11 (`libX11`, `libXi`, `libXcursor`) | Linux                  | Viewer window / input     |
| libdl, libpthread                     | Linux                  | Viewer                    |
| `opengl32`, `gdi32`                   | Windows                | Viewer (Sokol Win32 + GL) |

**CLI** (`build/m3g` / `build/m3g.exe`) links **libm** where needed (deflate via
vendored **miniz**).\
**Viewer** (`build/debug` / `build/debug.exe`) additionally links platform
GL/window libs.

Dear ImGui, Sokol, cJSON, cgltf, stb, and test helpers are **vendored** (not
system packages).

### Windows (Zig)

1. Install **Zig**, **Python 3**, and **7-Zip** (`7z.exe` on `PATH` or under
   `C:\Program Files\7-Zip\`).
2. Build (no file edits for platform flags — `build.zig` selects libs from the
   target). Host Python is auto-detected (`python` / `python3` / `py -3`);
   override with `-Dpy=python` or `-Dpy=py` if spawn fails (“subcommand
   failed”).

```bat
zig build setup
zig build -p build release
zig build -p build
build\m3g.exe assets\90.m3g out.glb --overwrite
build\debug.exe assets\90.m3g
```

Cross-compile a Windows binary from Linux:

```bash
zig build -p build/win release -Dtarget=x86_64-windows-gnu
zig build -p build/win         -Dtarget=x86_64-windows-gnu
```

Tests: `zig build test` → `scripts/run_tests.py` via `zig cc` / `zig c++`.
Filters: `zig build test -Dn=1` or `zig build test -Ds=2`.

On Arch Linux:

### What is usually already installed

In the table below, `→` means **requires** or **installed as a dependency of**.

| Need                                      | Often already on Arch via     |
| ----------------------------------------- | ----------------------------- |
| **glibc**                                 | `pacman` → `glibc`            |
| `g++`, `gcc`, `make`                      | `base-devel`                  |
| **OpenGL** / Mesa (`libGL`) (viewer)      | `mesa` or a GPU driver stack  |
| **X11** (`libX11`, `libXi`, `libXcursor`) | `libx11` `libxi` `libxcursor` |
| **curl** (Make `setup`)                   | `pacman` → `curl`             |

### Minimal install

`--needed` skips packages you already have.

```bash
sudo pacman -S --needed base-devel zig curl python mesa libx11 libxi libxcursor 7zip
```

**Vendored sources** — fetch once before the first build (**`7z`** required for
miniz and imgui zips):

```bash
make setup    # or: zig build setup
```

This downloads cgltf, cJSON, stb image headers, Sokol (including sokol-imgui),
mattias `testfw.h`, miniz, and Dear ImGui (core only) into `vendors/`.

## Build

Default builds the **viewer** (debug). The CLI is the **release** target. Make
uses host **g++/gcc**. Zig uses its bundled clang (`zig c++` / `zig cc`) and
selects Windows vs Linux link libraries from `-Dtarget` / host.

```bash
# Make
make                # viewer → build/debug
make debug          # same
make view           # alias for make debug
make release        # CLI    → build/m3g
make clean          # rm -rf build tests/bin

# Zig (default install prefix: zig-out/; use -p build to match Make paths)
zig build -p build              # viewer → build/debug[.exe]
zig build -p build debug        # same
zig build -p build view         # alias for debug
zig build -p build release      # CLI    → build/m3g[.exe]
rm -rf build tests/bin .zig-cache zig-out   # full wipe
```

| Target (Make / Zig) | Binary                            | Flags                  | Links                                                                          |
| ------------------- | --------------------------------- | ---------------------- | ------------------------------------------------------------------------------ |
| `debug` / `view`    | `build/debug` (`.exe` on Windows) | `-O0 -g` (C++17 + C99) | **Linux:** m, GL, X11, Xi, Xcursor, dl, pthread · **Windows:** opengl32, gdi32 |
| `release`           | `build/m3g` (`.exe` on Windows)   | `-Os -g0 -DNDEBUG`     | m (POSIX)                                                                      |

Object files for Make live under `build/obj/<debug\|release>/` so `build/debug`
can be the viewer executable (not a directory). Zig caches objects under
`.zig-cache/`.

Release compiles `src/*.cpp` and `src/*.c` (except `src/debug.cpp`) plus
`vendors/cjson/cJSON.c` and `vendors/miniz/miniz.c`. Debug links the same
library objects (minus `main.o`) with `src/debug.cpp` (`m3g.hpp`) and Dear ImGui.

### Public API header (`include/m3g.hpp`)

Decode is **sokol / stb style**: declarations always; implementation once.

```cpp
// exactly one .cpp in the link:
#define M3G_IMPL
#include <m3g.hpp>
// header enables M3G_DECODE_IMPL, then #undef M3G_IMPL

// every other TU:
#include <m3g.hpp>

m3g::decode::Decoder dec;
auto decoded = dec.decode_file("model.m3g");
```

The tree’s impl unit is `src/decode/decoder.cpp` (defines `M3G_IMPL`). `m3g.hpp`
does **not** include miniz or stb_image.

**Callbacks (backends):**

| Concern                                  | API                                 | Optional adapter in tree                                  |
| ---------------------------------------- | ----------------------------------- | --------------------------------------------------------- |
| zlib/deflate (sections, embedded images) | `m3g::DeflateIo` / `set_deflate_io` | `src/deflate_io_miniz.cpp` → `install_miniz_deflate_io()` |
| raster decode                            | `m3g::ImageIo` / `set_image_io`     | `src/image_io_stb.cpp` → `install_stb_image_io()`         |
| JSON (optional legacy)                   | `m3g::JsonIo` / `set_json_io`       | `src/json_io_cjson.cpp` → `install_cjson_json_io()`       |
| glTF write/parse                         | `m3g::GltfIo` / `set_gltf_io`       | `src/gltf_io_cgltf.cpp` → `install_cgltf_gltf_io()`       |

**glTF export** builds a `cgltf_data` tree, then calls
**`m3g::gltf_write_file`** (callback; default adapter uses `cgltf_write_file`).
External `.bin` / images are written by the app; optional validate uses
`gltf_parse_file` / `gltf_validate`.

Both adapters auto-register via static init when linked. PNG **write** still
uses `stb_image_write` in `src/util/png_writer.cpp` / glTF export for now.

Convert/export remain separate translation units for now.

### Cross-compilation

**Zig** (preferred):

```bash
zig build -p build release -Dtarget=aarch64-linux-gnu
zig build -p build release -Dtarget=x86_64-windows-gnu
# Viewer still needs target OpenGL/X11 (or skip debug on a headless cross)
```

**Make / gcc** triple via `CROSS_COMPILE`:

```bash
make release CROSS_COMPILE=aarch64-linux-gnu- CXX=aarch64-linux-gnu-g++
```

You need **target** headers and libraries for Make cross builds.

### Make package (`make lib` / `make install`)

POSIX Make builds an installable **static library package** (same role as CMake
`m3g::m3g` / Zig `artifact("m3g")`): `libm3g.a`, public headers, and
**pkg-config** `m3g.pc`.

```bash
make setup
make lib                          # → build/lib/libm3g.a, build/include/, build/lib/pkgconfig/m3g.pc
make install PREFIX=/usr/local    # DESTDIR= supported
make uninstall PREFIX=/usr/local
# skip CLI on install:
make install PREFIX=/usr/local INSTALL_CLI=0
```

**Consumer (pkg-config):**

```bash
c++ -std=c++17 main.cpp $(pkg-config --cflags --libs m3g) -o app
```

```makefile
CXXFLAGS += $(shell pkg-config --cflags m3g)
LDLIBS   += $(shell pkg-config --libs m3g)
```

**Bundle toggles** (same idea as CMake `M3G_BUNDLE_*`; default **1** = on):

| Variable           | Default | Meaning                                     |
| ------------------ | ------- | ------------------------------------------- |
| `M3G_WITH_EXPORT`  | 1       | Converter / glTF export objects in the `.a` |
| `M3G_BUNDLE_MINIZ` | 1       | miniz + `DeflateIo` adapter                 |
| `M3G_BUNDLE_STB`   | 1       | stb impl + `ImageIo` adapter                |
| `M3G_BUNDLE_CJSON` | 1       | cJSON + `JsonIo` adapter                    |
| `M3G_BUNDLE_CGLTF` | 1       | cgltf impl + `GltfIo` adapter               |

```bash
# decode-only, no vendored backends (call set_*_io yourself):
make lib M3G_WITH_EXPORT=0 M3G_BUNDLE_MINIZ=0 M3G_BUNDLE_STB=0 \
  M3G_BUNDLE_CJSON=0 M3G_BUNDLE_CGLTF=0
```

Installed layout: `include/m3g.{h,hpp}`, `lib/libm3g.a`, `lib/pkgconfig/m3g.pc`,
optional `bin/m3g`. Version in the `.pc` matches `0.1.0` / `M3G_VERSION_*`.

### Zig package (`build.zig.zon`)

Requires **Zig ≥ 0.16**. Package name: **`m3g`**, version **0.1.0** (see
`build.zig.zon` / `M3G_VERSION_*`).

Fetch (git URL or path) after vendored **library** sources exist in the package
tree (`zig build setup` or the subset below — see **Vendors required for the
package**):

```bash
zig fetch --save git+https://github.com/justforslop/m3g.git
# or path dependency: .m3g = .{ .path = "../m3g" },
```

**Consumer (`build.zig`):**

```zig
const m3g = b.dependency("m3g", .{
    .target = target,
    .optimize = optimize,
});
// Static lib + public headers (m3g.h / m3g.hpp on the include path):
exe_mod.linkLibrary(m3g.artifact("m3g"));
exe_mod.link_libcpp = true; // C++17 API
```

```bash
zig build -p build lib          # install libm3g.a + include/ only
zig build -p prefix lib         # same under another prefix
```

**Do not define `M3G_IMPL` / `M3G_DECODE_IMPL` in your app.** The package
library already compiles the single implementation TU
(`src/decode/decoder.cpp`). In consumer code only:

```cpp
#include <m3g.hpp>   // or <m3g.h>
// use m3g::decode::Decoder, m3g::Converter, …
```

#### Vendors required for the package (artifact `m3g`)

These must be present under `vendors/` **before** `zig build` compiles the
package (path dep: run setup in the m3g tree; published tarball/git checkout
must include them or you run setup after fetch).

| Vendor    | Path                                           | Role                                                 | Setup                                |
| --------- | ---------------------------------------------- | ---------------------------------------------------- | ------------------------------------ |
| **miniz** | `vendors/miniz/` (`miniz.c` / `miniz.h`)       | Default `DeflateIo` (zlib sections, embedded images) | `zig build setup-miniz` (needs `7z`) |
| **stb**   | `vendors/stb/stb_image.h`, `stb_image_write.h` | Default `ImageIo` + PNG write in export              | `zig build setup-stb`                |
| **cJSON** | `vendors/cjson/cJSON.c`, `cJSON.h`             | Default `JsonIo` (optional JSON paths)               | `zig build setup-cjson`              |
| **cgltf** | `vendors/cgltf/cgltf.h`, `cgltf_write.h`       | Default `GltfIo` (glTF write/parse)                  | `zig build setup-cgltf`              |

```bash
# all of the above (+ viewer/test extras you can ignore for the package):
zig build setup
# or only library backends:
zig build setup-miniz setup-stb setup-cjson setup-cgltf
```

They are **compiled into** `artifact("m3g")` and default callbacks are
registered from the decode TU when `M3G_HAS_*_BACKEND` is set
(`install_miniz_deflate_io`, `install_stb_image_io`, `install_cjson_json_io`,
`install_cgltf_gltf_io`). You do **not** ship or `#define` separate vendor impl
macros for normal use.

To **skip** these vendors and wire your own backends, use **CMake** instead of
the Zig package artifact (see **CMake** → `M3G_BUNDLE_*=OFF` below).

**Not required** to link or use the package library:

| Vendor                       | Used for        |
| ---------------------------- | --------------- |
| Dear ImGui (`vendors/imgui`) | Viewer only     |
| Sokol (`vendors/sokol`)      | Viewer only     |
| `vendors/libs/testfw.h`      | Unit tests only |

#### Optional: your own backend impl (replace vendors)

`m3g.hpp` does **not** embed miniz/stb/cJSON/cgltf. If you prefer not to use the
tree adapters, implement the callback structs and install them **before**
decode/export (you can still link `artifact("m3g")` and override the defaults):

| Concern          | Install API                         | Default vendor adapter in this repo  |
| ---------------- | ----------------------------------- | ------------------------------------ |
| zlib/deflate     | `m3g::set_deflate_io` / `DeflateIo` | miniz → `install_miniz_deflate_io()` |
| raster load      | `m3g::set_image_io` / `ImageIo`     | stb_image → `install_stb_image_io()` |
| JSON             | `m3g::set_json_io` / `JsonIo`       | cJSON → `install_cjson_json_io()`    |
| glTF write/parse | `m3g::set_gltf_io` / `GltfIo`       | cgltf → `install_cgltf_gltf_io()`    |

Without either the packaged adapters linked or your own `set_*_io`, decode and
glTF export fail at runtime (“callback not set”).

### CMake (optional package + tools)

CMake builds an installable **library package** `m3g::m3g` (static by default),
optional CLI, tests, and `compile_commands.json`.

Unlike the Zig package artifact (which always ships the default vendor
adapters), **CMake can omit vendored miniz/stb/cJSON/cgltf** so you link only
what you want and install your own `DeflateIo` / `ImageIo` / `JsonIo` / `GltfIo`
(or point at headers/libs you already have).

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build build
cmake --install build
```

| Option              | Default (top-level)                            | Meaning                                                     |
| ------------------- | ---------------------------------------------- | ----------------------------------------------------------- |
| `M3G_BUILD_CLI`     | ON                                             | `m3g` CLI executable                                        |
| `M3G_BUILD_VIEWER`  | OFF                                            | Sokol viewer (`m3g-view`)                                   |
| `M3G_BUILD_TESTS`   | ON                                             | unit tests + `ctest`                                        |
| `M3G_INSTALL`       | ON                                             | install + Config package                                    |
| `M3G_ENABLE_IPO`    | OFF                                            | LTO                                                         |
| `BUILD_SHARED_LIBS` | OFF                                            | shared vs static `libm3g`                                   |
| `M3G_WITH_EXPORT`   | ON                                             | Converter / glTF export TUs (needs cgltf + stb_image_write) |
| `M3G_BUNDLE_MINIZ`  | ON (top-level) / **OFF** if `add_subdirectory` | vendored miniz + `DeflateIo` adapter                        |
| `M3G_BUNDLE_STB`    | same                                           | stb_image(_write) impl + `ImageIo` adapter                  |
| `M3G_BUNDLE_CJSON`  | same                                           | vendored cJSON + `JsonIo` adapter                           |
| `M3G_BUNDLE_CGLTF`  | same                                           | cgltf header impl + `GltfIo` adapter                        |

**External headers / targets** (when the matching `M3G_BUNDLE_*=OFF`):

| Cache variable                               | Purpose                                                               |
| -------------------------------------------- | --------------------------------------------------------------------- |
| `M3G_MINIZ_INCLUDE_DIR` / `M3G_MINIZ_TARGET` | miniz (only if you still compile the miniz adapter yourself)          |
| `M3G_STB_INCLUDE_DIR` / `M3G_STB_TARGET`     | dir with `stb/stb_image*.h` (required for export if not bundling stb) |
| `M3G_CJSON_INCLUDE_DIR` / `M3G_CJSON_TARGET` | cJSON                                                                 |
| `M3G_CGLTF_INCLUDE_DIR` / `M3G_CGLTF_TARGET` | dir with `cgltf/cgltf.h` (required for export if not bundling cgltf)  |

Layout expected by sources: `#include "miniz.h"`, `#include "stb/stb_image.h"`,
`#include "cgltf/cgltf.h"`, `#include "cJSON.h"` (or equivalent via include
path).

**Lean embed (no vendor bloat — decode only, your IO):**

```cmake
add_subdirectory(m3g)  # M3G_BUNDLE_*=OFF by default when not top-level
# or explicitly:
#   -DM3G_WITH_EXPORT=OFF
#   -DM3G_BUNDLE_MINIZ=OFF -DM3G_BUNDLE_STB=OFF
#   -DM3G_BUNDLE_CJSON=OFF -DM3G_BUNDLE_CGLTF=OFF
target_link_libraries(app PRIVATE m3g::m3g)
# In app startup, before decode:
#   m3g::set_deflate_io(&my_dio);
#   m3g::set_image_io(&my_iio);
#   // optional: set_json_io, set_gltf_io
```

**Export without tree vendors** (use your cgltf/stb trees):

```bash
cmake -S . -B build \
  -DM3G_BUNDLE_MINIZ=OFF -DM3G_BUNDLE_STB=OFF \
  -DM3G_BUNDLE_CJSON=OFF -DM3G_BUNDLE_CGLTF=OFF \
  -DM3G_WITH_EXPORT=ON \
  -DM3G_STB_INCLUDE_DIR=/path/to/parent-of-stb \
  -DM3G_CGLTF_INCLUDE_DIR=/path/to/parent-of-cgltf \
  -DM3G_BUILD_CLI=OFF
```

Then register backends yourself (and link any zlib/image/glTF libs your
callbacks need). `M3G_BUILD_CLI` expects the bundled defaults unless you install
IO another way.

**Consumer (`find_package`) with a full install:**

```cmake
cmake_minimum_required(VERSION 3.16)
project(app LANGUAGES CXX)
find_package(m3g 0.1 REQUIRED CONFIG)
add_executable(app main.cpp)
target_link_libraries(app PRIVATE m3g::m3g)
```

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/usr/local
```

As a subdirectory: `add_subdirectory(m3g)` then
`target_link_libraries(app PRIVATE m3g::m3g)`.

Installed layout: `include/m3g.hpp`, `include/m3g.h`, `lib/libm3g.*`,
`lib/cmake/m3g/m3gConfig.cmake`. Bundled vendors are compiled into the library
only when the corresponding `M3G_BUNDLE_*` option is ON.

## Run

**Viewer** (Sokol + ImGui; orbit with LMB, zoom with wheel):

```bash
make debug    # or: zig build -p build
./build/debug assets/90.m3g
```

**CLI** (M3G → glTF / GLB):

```bash
make release  # or: zig build -p build release
./build/m3g assets/90.m3g out.glb --overwrite
./build/m3g assets/90.m3g out.gltf --pattern texture.png --overwrite --verbose
```

Usage:

```text
m3g <input.m3g> <output.{gltf|glb}> [--pattern <image.{png|jpg|jpeg}>] [--overwrite] [--verbose]
```

## Tests

Tests live under `tests/NNN_*.c` (or `NNN-*.c` / `.cpp`). `make test` or
`zig build test` compiles each selected file (plus `tests/impl.c`) into
`tests/bin/` and runs the binaries. CMake is not required. Unit tests link only
helpers / vendored headers (no OpenGL or X11) for pure C tests; C++ tests link
the m3g library sources.

### Compiler

**Make `test`:**

| Condition                                                                       | Test compiler (`TEST_CC`)                                                  |
| ------------------------------------------------------------------------------- | -------------------------------------------------------------------------- |
| `CROSS_COMPILE` is set                                                          | Same as app build: `$(CC)` (do not use host `musl-gcc` when cross-testing) |
| Else if `musl-gcc` is on `PATH` (or `x86_64-linux-musl-gcc` as a fallback name) | That musl driver                                                           |
| Else                                                                            | `$(CC)` — normally host **glibc** `gcc`                                    |

```bash
make test TEST_CC=gcc
make test TEST_CC=musl-gcc
```

**Zig `test`:** always uses `zig cc` / `zig c++` (works on Windows without
MinGW).

This does **not** switch the **app** Make build (`debug` / `release`). Those
stay on `$(CXX)` / `$(CC)`.

Optional package on Arch if you want musl test builds with Make:

```bash
sudo pacman -S --needed musl   # provides musl-gcc
```

Run all tests:

```bash
make test              # or: zig build test
```

`zig build test` runs `scripts/run_tests.py`. It does **not** require Make.

Run **one or more** tests by number:

```bash
make test n=1
make test n=2,3
zig build test -Dn=1
zig build test -Dn=2,3
```

Run **from a test onward**:

```bash
make test s=1
zig build test -Ds=1
```

`n` and `s` are mutually exclusive (`n` wins if both are set). Needs a C
compiler as above and `vendors/libs/testfw.h` (`make setup-libs` or
`zig build setup-libs`).

## Editor / LSP support

Optional (CMake only for this):

```bash
cmake -S . -B build
# compile_commands.json is generated in build/ (CMAKE_EXPORT_COMPILE_COMMANDS)
```

## Targets (Make and Zig)

| Target          | Action                                          |
| --------------- | ----------------------------------------------- |
| `all` (default) | Viewer (`build/debug`)                          |
| `debug`         | Same as `view` — Sokol + ImGui viewer           |
| `view`          | Alias for `debug`                               |
| `release`       | CLI converter `build/m3g`                       |
| `test`          | Run tests (`n=` / `s=` or Zig `-Dn=` / `-Ds=`)  |
| `doc`           | Doxygen HTML API docs → `docs/api/html/`        |
| `setup`         | Fetch all vendors (run once before first build) |
| `make clean`    | Remove `build/` and `tests/bin/`                |

Zig: `zig build <step>`; install with `-p build` to place binaries under
`build/`.

### API documentation

Requires **doxygen** on `PATH`. Config: `Doxyfile` (inputs: `include/m3g.hpp`,
`include/m3g.h`).

```bash
make doc    # or: zig build doc
# open docs/api/html/index.html
```

Generated HTML is gitignored under `docs/api/`.

### Vendor setup

| Target        | Fetches                                                                        |
| ------------- | ------------------------------------------------------------------------------ |
| `setup`       | All vendors below                                                              |
| `setup-libs`  | `testfw.h` (unit tests)                                                        |
| `setup-cgltf` | `cgltf.h`, `cgltf_write.h` (parse/validate + write)                            |
| `setup-cjson` | `cJSON.c`, `cJSON.h`                                                           |
| `setup-stb`   | `stb_image.h`, `stb_image_write.h`                                             |
| `setup-sokol` | Viewer only: `sokol_app/gfx/glue/log/time`, `sokol_gl`, sokol-imgui trio       |
| `setup-miniz` | miniz 3.1.2 zip (needs `7z`; examples/docs stripped)                           |
| `setup-imgui` | Dear ImGui 1.92.9b zip (needs `7z`; backends/docs/examples/misc/demo stripped) |

Re-run the relevant target to refresh a single vendor. Zig records setup stamps
under `build/setup/` so vendor files are not re-downloaded on every compile.
