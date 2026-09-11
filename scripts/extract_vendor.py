#!/usr/bin/env python3
"""Extract miniz or imgui zip into vendors/ (needs 7z on PATH)."""
from __future__ import annotations

import os
import shutil
import subprocess
import sys


def run_7z(archive: str, out_dir: str) -> None:
    subprocess.check_call(["7z", "x", archive, f"-o{out_dir}", "-y"])


def touch(path: str) -> None:
    os.makedirs(os.path.dirname(os.path.abspath(path)) or ".", exist_ok=True)
    with open(path, "wb"):
        pass


def extract_miniz(archive: str, dest_dir: str, stamp: str) -> None:
    if os.path.isdir(dest_dir):
        shutil.rmtree(dest_dir)
    os.makedirs(dest_dir, exist_ok=True)
    run_7z(archive, dest_dir)
    ex = os.path.join(dest_dir, "examples")
    if os.path.isdir(ex):
        shutil.rmtree(ex)
    for name in ("ChangeLog.md", "readme.md"):
        p = os.path.join(dest_dir, name)
        if os.path.isfile(p):
            os.remove(p)
    touch(stamp)


def extract_imgui(archive: str, dest_dir: str, tag: str, stamp: str) -> None:
    if os.path.isdir(dest_dir):
        shutil.rmtree(dest_dir)
    os.makedirs(dest_dir, exist_ok=True)
    run_7z(archive, dest_dir)
    nested = os.path.join(dest_dir, f"imgui-{tag}")
    if os.path.isdir(nested):
        for name in os.listdir(nested):
            shutil.move(os.path.join(nested, name), os.path.join(dest_dir, name))
        shutil.rmtree(nested)
    for name in ("backends", "docs", "examples", "misc"):
        p = os.path.join(dest_dir, name)
        if os.path.isdir(p):
            shutil.rmtree(p)
    demo = os.path.join(dest_dir, "imgui_demo.cpp")
    if os.path.isfile(demo):
        os.remove(demo)
    touch(stamp)


def main() -> int:
    if len(sys.argv) < 2:
        print(
            f"usage: {sys.argv[0]} miniz <archive> <dir> <stamp>\n"
            f"       {sys.argv[0]} imgui <archive> <dir> <tag> <stamp>",
            file=sys.stderr,
        )
        return 2
    kind = sys.argv[1]
    if kind == "miniz":
        _, _, archive, dest_dir, stamp = sys.argv
        extract_miniz(archive, dest_dir, stamp)
    elif kind == "imgui":
        _, _, archive, dest_dir, tag, stamp = sys.argv
        extract_imgui(archive, dest_dir, tag, stamp)
    else:
        print(f"unknown kind: {kind}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
