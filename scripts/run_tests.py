#!/usr/bin/env python3
"""Make-free unit-test runner for zig build test (Windows-friendly).

Builds shared m3g/backend objects once under tests/bin/obj/, then links each
test binary against them (same layout as Makefile test-lib).
"""
from __future__ import annotations

import glob
import os
import re
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)

TEST_BINDIR = os.environ.get("TEST_BINDIR", os.path.join("tests", "bin"))
TEST_CC = os.environ.get("TEST_CC", os.environ.get("test_cc", "gcc"))
TEST_CXX = os.environ.get("TEST_CXX", os.environ.get("cxx", "g++"))
TEST_USE_ZIG = os.environ.get("TEST_USE_ZIG", "").strip().lower() in (
    "1",
    "true",
    "yes",
)


def _basename_no_ext(path: str) -> str:
    base = os.path.basename(path)
    low = base.lower()
    if low.endswith(".exe"):
        base = base[:-4]
    return base


def is_zig_compiler(name: str) -> bool:
    return _basename_no_ext(name).lower() == "zig"


TEST_USE_ZIG = TEST_USE_ZIG or is_zig_compiler(TEST_CC)
# Keep host-g++ objs separate from zig-cc objs (different linkers / libstdc++).
_default_obj = os.path.join(TEST_BINDIR, "obj-zig" if TEST_USE_ZIG or is_zig_compiler(TEST_CC) else "obj")
TEST_OBJDIR = os.environ.get("TEST_OBJDIR", _default_obj)

TEST_CFLAGS = os.environ.get("TEST_CFLAGS", "-std=c99 -Wall -Wextra -g").split()
TEST_CXXFLAGS = os.environ.get(
    "TEST_CXXFLAGS", "-std=c++17 -Wall -Wextra -g"
).split()
if os.name != "nt" and "-D_DEFAULT_SOURCE" not in TEST_CFLAGS:
    TEST_CFLAGS.append("-D_DEFAULT_SOURCE")
    TEST_CXXFLAGS.append("-D_DEFAULT_SOURCE")
for _d in (
    "-DM3G_HAS_MINIZ_BACKEND=1",
    "-DM3G_HAS_STB_BACKEND=1",
    "-DM3G_HAS_CJSON_BACKEND=1",
    "-DM3G_HAS_CGLTF_BACKEND=1",
    "-DM3G_IMPL_STB=1",
    "-DM3G_IMPL_CGLTF=1",
):
    if _d not in TEST_CFLAGS:
        TEST_CFLAGS.append(_d)
    if _d not in TEST_CXXFLAGS:
        TEST_CXXFLAGS.append(_d)


def compiler_prefix(cxx: bool) -> list[str]:
    raw = TEST_CXX if cxx else TEST_CC
    if TEST_USE_ZIG or is_zig_compiler(raw):
        zig = raw
        if not os.path.dirname(zig):
            found = shutil.which(zig) or shutil.which(zig + ".exe")
            if found:
                zig = found
        return [zig, "c++" if cxx else "cc"]
    found = shutil.which(raw) or shutil.which(raw + ".exe")
    return [found or raw]


def split_libs(raw: str | None) -> list[str]:
    if raw is None:
        if os.name == "nt":
            return []
        # zig c++ still needs an explicit C++ runtime on some hosts when linking .o files.
        if TEST_USE_ZIG or is_zig_compiler(TEST_CXX):
            return ["-lm", "-lstdc++"]
        return ["-lm"]
    raw = raw.strip()
    if not raw:
        return []
    return raw.split()


TEST_INCLUDES = ["-I.", "-Iinclude", "-Ivendors/libs"]
TEST_M3G_INCLUDES = [
    "-I.",
    "-Iinclude",
    "-Isrc",
    "-Ivendors",
    "-Ivendors/libs",
    "-Ivendors/cgltf",
    "-Ivendors/miniz",
]
TEST_IMPL = os.path.join("tests", "impl.c")
TEST_M3G_LIBS = split_libs(os.environ.get("TEST_M3G_LIBS"))

# Shared library sources (built once into TEST_OBJDIR).
TEST_M3G_CXX_SRCS = [
    "src/converter.cpp",
    "src/decode/decoder.cpp",
    "src/io_adapters_cxx.cpp",
    "src/export/gltf_exporter.cpp",
    "src/gltf/gltf_writer.cpp",
    "src/util/png_writer.cpp",
]
TEST_M3G_C_SRCS = [
    "src/m3g_impl.c",
    "src/deflate_io_miniz.c",
    "src/gltf_io_cgltf.c",
    "src/image_io_stb.c",
    "src/json_io_cjson.c",
    "src/impl.c",
    "vendors/cjson/cJSON.c",
    "vendors/miniz/miniz.c",
]

n = os.environ.get("n", "").strip()
s = os.environ.get("s", "").strip()


def pad3(part: str) -> str:
    try:
        return f"{int(part):03d}"
    except ValueError:
        return part


def selected(num: str) -> bool:
    if n:
        parts = [p for p in n.replace(" ", "").split(",") if p]
        return any(num == pad3(p) for p in parts)
    if s:
        try:
            start = int(s.lstrip("0") or "0")
            num_i = int(num.lstrip("0") or "0")
            return num_i >= start
        except ValueError:
            return True
    return True


def run_cmd(cmd: list[str], *, label: str) -> None:
    print(label)
    try:
        subprocess.check_call(cmd, cwd=ROOT)
    except FileNotFoundError as e:
        print(
            f"error: failed to spawn {cmd[0]!r}: {e}\n"
            f"  full command: {cmd}",
            file=sys.stderr,
        )
        raise SystemExit(1) from e
    except subprocess.CalledProcessError as e:
        print(
            f"error: command failed with exit {e.returncode}: {cmd}",
            file=sys.stderr,
        )
        raise SystemExit(e.returncode) from e


def run_exe(exe_path: str) -> int:
    abs_exe = os.path.abspath(exe_path)
    if not os.path.isfile(abs_exe):
        print(f"error: test binary missing: {abs_exe}", file=sys.stderr)
        return 1
    print(f"RUN {abs_exe}")
    return subprocess.call([abs_exe], cwd=ROOT)


def compile_shared() -> tuple[list[str], str]:
    """Build shared C/C++ objs once. Returns (m3g_objs, impl_obj)."""
    objs: list[str] = []

    # C backends
    for src in TEST_M3G_C_SRCS:
        out = os.path.join(TEST_OBJDIR, os.path.splitext(src)[0] + ".o")
        os.makedirs(os.path.dirname(out), exist_ok=True)
        if not os.path.isfile(out) or os.path.getmtime(src) > os.path.getmtime(out):
            prefix = compiler_prefix(False)
            run_cmd(
                [*prefix, *TEST_CFLAGS, *TEST_M3G_INCLUDES, "-c", "-o", out, src],
                label=f"CC  {src}  (test-lib)",
            )
        objs.append(out)

    # C++ library
    for src in TEST_M3G_CXX_SRCS:
        out = os.path.join(TEST_OBJDIR, os.path.splitext(src)[0] + ".o")
        os.makedirs(os.path.dirname(out), exist_ok=True)
        if not os.path.isfile(out) or os.path.getmtime(src) > os.path.getmtime(out):
            prefix = compiler_prefix(True)
            run_cmd(
                [*prefix, *TEST_CXXFLAGS, *TEST_M3G_INCLUDES, "-c", "-o", out, src],
                label=f"CXX {src}  (test-lib)",
            )
        objs.append(out)

    # testfw as C++ so symbols match C++ tests
    impl_out = os.path.join(TEST_OBJDIR, "tests", "impl.o")
    os.makedirs(os.path.dirname(impl_out), exist_ok=True)
    if not os.path.isfile(impl_out) or os.path.getmtime(TEST_IMPL) > os.path.getmtime(impl_out):
        prefix = compiler_prefix(True)
        run_cmd(
            [
                *prefix,
                *TEST_CXXFLAGS,
                *TEST_M3G_INCLUDES,
                "-x",
                "c++",
                "-c",
                "-o",
                impl_out,
                TEST_IMPL,
            ],
            label=f"CXX {TEST_IMPL}  (testfw)",
        )
    return objs, impl_out


def main() -> int:
    os.makedirs(TEST_BINDIR, exist_ok=True)
    patterns = [
        "tests/[0-9][0-9][0-9]_*.c",
        "tests/[0-9][0-9][0-9]-*.c",
        "tests/[0-9][0-9][0-9]_*.cpp",
        "tests/[0-9][0-9][0-9]-*.cpp",
    ]
    sources: list[str] = []
    for p in patterns:
        sources.extend(sorted(glob.glob(p)))
    seen: set[str] = set()
    uniq: list[str] = []
    for src in sources:
        if src in seen or os.path.basename(src) == "impl.c":
            continue
        seen.add(src)
        uniq.append(src)

    # Only build shared lib if any C++ test is selected
    need_lib = False
    for src in uniq:
        base = os.path.splitext(os.path.basename(src))[0]
        m = re.match(r"^(\d{3})", base)
        if not m or not selected(m.group(1)):
            continue
        if src.endswith(".cpp"):
            need_lib = True
            break

    m3g_objs: list[str] = []
    impl_obj = ""
    if need_lib:
        m3g_objs, impl_obj = compile_shared()

    failed = 0
    exe_suffix = ".exe" if os.name == "nt" else ""

    for src in uniq:
        base = os.path.splitext(os.path.basename(src))[0]
        m = re.match(r"^(\d{3})", base)
        if not m:
            continue
        num = m.group(1)
        if not selected(num):
            continue
        lang = "cpp" if src.endswith(".cpp") else "c"
        exe_out = os.path.join(TEST_BINDIR, base + exe_suffix)
        if lang == "cpp":
            prefix = compiler_prefix(True)
            cmd = [
                *prefix,
                *TEST_CXXFLAGS,
                *TEST_M3G_INCLUDES,
                "-o",
                exe_out,
                src,
                impl_obj,
                *m3g_objs,
                *TEST_M3G_LIBS,
            ]
            run_cmd(cmd, label=f"CXX {exe_out}  [{' '.join(prefix)}]")
        else:
            prefix = compiler_prefix(False)
            cmd = [
                *prefix,
                *TEST_CFLAGS,
                *TEST_INCLUDES,
                "-o",
                exe_out,
                src,
                TEST_IMPL,
            ]
            run_cmd(cmd, label=f"CC  {exe_out}  [{' '.join(prefix)}]")
        if run_exe(exe_out) != 0:
            failed = 1

    if failed:
        print("Some tests failed.")
        return 1
    print("All selected tests passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
