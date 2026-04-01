#!/usr/bin/env python3
# /// script
# requires-python = ">=3.11"
# dependencies = [
#   "pyelftools",
# ]
# ///
"""
Extract SoC part number from Zephyr ELF files using CONFIG_SOC_* ABS symbols.

Strategy:
  Tier 1: CONFIG_SOC_PART_NUMBER_<X> = 1  → explicit full part number (NXP, Infineon, etc.)
  Tier 2: CONFIG_SOC_SERIES_<S> + prefix matching + structural exclusion filter
  Tier 3: no-series fallback for older HW model v1 builds
"""

import argparse
import re
import sys

from elftools.elf.elffile import ELFFile
from elftools.elf.sections import SymbolTableSection

# Structural CONFIG_SOC_* prefixes that are never SoC names.
# These correspond to stable Kconfig roles, not individual parts.
EXCLUDE_PREFIXES = (
    "FAMILY_",
    "SERIES_",
    "FLASH_",
    "DCDC_",
    "COMPATIBLE_",
    "HAS_",
    "LOG_",
    "TOOLCHAIN_",
    "RESET_HOOK",
    "EARLY_INIT_HOOK",
    "LATE_INIT_HOOK",
    "PART_NUMBER_",   # handled separately as tier 1
    "HFXO_",
    "LFXO_",
    "ENABLE_",
)


def get_soc_symbols(elf_path: str) -> dict[str, int] | None:
    """
    Parse the ELF symbol table and return {symbol_name: integer_value}
    for all CONFIG_SOC_* absolute symbols.
    Returns None if no such symbols are found (not a Zephyr ELF).
    """
    try:
        with open(elf_path, "rb") as f:
            elf = ELFFile(f)
            symbols: dict[str, int] = {}
            for section in elf.iter_sections():
                if not isinstance(section, SymbolTableSection):
                    continue
                for sym in section.iter_symbols():
                    if sym["st_shndx"] != "SHN_ABS":
                        continue
                    if not sym.name.startswith("CONFIG_SOC_"):
                        continue
                    symbols[sym.name] = sym["st_value"]
    except Exception:
        return None
    return symbols or None


def strip_trailing_x(series: str) -> str:
    """
    Derive the SoC prefix from a series name by stripping trailing X characters.
      NRF52X    -> NRF52
      STM32G4X  -> STM32G4
      IMXRT10XX -> IMXRT10
      ESP32S3   -> ESP32S3   (no trailing X)
      NRF54HX   -> NRF54H
    """
    return re.sub(r"X+$", "", series, flags=re.IGNORECASE)


def is_structural(part: str) -> bool:
    """Return True if CONFIG_SOC_<part> is a structural/utility symbol, not a SoC name."""
    if part in ("PART_NUMBER", "FAMILY", "SERIES"):
        return True
    return any(part.startswith(p) for p in EXCLUDE_PREFIXES)


def extract_soc(symbols: dict[str, int]) -> dict:
    active = {name for name, val in symbols.items() if val == 1}

    # --- Tier 1: explicit PART_NUMBER_* ---
    part_numbers = [
        name.removeprefix("CONFIG_SOC_PART_NUMBER_")
        for name in active
        if name.startswith("CONFIG_SOC_PART_NUMBER_")
    ]

    # --- Family / series ---
    families = sorted(
        name.removeprefix("CONFIG_SOC_FAMILY_")
        for name in active
        if name.startswith("CONFIG_SOC_FAMILY_")
    )
    series_list = sorted(
        name.removeprefix("CONFIG_SOC_SERIES_")
        for name in active
        if name.startswith("CONFIG_SOC_SERIES_")
    )

    # --- Tier 2: series-anchored prefix match ---
    candidates: list[str] = []
    for series in series_list:
        prefix = strip_trailing_x(series)
        for name in active:
            part = name.removeprefix("CONFIG_SOC_")
            if part == name:
                continue
            if not part.startswith(prefix):
                continue
            if is_structural(part):
                continue
            candidates.append(part)

    # --- Tier 3: no-series fallback ---
    # Older Zephyr builds (HW model v1) may not emit CONFIG_SOC_SERIES_* at all.
    # Fall back to collecting every non-structural CONFIG_SOC_<PART> (value=1).
    if not candidates and not series_list:
        for name in active:
            part = name.removeprefix("CONFIG_SOC_")
            if part == name or not part:
                continue
            if is_structural(part):
                continue
            candidates.append(part)

    # Sort shortest-first: base SoC < variant < anomaly-workaround (usually much longer)
    candidates = sorted(set(candidates), key=len)

    soc_base = candidates[0] if candidates else None
    soc_variant = None
    if soc_base:
        # Variant: starts with base + "_", no further underscores, short suffix (package codes)
        variants = [
            c for c in candidates[1:]
            if c.startswith(soc_base + "_")
            and "_" not in c[len(soc_base) + 1:]
            and len(c) - len(soc_base) <= 8
        ]
        if variants:
            soc_variant = variants[0]

    return {
        "part_numbers": part_numbers,   # tier 1
        "soc_base": soc_base,           # tier 2/3 shortest match
        "soc_variant": soc_variant,     # tier 2/3 with package suffix
        "series": series_list,
        "families": families,
        "candidates": candidates,
    }


def main():
    parser = argparse.ArgumentParser(
        description="Extract SoC part number from Zephyr ELF CONFIG_SOC_* symbols",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  uv run extract_soc.py zephyr.elf
  uv run extract_soc.py -v *.elf
""",
    )
    parser.add_argument("elf_files", nargs="+", metavar="ELF", help="ELF file(s) to inspect")
    parser.add_argument("-v", "--verbose", action="store_true", help="Print all CONFIG_SOC_* symbols")
    args = parser.parse_args()

    for elf_path in args.elf_files:
        print(f"\n{'='*60}")
        print(f"File: {elf_path}")

        symbols = get_soc_symbols(elf_path)
        if not symbols:
            print("  (no CONFIG_SOC_* ABS symbols found — not a Zephyr ELF, or wrong format)")
            continue

        info = extract_soc(symbols)

        print(f"  Family:   {', '.join(info['families']) or '—'}")
        print(f"  Series:   {', '.join(info['series']) or '—'}")

        if info["part_numbers"]:
            print(f"  Part:     {', '.join(info['part_numbers'])}  [explicit PART_NUMBER_*]")
        elif info["soc_variant"]:
            print(f"  SoC:      {info['soc_base']}  [series-matched base]")
            print(f"  Variant:  {info['soc_variant']}  [+ package/revision suffix]")
        elif info["soc_base"]:
            print(f"  SoC:      {info['soc_base']}  [series-matched, no variant]")
        else:
            print("  SoC:      (could not determine)")

        if len(info["candidates"]) > 1:
            print(f"  All candidates: {', '.join(info['candidates'])}")

        if args.verbose:
            print(f"\n  CONFIG_SOC_* symbols ({len(symbols)} total):")
            for name in sorted(symbols):
                print(f"    {symbols[name]:10}  {name}")


if __name__ == "__main__":
    main()
