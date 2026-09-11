#!/usr/bin/env python3
"""Make-free unit-test runner for zig build test (Windows-friendly)."""
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
# When TEST_USE_ZIG=1 (or compiler basename is "zig"), invoke `zig cc` / `zig c++`.
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

TEST_CFLAGS = os.environ.get("TEST_CFLAGS", "-std=c99 -Wall -Wextra -g").split()
TEST_CXXFLAGS = os.environ.get(
    "TEST_CXXFLAGS", "-std=c++17 -Wall -Wextra -g"
).split()
# Optional POSIX define when not on Windows
if os.name != "nt" and "-D_DEFAULT_SOURCE" not in TEST_CFLAGS:
    TEST_CFLAGS.append("-D_DEFAULT_SOURCE")
    TEST_CXXFLAGS.append("-D_DEFAULT_SOURCE")
# Match CMake M3G_BUNDLE_* / Makefile M3G_BACKEND_DEFS (vendored default IO).
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
        # Resolve zig.exe on PATH if only "zig" was passed without a directory.
        zig = raw
        if not os.path.dirname(zig):
            found = shutil.which(zig) or shutil.which(zig + ".exe")
            if found:
                zig = found
        return [zig, "c++" if cxx else "cc"]
    # Host gcc/g++: resolve .cmd shims on Windows via which.
    found = shutil.which(raw) or shutil.which(raw + ".exe")
    return [found or raw]


def split_libs(raw: str | None) -> list[str]:
    if raw is None:
        # Default: libm on POSIX; omit on Windows (zig/msvc/mingw differ).
        if os.name == "nt":
            return []
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
TEST_M3G_SRCS = [
    "src/converter.cpp",
    "src/decode/decoder.cpp",
    "src/deflate_io_miniz.cpp",
    "src/export/gltf_exporter.cpp",
    "src/gltf/gltf_writer.cpp",
    "src/gltf_io_cgltf.cpp",
    "src/image_io_stb.cpp",
    "src/json_io_cjson.cpp",
    "src/util/png_writer.cpp",
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
    """Run a subprocess; never use shell=True (Windows quoting breaks)."""
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
    """Run a built test binary with an absolute path (Windows CreateProcess)."""
    abs_exe = os.path.abspath(exe_path)
    if not os.path.isfile(abs_exe):
        print(f"error: test binary missing: {abs_exe}", file=sys.stderr)
        return 1
    print(f"RUN {abs_exe}")
    # Absolute path avoids Win32 relative-path spawn failures.
    return subprocess.call([abs_exe], cwd=ROOT)


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
    # de-dup preserve order
    seen: set[str] = set()
    uniq: list[str] = []
    for src in sources:
        if src in seen or os.path.basename(src) == "impl.c":
            continue
        seen.add(src)
        uniq.append(src)

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
            cmd = (
                [
                    *prefix,
                    *TEST_CXXFLAGS,
                    *TEST_M3G_INCLUDES,
                    "-o",
                    exe_out,
                    src,
                    TEST_IMPL,
                ]
                + TEST_M3G_SRCS
                + TEST_M3G_LIBS
            )
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
