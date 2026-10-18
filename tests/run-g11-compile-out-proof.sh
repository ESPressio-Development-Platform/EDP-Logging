#!/usr/bin/env bash
set -Eeuo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$(mktemp -d)"
trap 'rm -rf "$BUILD"' EXIT
SOURCE="$ROOT/tests/G11CompileOutProof.cpp"

for CXX in g++ clang++; do
    for SITE in 0 1; do
        BINARY="$BUILD/${CXX}-${SITE}"
        "$CXX" -std=c++20 -Os -Wall -Wextra -Wpedantic -Werror \
            -ffunction-sections -fdata-sections -Wl,--gc-sections \
            -DEDP_LOGGING_G11_ENABLE_SITE="$SITE" \
            -I"$ROOT/src" "$SOURCE" -o "$BINARY"
        "$BINARY"
        echo "CODE_SIZE|compiler=$CXX|site=$SITE"
        size "$BINARY"
    done
    if nm -C "$BUILD/${CXX}-0" | grep -Fq 'G11Proof::CaptureClock()'; then
        echo "FAILED|compiled-out binary contains reachable Clock capture: $CXX"
        exit 1
    fi
    if ! nm -C "$BUILD/${CXX}-1" | grep -Fq 'G11Proof::CaptureClock()'; then
        echo "FAILED|enabled binary lacks Clock capture: $CXX"
        exit 1
    fi
    echo "EDP_G11_SITE_SYMBOL_ELIMINATION|compiler=$CXX|PASS"
done

XTENSA="${EDP_LOGGING_G11_XTENSA_GXX:-}"
if [[ -z "$XTENSA" ]] && command -v xtensa-esp32-elf-g++ >/dev/null 2>&1; then
    XTENSA="$(command -v xtensa-esp32-elf-g++)"
fi
if [[ -n "$XTENSA" ]]; then
    for SITE in 0 1; do
        OUTPUT="$BUILD/xtensa-${SITE}.o"
        "$XTENSA" -std=c++20 -Os -ffunction-sections -fdata-sections \
            -DEDP_LOGGING_G11_ENABLE_SITE="$SITE" -I"$ROOT/src" \
            -c "$SOURCE" -o "$OUTPUT"
        echo "XTENSA_OBJECT|site=$SITE"
        "${XTENSA%-g++}-size" "$OUTPUT"
    done
else
    echo "XTENSA_OBJECT|SKIP|toolchain not on PATH and not supplied"
fi
echo "EDP_LOGGING_G11_COMPILE_OUT_SITE_PROOF|PASS"
