#!/usr/bin/env python3
"""Make-free unit-test runner for ninja test (Windows-friendly)."""
from __future__ import annotations

import glob
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)

TEST_BINDIR = os.environ.get("TEST_BINDIR", os.path.join("tests", "bin"))
TEST_CC = os.environ.get("TEST_CC", os.environ.get("test_cc", "gcc"))
TEST_CXX = os.environ.get("TEST_CXX", os.environ.get("cxx", "g++"))
TEST_CFLAGS = os.environ.get(
    "TEST_CFLAGS", "-std=c99 -Wall -Wextra -g"
).split()
TEST_CXXFLAGS = os.environ.get(
    "TEST_CXXFLAGS", "-std=c++17 -Wall -Wextra -g"
).split()
# Optional POSIX define when building under mingw-w64 with feature macros
if os.name != "nt" and "-D_DEFAULT_SOURCE" not in TEST_CFLAGS:
    TEST_CFLAGS.append("-D_DEFAULT_SOURCE")
    TEST_CXXFLAGS.append("-D_DEFAULT_SOURCE")

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
TEST_M3G_LIBS = os.environ.get("TEST_M3G_LIBS", "-lm").split()
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
    seen = set()
    uniq = []
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
            cmd = (
                [TEST_CXX, *TEST_CXXFLAGS, *TEST_M3G_INCLUDES, "-o", exe_out, src, TEST_IMPL]
                + TEST_M3G_SRCS
                + TEST_M3G_LIBS
            )
            print(f"CXX {exe_out}  [{TEST_CXX}]")
        else:
            cmd = [TEST_CC, *TEST_CFLAGS, *TEST_INCLUDES, "-o", exe_out, src, TEST_IMPL]
            print(f"CC  {exe_out}  [{TEST_CC}]")
        subprocess.check_call(cmd)
        print(f"RUN {exe_out}")
        r = subprocess.call([exe_out])
        if r != 0:
            failed = 1

    if failed:
        print("Some tests failed.")
        return 1
    print("All selected tests passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
