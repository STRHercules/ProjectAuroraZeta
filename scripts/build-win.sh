#!/usr/bin/env bash
set -euo pipefail

# Cross-build Windows binaries using MinGW-w64 and the prebuilt SDL packages.

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build-win}"
TOOLCHAIN_FILE="${TOOLCHAIN_FILE:-$ROOT_DIR/cmake/toolchains/mingw-w64.cmake}"
WIN_SDL_ROOT="${WIN_SDL_ROOT:-$HOME/winlibs/sdl}"
export WIN_SDL_ROOT

SDL_PREFIX_PATH="${WIN_SDL_ROOT}/SDL2-2.30.9/x86_64-w64-mingw32;${WIN_SDL_ROOT}/SDL2_image-2.8.2/x86_64-w64-mingw32;${WIN_SDL_ROOT}/SDL2_ttf-2.22.0/x86_64-w64-mingw32;${WIN_SDL_ROOT}/SDL2_mixer-2.8.0/x86_64-w64-mingw32"

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" \
  -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$SDL_PREFIX_PATH" \
  -DWIN_SDL_ROOT="$WIN_SDL_ROOT"

cmake --build "$BUILD_DIR" --config Release -j"$(nproc)"
