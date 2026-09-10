# Build

`slop` is built with **Make + g++/gcc** only for the **viewer** (`debug`) and
**CLI** (`release`) builds.\
**No CMake** is required to compile, test, or convert M3G files.\
Most third-party code lives under `vendors/` and is fetched with the Makefile
`setup` targets.

```bash
make debug      # Sokol + Dear ImGui viewer → build/debug  (default `make`)
make release    # CLI converter             → build/slop
make test       # unit tests under tests/bin/
```

## Requirements

**Host:** Linux (POSIX; `_DEFAULT_SOURCE` is set for vendor and app code).

**Toolchain (required for build / release / tests):**

| Tool   | Role                                                             |
| ------ | ---------------------------------------------------------------- |
| `g++`  | C++17 compiler (CLI + viewer; `debug.c` is compiled as C++)      |
| `gcc`  | C99 compiler (app C sources, cJSON, tests)                       |
| `make` | Build driver (`Makefile`)                                        |
| `curl` | `make setup` downloads (`-fsSL`)                                 |
| `7z`   | Unpack miniz / imgui zip archives (`setup-miniz`, `setup-imgui`) |

**Not required** for `make debug`, `make release`, or `make test`:

| Tool  | Role if present                                                            |
| ----- | -------------------------------------------------------------------------- |
| CMake | Optional only: `compile_commands.json` for clangd / LSP (`CMakeLists.txt`) |

**System libraries** (headers + link libs):

| Library                               | Used for                                     |
| ------------------------------------- | -------------------------------------------- |
| libm                                  | Math (`-lm`)                                 |
| OpenGL (`libGL`)                      | Viewer only (Sokol GLCORE)                   |
| X11 (`libX11`, `libXi`, `libXcursor`) | Viewer window / input (`sokol_app`)          |
| libdl, libpthread                     | Viewer (dynamic GL, threads)                 |

**CLI** (`build/slop`) links only **libm** (deflate/Adler-32 via vendored **miniz**).\
**Viewer** (`build/debug`) additionally links **OpenGL** and **X11**.

Dear ImGui, Sokol, cJSON, cgltf, stb, and test helpers are **vendored** (not
system packages).

On Arch Linux:

### What is usually already installed

In the table below, `→` means **requires** or **installed as a dependency of**.

| Need                                        | Often already on Arch via     |
| ------------------------------------------- | ----------------------------- |
| **glibc**                                   | `pacman` → `glibc`            |
| `g++`, `gcc`, `make`                        | `base-devel`                  |
| **miniz** (vendored)                        | `make setup-miniz`            |
| **OpenGL** / Mesa (`libGL`) (viewer)        | `mesa` or a GPU driver stack  |
| **X11** (`libX11`, `libXi`, `libXcursor`)   | `libx11` `libxi` `libxcursor` |
| **curl** (`make setup`)                     | `pacman` → `curl`             |
| **7z** (`make setup-miniz` / `setup-imgui`) | `7zip` or `p7zip`             |

### Minimal install

`--needed` skips packages you already have.

```bash
sudo pacman -S --needed base-devel curl mesa libx11 libxi libxcursor 7zip
```

**Vendored sources** — fetch once before the first build (CLI **`curl`**
required; **`7z`** required for miniz and imgui zips):

```bash
make setup
```

This downloads cgltf, cJSON, stb image headers, Sokol (including sokol-imgui),
mattias `testfw.h` / `vecmath.h`, miniz, and Dear ImGui into `vendors/`.

## Build

Default `make` / `make all` builds the **viewer** (`BUILD=debug`). The CLI is
`make release`. Toolchain is always **make + g++/gcc**. **Zig is not used.**

```bash
make                # viewer → build/debug
make debug          # same
make view           # alias for make debug
make release        # CLI    → build/slop
make clean          # rm -rf build tests/bin
```

| Target         | Binary        | Flags                  | Links                                      |
| -------------- | ------------- | ---------------------- | ------------------------------------------ |
| `make debug`   | `build/debug` | `-O0 -g` (C++17 + C99) | m, GL, X11, Xi, Xcursor, dl, pthread |
| `make release` | `build/slop`  | `-Os -g0 -DNDEBUG`     | m                                    |

Object files live under `build/obj/<debug\|release>/` so `build/debug` can be
the viewer executable (not a directory).

Release compiles `src/*.cpp` and `src/*.c` (except `src/debug.c`) plus
`vendors/cjson/cJSON.c` and `vendors/miniz/miniz.c`. Debug links the same library
objects (minus `main.o`)
with `src/debug.c` (as C++) and Dear ImGui.

### Cross-compilation (gcc)

Use a **gcc** target triple via `CROSS_COMPILE` (or set `CC` / `CXX` yourself).
No Zig, no CMake.

```bash
# Example: aarch64 Linux
make release CROSS_COMPILE=aarch64-linux-gnu- CXX=aarch64-linux-gnu-g++

# Viewer still needs target OpenGL/X11 (or skip make debug on a headless cross)
```

You need **target** headers and libraries (`libz`, and for the viewer `libGL` /
X11) for that architecture.

### CMake (optional)

`CMakeLists.txt` remains only for optional developer tooling (LSP
`compile_commands.json`, optional `ctest`):

```bash
cmake -S . -B build && cmake --build build
```

Do **not** use CMake for the normal viewer/CLI workflow. Do **not** use Zig.

## Run

**Viewer** (Sokol + ImGui; orbit with LMB, zoom with wheel):

```bash
make debug
./build/debug assets/90.m3g
```

**CLI** (M3G → glTF / GLB):

```bash
make release
./build/slop assets/90.m3g out.glb --overwrite
./build/slop assets/90.m3g out.gltf --pattern texture.png --overwrite --verbose
```

Usage:

```text
slop <input.m3g> <output.{gltf|glb}> [--pattern <image.{png|jpg|jpeg}>] [--overwrite] [--verbose]
```

## Tests

Tests live under `tests/NNN_*.c` (or `NNN-*.c`). `make test` compiles each
selected file (plus `tests/impl.c`) into `tests/bin/` and runs the binaries.
CMake is not required. Unit tests link only helpers / vendored headers (no
OpenGL or X11).

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
```

This does **not** switch the **app** build (`make debug` / `release`). Those
stay on `$(CXX)` / `$(CC)` and link distro shared libraries.

Optional package on Arch if you want musl test builds:

```bash
sudo pacman -S --needed musl   # provides musl-gcc
```

Run all tests:

```bash
make test
```

Run **one or more** tests by number:

```bash
make test n=1      # tests/001_* only
make test n=2,3    # tests/002_* and 003_*
make test n="2, 3" # same (spaces allowed if quoted)
```

Run **from a test onward**:

```bash
make test s=1   # tests/001_* and all higher-numbered tests
```

`n` and `s` are mutually exclusive (`n` wins if both are set). Needs a C
compiler as above and `vendors/libs/testfw.h` (`make setup-libs`).

## Editor / LSP support

Optional (CMake only for this):

```bash
cmake -S . -B build
# compile_commands.json is generated in build/ (CMAKE_EXPORT_COMPILE_COMMANDS)
```

## Makefile targets

| Target              | Action                                          |
| ------------------- | ----------------------------------------------- |
| `make` / `make all` | Viewer (`build/debug`)                          |
| `make debug`        | Same as `make view` — Sokol + ImGui viewer      |
| `make view`         | Alias for `make debug`                          |
| `make release`      | CLI converter `build/slop`                      |
| `make test`         | Run tests (`n=` / `s=` filters; see **Tests**)  |
| `make clean`        | Remove `build/` and `tests/bin/`                |
| `make setup`        | Fetch all vendors (run once before first build) |

### Vendor setup

| Target             | Fetches                                                                     |
| ------------------ | --------------------------------------------------------------------------- |
| `make setup`       | All vendors below                                                           |
| `make setup-libs`  | `testfw.h`, `vecmath.h`                                                     |
| `make setup-cgltf` | `cgltf.h`, `cgltf_write.h`                                                  |
| `make setup-cjson` | `cJSON.c`, `cJSON.h`                                                        |
| `make setup-stb`   | `stb_image.h`, `stb_image_write.h`, `stb_image_resize2.h`                   |
| `make setup-sokol` | Sokol headers + `sokol_imgui.h` / `sokol_gfx_imgui.h` / `sokol_app_imgui.h` |
| `make setup-miniz` | miniz 3.1.2 zip (needs `7z`)                                                |
| `make setup-imgui` | Dear ImGui 1.92.9b zip (needs `7z`)                                         |

Re-run the relevant target to refresh a single vendor.
