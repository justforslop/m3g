# Build

`m3g` is built with **g++/gcc** for the **viewer** (`debug`) and **CLI**
(`release`) builds. Use either **Make** (`Makefile`) or **Ninja**
(`build.ninja`) — same outputs and flags.\
**Make or Ninja** is enough to compile, test, or convert M3G files. **CMake** is
optional for packaging (`find_package`), `ctest`, and LSP `compile_commands`.\
Most third-party code lives under `vendors/` and is fetched with the `setup`
targets (Make or Ninja).

```bash
# Make (Linux / POSIX)
make debug      # Sokol + Dear ImGui viewer → build/debug  (default `make`)
make release    # CLI converter             → build/m3g
make test       # unit tests under tests/bin/

# Ninja — one file: build.ninja (Linux defaults; Windows: see below)
ninja           # viewer → build/debug  (default)
ninja release   # CLI    → build/m3g
ninja test      # unit tests under tests/bin/
```

## Requirements

**Host:**

| Host | Driver | Notes |
| ---- | ------ | ----- |
| **Linux** (POSIX) | Make and/or Ninja | `_DEFAULT_SOURCE` set; viewer needs X11 + GL |
| **Windows** | **Ninja** + **MinGW** `g++`/`gcc` | Uncomment Win block in `build.ninja` (or CLI overrides); viewer uses Win32 + `opengl32` |

MSVC `cl.exe` is **not** wired in `build.ninja` (GCC-style flags). Use **CMake**
if you need MSVC. **Make** remains Linux/POSIX-oriented.

**Toolchain (required for build / release / tests):**

| Tool    | Role                                                                  |
| ------- | --------------------------------------------------------------------- |
| `g++` / `c++` | C++17 compiler (CLI + viewer; `debug.c` is compiled as C++)     |
| `gcc`   | C99 compiler (app C sources, cJSON, tests)                            |
| `make`  | Build driver (`Makefile`) — Linux/POSIX — **or**                      |
| `ninja` | Build driver (`build.ninja`) — **Linux and Windows**                  |
| `python3` | Ninja `setup` / `test` / `doc` helpers (`scripts/*.py`)             |
| `7z`    | Unpack miniz / imgui zip archives (`setup-miniz`, `setup-imgui`)      |

You need **either** `make` **or** `ninja` on Linux (or both). On Windows use
**ninja**. Make still uses `curl` for `setup`; Ninja `setup` uses Python
(`urllib`) plus `7z` for zips.

Everything lives in **`build.ninja`** (no `conf.ninja` / configure step).
Linux link flags are the defaults. Ninja cannot `if` on OS, so Windows is:

1. Uncomment the **Windows MinGW overrides** block at the top of `build.ninja`, or
2. Override on the CLI, for example:

```bash
ninja release exe=.exe cxx=g++ view_libs="-lm -lopengl32 -lgdi32" posix_cppflags= py=python
```

**Not required** for debug / release / test builds:

| Tool  | Role if present                                                            |
| ----- | -------------------------------------------------------------------------- |
| CMake | Optional: installable package, `ctest`, `compile_commands.json`, MSVC      |
| `curl` | Make `setup` only (Ninja setup uses Python)                              |

**System libraries** (headers + link libs):

| Library | Platform | Used for |
| ------- | -------- | -------- |
| libm (`-lm`) | Linux, MinGW | Math |
| OpenGL (`libGL`) | Linux | Viewer (Sokol GLCORE) |
| X11 (`libX11`, `libXi`, `libXcursor`) | Linux | Viewer window / input |
| libdl, libpthread | Linux | Viewer |
| `opengl32`, `gdi32` | Windows | Viewer (Sokol Win32 + GL) |

**CLI** (`build/m3g` / `build/m3g.exe`) links **libm** (deflate via vendored **miniz**).\
**Viewer** (`build/debug` / `build/debug.exe`) additionally links platform GL/window libs.

Dear ImGui, Sokol, cJSON, cgltf, stb, and test helpers are **vendored** (not
system packages).

### Windows (Ninja + MinGW)

1. Install **ninja**, **MinGW-w64** `g++`/`gcc`, **Python 3**, and **7z** on `PATH`.
2. Uncomment the Windows override block in `build.ninja` (or pass CLI overrides).
3. Build:

```bat
ninja setup
ninja release
ninja
build\m3g.exe assets\90.m3g out.glb --overwrite
build\debug.exe assets\90.m3g
```

Tests: `ninja test` → `scripts/run_tests.py`. Filters: `set n=1&& ninja test`.

On Arch Linux:

### What is usually already installed

In the table below, `→` means **requires** or **installed as a dependency of**.

| Need                                        | Often already on Arch via     |
| ------------------------------------------- | ----------------------------- |
| **glibc**                                   | `pacman` → `glibc`            |
| `g++`, `gcc`, `make`                        | `base-devel`                  |
| **ninja** (optional alternative driver)     | `pacman` → `ninja`            |
| **miniz** (vendored)                        | `make setup-miniz` / `ninja setup-miniz` |
| **OpenGL** / Mesa (`libGL`) (viewer)        | `mesa` or a GPU driver stack  |
| **X11** (`libX11`, `libXi`, `libXcursor`)   | `libx11` `libxi` `libxcursor` |
| **curl** (Make `setup`)                     | `pacman` → `curl`             |
| **python** (Ninja `setup` / `test`)         | `pacman` → `python`           |
| **7z** (`setup-miniz` / `setup-imgui`)      | `7zip` or `p7zip`             |

### Minimal install

`--needed` skips packages you already have.

```bash
sudo pacman -S --needed base-devel ninja curl python mesa libx11 libxi libxcursor 7zip
```

**Vendored sources** — fetch once before the first build (CLI **`curl`**
required; **`7z`** required for miniz and imgui zips):

```bash
make setup    # or: ninja setup
```

This downloads cgltf, cJSON, stb image headers, Sokol (including sokol-imgui),
mattias `testfw.h`, miniz, and Dear ImGui (core only) into `vendors/`.

## Build

Default builds the **viewer** (debug). The CLI is the **release** target.
Toolchain is always **g++/gcc**, driven by **Make** or **Ninja**. **Zig is not
used.**

```bash
# Make
make                # viewer → build/debug
make debug          # same
make view           # alias for make debug
make release        # CLI    → build/m3g
make clean          # rm -rf build tests/bin

# Ninja (single build.ninja; Windows: uncomment Win overrides in file)
ninja               # viewer → build/debug[.exe]
ninja debug         # same
ninja view          # alias for debug
ninja release       # CLI    → build/m3g[.exe]
ninja -t clean      # remove ninja-known outputs
# full wipe (matches make clean):
rm -rf build tests/bin          # POSIX
# rmdir /s /q build tests\bin   # Windows cmd
```

| Target (Make / Ninja) | Binary | Flags | Links |
| --------------------- | ------ | ----- | ----- |
| `debug` / `view` | `build/debug` (`.exe` on Windows) | `-O0 -g` (C++17 + C99) | **Linux:** m, GL, X11, Xi, Xcursor, dl, pthread · **Windows:** m, opengl32, gdi32 |
| `release` | `build/m3g` (`.exe` on Windows) | `-Os -g0 -DNDEBUG` | m |

Object files live under `build/obj/<debug\|release>/` so `build/debug` can be
the viewer executable (not a directory).

Release compiles `src/*.cpp` and `src/*.c` (except `src/debug.c`) plus
`vendors/cjson/cJSON.c` and `vendors/miniz/miniz.c`. Debug links the same library
objects (minus `main.o`)
with `src/debug.c` (as C++) and Dear ImGui.

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

The tree’s impl unit is `src/decode/decoder.cpp` (defines `M3G_IMPL`).
`m3g.hpp` does **not** include miniz or stb_image.

**Callbacks (backends):**

| Concern | API | Optional adapter in tree |
| -------- | --- | ------------------------ |
| zlib/deflate (sections, embedded images) | `m3g::DeflateIo` / `set_deflate_io` | `src/deflate_io_miniz.cpp` → `install_miniz_deflate_io()` |
| raster decode | `m3g::ImageIo` / `set_image_io` | `src/image_io_stb.cpp` → `install_stb_image_io()` |
| JSON (optional legacy) | `m3g::JsonIo` / `set_json_io` | `src/json_io_cjson.cpp` → `install_cjson_json_io()` |
| glTF write/parse | `m3g::GltfIo` / `set_gltf_io` | `src/gltf_io_cgltf.cpp` → `install_cgltf_gltf_io()` |

**glTF export** builds a `cgltf_data` tree, then calls **`m3g::gltf_write_file`**
(callback; default adapter uses `cgltf_write_file`). External `.bin` / images
are written by the app; optional validate uses `gltf_parse_file` / `gltf_validate`.

Both adapters auto-register via static init when linked. PNG **write** still
uses `stb_image_write` in `src/util/png_writer.cpp` / glTF export for now.

Convert/export remain separate translation units for now.

### Cross-compilation (gcc)

Use a **gcc** target triple via `CROSS_COMPILE` (or set `CC` / `CXX` yourself).
No Zig, no CMake. Cross flags are wired through the **Makefile**; with Ninja,
override `cc` / `cxx` on the command line:

```bash
# Make — example: aarch64 Linux
make release CROSS_COMPILE=aarch64-linux-gnu- CXX=aarch64-linux-gnu-g++

# Ninja — override top-level cc/cxx variables
ninja release cc=aarch64-linux-gnu-gcc cxx=aarch64-linux-gnu-g++

# Viewer still needs target OpenGL/X11 (or skip debug on a headless cross)
```

You need **target** headers and libraries (`libz`, and for the viewer `libGL` /
X11) for that architecture.

### CMake (optional package + tools)

CMake builds an installable **library package** `m3g::m3g` (static by default),
optional CLI, tests, and `compile_commands.json`.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build build
cmake --install build
```

| Option | Default (top-level) | Meaning |
| ------ | ------------------- | ------- |
| `M3G_BUILD_CLI` | ON | `m3g` CLI executable |
| `M3G_BUILD_VIEWER` | OFF | Sokol viewer (`m3g-view`) |
| `M3G_BUILD_TESTS` | ON | unit tests + `ctest` |
| `M3G_INSTALL` | ON | install + Config package |
| `M3G_ENABLE_IPO` | OFF | LTO |
| `BUILD_SHARED_LIBS` | OFF | shared vs static `libm3g` |

**Consumer (`find_package`):**

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

As a subdirectory: `add_subdirectory(m3g)` then `target_link_libraries(app PRIVATE m3g::m3g)`.

Installed layout: `include/m3g.hpp`, `include/m3g.h`, `lib/libm3g.*`,
`lib/cmake/m3g/m3gConfig.cmake`. Vendored miniz/cgltf/stb/cJSON are compiled
into the library (not separate find modules).

## Run

**Viewer** (Sokol + ImGui; orbit with LMB, zoom with wheel):

```bash
make debug    # or: ninja
./build/debug assets/90.m3g
```

**CLI** (M3G → glTF / GLB):

```bash
make release  # or: ninja release
./build/m3g assets/90.m3g out.glb --overwrite
./build/m3g assets/90.m3g out.gltf --pattern texture.png --overwrite --verbose
```

Usage:

```text
m3g <input.m3g> <output.{gltf|glb}> [--pattern <image.{png|jpg|jpeg}>] [--overwrite] [--verbose]
```

## Tests

Tests live under `tests/NNN_*.c` (or `NNN-*.c`). `make test` or `ninja test`
compiles each selected file (plus `tests/impl.c`) into `tests/bin/` and runs
the binaries. CMake is not required. Unit tests link only helpers / vendored
headers (no OpenGL or X11).

### Compiler (musl if present, else glibc gcc)

For **`make test` only**, the Makefile picks the C compiler as follows:

| Condition                                                                       | Test compiler (`TEST_CC`)                                                  |
| ------------------------------------------------------------------------------- | -------------------------------------------------------------------------- |
| `CROSS_COMPILE` is set                                                          | Same as app build: `$(CC)` (do not use host `musl-gcc` when cross-testing) |
| Else if `musl-gcc` is on `PATH` (or `x86_64-linux-musl-gcc` as a fallback name) | That musl driver                                                           |
| Else                                                                            | `$(CC)` — normally host **glibc** `gcc`                                    |

Override at any time:

```bash
make test TEST_CC=gcc          # force glibc gcc
make test TEST_CC=musl-gcc     # force musl
# Ninja: override the `test_cc` variable
ninja test test_cc=musl-gcc
```

This does **not** switch the **app** build (`debug` / `release`). Those stay on
`$(CXX)` / `$(CC)` (or Ninja `cxx` / `cc`) and link distro shared libraries.

Optional package on Arch if you want musl test builds:

```bash
sudo pacman -S --needed musl   # provides musl-gcc
```

Run all tests:

```bash
make test     # or: ninja test
```

`ninja test` runs `scripts/run_tests.py` (Make still has its own recipe).
It does **not** require Make.

Run **one or more** tests by number:

```bash
make test n=1      # tests/001_* only
make test n=2,3    # tests/002_* and 003_*
make test n="2, 3" # same (spaces allowed if quoted)
n=1 ninja test     # Ninja: pass filters as env vars
n=2,3 ninja test
# Windows cmd:
set n=1&& ninja test
```

Run **from a test onward**:

```bash
make test s=1   # tests/001_* and all higher-numbered tests
s=1 ninja test
```

`n` and `s` are mutually exclusive (`n` wins if both are set). Needs a C
compiler as above and `vendors/libs/testfw.h` (`make setup-libs` or
`ninja setup-libs`).

## Editor / LSP support

Optional (CMake only for this):

```bash
cmake -S . -B build
# compile_commands.json is generated in build/ (CMAKE_EXPORT_COMPILE_COMMANDS)
```

## Targets (Make and Ninja)

Same target names work with both drivers (`make <target>` or `ninja <target>`),
except clean (see below).

| Target              | Action                                          |
| ------------------- | ----------------------------------------------- |
| `all` (default)     | Viewer (`build/debug`)                          |
| `debug`             | Same as `view` — Sokol + ImGui viewer           |
| `view`              | Alias for `debug`                               |
| `release`           | CLI converter `build/m3g`                      |
| `test`              | Run tests (`n=` / `s=` filters; see **Tests**)  |
| `doc`               | Doxygen HTML API docs → `docs/api/html/`        |
| `setup`             | Fetch all vendors (run once before first build) |
| `make clean`        | Remove `build/` and `tests/bin/`                |
| `ninja -t clean`    | Remove outputs known to Ninja                   |

### API documentation

Requires **doxygen** on `PATH`. Config: `Doxyfile` (inputs: `include/m3g.hpp`,
`include/m3g.h`).

```bash
make doc    # or: ninja doc
# open docs/api/html/index.html
```

Generated HTML is gitignored under `docs/api/`.

### Vendor setup

| Target        | Fetches                                                                                          |
| ------------- | ------------------------------------------------------------------------------------------------ |
| `setup`       | All vendors below                                                                                |
| `setup-libs`  | `testfw.h` (unit tests)                                                                          |
| `setup-cgltf` | `cgltf.h`, `cgltf_write.h` (parse/validate + write)                                               |
| `setup-cjson` | `cJSON.c`, `cJSON.h`                                                                             |
| `setup-stb`   | `stb_image.h`, `stb_image_write.h`                                                               |
| `setup-sokol` | Viewer only: `sokol_app/gfx/glue/log/time`, `sokol_gl`, sokol-imgui trio                          |
| `setup-miniz` | miniz 3.1.2 zip (needs `7z`; examples/docs stripped)                                             |
| `setup-imgui` | Dear ImGui 1.92.9b zip (needs `7z`; backends/docs/examples/misc/demo stripped)                   |

Re-run the relevant target to refresh a single vendor. Ninja records setup
stamps under `build/setup/` so vendor files are not re-downloaded on every
compile.
