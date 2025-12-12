#!/usr/bin/env bash
set -euo pipefail

# Build macOS binaries using system SDL2 packages (installed via Homebrew).

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build-mac}"

# On macOS, Homebrew installs SDL2 to /opt/homebrew (Apple Silicon) or /usr/local (Intel)
# CMake's find_package should automatically find them, but we can set the prefix path if needed
if [ -d "/opt/homebrew" ]; then
    # Apple Silicon Mac
    CMAKE_PREFIX_PATH="/opt/homebrew"
elif [ -d "/usr/local" ]; then
    # Intel Mac
    CMAKE_PREFIX_PATH="/usr/local"
else
    CMAKE_PREFIX_PATH=""
fi

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  ${CMAKE_PREFIX_PATH:+-DCMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH"}

cmake --build "$BUILD_DIR" --config Release -j"$(sysctl -n hw.ncpu)"

echo ""
echo "Build complete! Executable: $BUILD_DIR/Project Aurora Zeta"

