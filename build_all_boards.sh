#!/usr/bin/env bash
# Build samples/hello_world for every board (excluding _ns variants) and
# collect ELFs in board-builds/<board-with-slashes-replaced-by-dashes>.elf

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUTPUT_DIR="${SCRIPT_DIR}/board-builds"
BUILD_DIR="${SCRIPT_DIR}/_build_tmp"

mkdir -p "$OUTPUT_DIR"

boards=$(python3 "${SCRIPT_DIR}/list_boards.py" 2>/dev/null)

total=$(echo "$boards" | wc -l)
count=0
failed=()

while IFS= read -r board; do
    count=$((count + 1))
    elf_name="${board//\//-}.elf"
    dest="${OUTPUT_DIR}/${elf_name}"

    echo "[${count}/${total}] ${board}"

    if west build -p always -b "$board" \
            --build-dir "$BUILD_DIR" \
            "${SCRIPT_DIR}/samples/hello_world" \
            > "${BUILD_DIR}.log" 2>&1; then
        cp "${BUILD_DIR}/zephyr/zephyr.elf" "$dest"
    else
        echo "  FAILED (see ${BUILD_DIR}.log)"
        failed+=("$board")
    fi
done <<< "$boards"

echo ""
echo "Done: $((count - ${#failed[@]}))/${total} succeeded, ${#failed[@]} failed."

if [[ ${#failed[@]} -gt 0 ]]; then
    echo "Failed boards:"
    printf '  %s\n' "${failed[@]}"
fi
