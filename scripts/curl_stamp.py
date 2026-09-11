#!/usr/bin/env python3
"""Download url -> dest and touch stamp out (portable setup helper)."""
from __future__ import annotations

import os
import sys
import urllib.request


def main() -> int:
    if len(sys.argv) != 4:
        print(f"usage: {sys.argv[0]} <url> <dest> <stamp>", file=sys.stderr)
        return 2
    url, dest, stamp = sys.argv[1], sys.argv[2], sys.argv[3]
    os.makedirs(os.path.dirname(os.path.abspath(dest)) or ".", exist_ok=True)
    os.makedirs(os.path.dirname(os.path.abspath(stamp)) or ".", exist_ok=True)
    # curl-compatible: fail on HTTP errors
    req = urllib.request.Request(url, headers={"User-Agent": "m3g-setup"})
    with urllib.request.urlopen(req) as r, open(dest, "wb") as f:
        f.write(r.read())
    with open(stamp, "wb") as f:
        pass
    return 0


if __name__ == "__main__":
    sys.exit(main())
