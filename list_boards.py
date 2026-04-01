#!/usr/bin/env python3
"""List all Zephyr board identifiers, excluding _ns variants."""

import re
import sys
from pathlib import Path

BOARDS_DIR = Path(__file__).parent / "boards"
IDENTIFIER_RE = re.compile(r"^identifier:\s*(\S+)", re.MULTILINE)


def main():
    identifiers = set()
    for yaml_file in BOARDS_DIR.rglob("*.yaml"):
        text = yaml_file.read_text(errors="replace")
        for m in IDENTIFIER_RE.finditer(text):
            ident = m.group(1)
            if not ident.endswith("/ns") and "_ns" not in ident:
                identifiers.add(ident)

    for ident in sorted(identifiers):
        print(ident)

    print(f"\n# Total: {len(identifiers)}", file=sys.stderr)


if __name__ == "__main__":
    main()
