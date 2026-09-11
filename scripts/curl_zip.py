#!/usr/bin/env python3
"""Download url -> out path (zip archives for miniz/imgui)."""
from __future__ import annotations

import os
import sys
import urllib.request


def main() -> int:
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} <url> <out>", file=sys.stderr)
        return 2
    url, out = sys.argv[1], sys.argv[2]
    os.makedirs(os.path.dirname(os.path.abspath(out)) or ".", exist_ok=True)
    req = urllib.request.Request(url, headers={"User-Agent": "m3g-setup"})
    with urllib.request.urlopen(req) as r, open(out, "wb") as f:
        f.write(r.read())
    return 0


if __name__ == "__main__":
    sys.exit(main())
