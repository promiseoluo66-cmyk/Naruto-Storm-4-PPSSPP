#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PPSSPP_REV="fc9ca4d1f78a5d60ae03f3311c6b8ab49273f6a8"
CACHE_DIR="\${PPSSPP_SRC:-$HOME/.cache/ppsspp-libkirk}"
BUILD_DIR="\${BUILD_DIR:-$SCRIPT_DIR/build}"
PREPARED_DIR="$BUILD_DIR/ppsspp-libkirk"

for command_name in git cmake clang python3; do
    if ! command -v "$command_name" >/dev/null 2>&1; then
        echo "Missing build tool: $command_name"
        echo "Install the Termux build tools with:"
        echo "  pkg install git cmake clang make python -y"
        exit 2
    fi
done

if [ ! -d "$CACHE_DIR/.git" ]; then
    mkdir -p "$(dirname "$CACHE_DIR")"
    git clone --filter=blob:none --no-checkout --sparse \
        https://github.com/hrydgard/ppsspp.git "$CACHE_DIR"
    git -C "$CACHE_DIR" sparse-checkout set ext/libkirk
fi

git -C "$CACHE_DIR" fetch --depth 1 origin "$PPSSPP_REV"
git -C "$CACHE_DIR" checkout --detach FETCH_HEAD

python3 "$SCRIPT_DIR/prepare_build.py" \
    --ppsspp-src "$CACHE_DIR" \
    --output-dir "$PREPARED_DIR"

cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" \
    -DPPSSPP_LIBKIRK_DIR="$PREPARED_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++

cmake --build "$BUILD_DIR" --parallel 2
ctest --test-dir "$BUILD_DIR" --output-on-failure

echo
echo "Build ready: $BUILD_DIR/impact_pgd_decryptor"
echo "Usage:"
echo "  $BUILD_DIR/impact_pgd_decryptor /path/to/0005.BIN /path/to/0005.decrypted.bin"
