#!/usr/bin/env python3
# Copyright (c) 2026 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

"""Convert Zephyr RST documentation to GitHub-Flavored Markdown.

Usage:
  # Single file
  python3 scripts/docs/rst2md.py doc/releases/release-notes-4.4.rst

  # All release notes
  python3 scripts/docs/rst2md.py doc/releases/release-notes-*.rst -o /tmp/md/

  # Whole doc tree
  python3 scripts/docs/rst2md.py doc/ -o /tmp/md/

Dependencies: docutils, markdownify (both installable via pip)
"""

import argparse
import re
import sys
from pathlib import Path

try:
    from docutils import nodes
    from docutils.parsers.rst import roles
    from docutils.writers.html5_polyglot import Writer
    from docutils.core import publish_string
except ImportError:
    sys.exit("Missing: pip install docutils")

try:
    from markdownify import markdownify
    from bs4 import BeautifulSoup
except ImportError:
    sys.exit("Missing: pip install markdownify beautifulsoup4")


# Sphinx roles not known to plain docutils — render as inline code or plain text.
_INLINE_CODE_ROLES = {
    "kconfig:option",
    "dtcompatible",
    "c:func",
    "c:struct",
    "c:type",
    "c:macro",
    "c:member",
    "c:enumerator",
    "c:enum",
    "c:var",
    "func",
    "struct",
    "type",
    "macro",
    "var",
    "file",
}

_PLAIN_TEXT_ROLES = {
    "ref",
    "doc",
    "zephyr:board",
    "zephyr:board-catalog",
    "zephyr:code-sample",
    "zephyr:code-sample-category",
    "abbr",
    "cve",
    "numref",
    "term",
    "any",
    "github",
    "jira",
}


def _make_code_role(role_name):
    def role(name, rawtext, text, lineno, inliner, options=None, content=None):
        display = re.sub(r'\s*<[^>]+>$', '', text)
        return [nodes.literal(rawtext, display)], []

    role.options = {}
    role.content = False
    return role


def _make_text_role(role_name):
    def role(name, rawtext, text, lineno, inliner, options=None, content=None):
        display = re.sub(r'\s*<[^>]+>$', '', text)
        return [nodes.Text(display)], []

    role.options = {}
    role.content = False
    return role


def _register_stub_roles():
    for r in _INLINE_CODE_ROLES:
        roles.register_canonical_role(r, _make_code_role(r))
    for r in _PLAIN_TEXT_ROLES:
        roles.register_canonical_role(r, _make_text_role(r))


def _preprocess_rst(text: str) -> str:
    """Strip Sphinx-only directives that confuse plain docutils."""
    # Remove :orphan: field
    text = re.sub(r'^:orphan:\s*\n', '', text, flags=re.MULTILINE)
    # Remove .. comment blocks (.. followed by indented lines)
    text = re.sub(r'^\.\.\s*\n([ \t]+[^\n]*\n)*', '', text, flags=re.MULTILINE)
    # Remove standalone .. directives we can't render (board-catalog, code-sample, etc.)
    text = re.sub(
        r'^\.\.\s+(zephyr:|board-|code-)[^\n]*\n([ \t]+[^\n]*\n)*',
        '',
        text,
        flags=re.MULTILINE,
    )
    return text


def _unwrap_definition_lists(html: str) -> str:
    """Replace <dl>/<dt>/<dd> with plain paragraphs so markdownify skips PHP MD syntax."""
    soup = BeautifulSoup(html, "html.parser")
    for dl in soup.find_all("dl"):
        items = []
        for child in list(dl.children):
            if getattr(child, "name", None) == "dt":
                items.append(child)
            elif getattr(child, "name", None) == "dd":
                items.extend(list(child.children))
        dl.replace_with(*items if items else [""])
    return str(soup)


def _postprocess_md(text: str) -> str:
    """Clean up markdownify output."""
    # Remove excessive blank lines
    text = re.sub(r'\n{3,}', '\n\n', text)
    return text.strip()


def rst_to_md(rst_text: str) -> str:
    rst_text = _preprocess_rst(rst_text)
    html = publish_string(
        rst_text,
        writer=Writer(),
        settings_overrides={
            "halt_level": 5,
            "report_level": 5,
            "output_encoding": "unicode",
        },
    )
    if isinstance(html, bytes):
        html = html.decode()

    html = _unwrap_definition_lists(html)

    md = markdownify(
        html,
        heading_style="ATX",
        strip=["div", "footer", "nav"],
    )

    # Remove the duplicate <title> that docutils inserts as the first H1
    # (markdownify renders it before the actual document heading)
    lines = md.split('\n')
    if lines and not lines[0].startswith('#'):
        # First non-empty paragraph before the first heading is the page title — drop it
        first_heading = next((i for i, l in enumerate(lines) if l.startswith('# ')), None)
        if first_heading is not None:
            lines = lines[first_heading:]
        md = '\n'.join(lines)

    return _postprocess_md(md)


def convert_file(src: Path, dst: Path) -> None:
    dst.parent.mkdir(parents=True, exist_ok=True)
    rst = src.read_text(encoding="utf-8")
    md = rst_to_md(rst)
    dst.write_text(md + '\n', encoding="utf-8")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("sources", nargs="+", type=Path, help="RST file(s) or directory")
    ap.add_argument("-o", "--outdir", type=Path, default=None,
                    help="Output directory (default: print to stdout for single file)")
    args = ap.parse_args()

    _register_stub_roles()

    files: list[tuple[Path, Path | None]] = []

    for src in args.sources:
        if src.is_dir():
            for rst in sorted(src.rglob("*.rst")):
                rel = rst.relative_to(src)
                dst = (args.outdir / rel).with_suffix(".md") if args.outdir else None
                files.append((rst, dst))
        elif src.is_file():
            if args.outdir:
                dst = (args.outdir / src.name).with_suffix(".md")
            else:
                dst = None
            files.append((src, dst))
        else:
            print(f"warning: {src} not found, skipping", file=sys.stderr)

    if not files:
        ap.error("No RST files found")

    if len(files) == 1 and files[0][1] is None:
        # Single file, no outdir → stdout
        src, _ = files[0]
        print(rst_to_md(src.read_text(encoding="utf-8")))
        return

    for src, dst in files:
        if dst is None:
            dst = Path(src.stem + ".md")
        print(f"  {src} → {dst}", file=sys.stderr)
        convert_file(src, dst)

    print(f"Done. {len(files)} file(s) converted.", file=sys.stderr)


if __name__ == "__main__":
    main()
