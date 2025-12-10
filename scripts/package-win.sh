#!/usr/bin/env bash
set -euo pipefail

# Package the Windows build into a distributable zip with required SDL DLLs and data.

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build-win}"
WIN_SDL_ROOT="${WIN_SDL_ROOT:-$HOME/winlibs/sdl}"
DIST_DIR="$ROOT_DIR/dist/windows"
STAGE_DIR="$DIST_DIR/zeta-win64"
ZIP_PATH="$DIST_DIR/zeta-win64.zip"
EXE_NAME="Project Aurora Zeta.exe"

mkdir -p "$DIST_DIR"
rm -rf "$STAGE_DIR"
mkdir -p "$STAGE_DIR"

# Prefer single-config MinGW output; fall back to multi-config layout if present.
EXE_PATH="$BUILD_DIR/$EXE_NAME"
if [[ ! -f "$EXE_PATH" && -f "$BUILD_DIR/Release/$EXE_NAME" ]]; then
  EXE_PATH="$BUILD_DIR/Release/$EXE_NAME"
fi

if [[ ! -f "$EXE_PATH" ]]; then
  echo "$EXE_NAME not found in $BUILD_DIR. Build first with scripts/build-win.sh" >&2
  exit 1
fi

cp "$EXE_PATH" "$STAGE_DIR/"

for dir in assets data; do
  cp -r "$ROOT_DIR/$dir" "$STAGE_DIR/"
done

# Ensure saves folder exists for the player.
mkdir -p "$STAGE_DIR/saves"

# Pull in all DLLs from the SDL runtime zips (covers image/font dependencies).
for pkg in SDL2-2.30.9 SDL2_image-2.8.2 SDL2_ttf-2.22.0; do
  BIN_DIR="$WIN_SDL_ROOT/$pkg/x86_64-w64-mingw32/bin"
  if [[ -d "$BIN_DIR" ]]; then
    cp "$BIN_DIR"/*.dll "$STAGE_DIR/" 2>/dev/null || true
  else
    echo "Warning: missing $BIN_DIR" >&2
  fi
done

# Bundle MinGW runtime DLLs so users don't need the toolchain installed.
MINGW_TRIPLE="x86_64-w64-mingw32"
for dll in libstdc++-6.dll libgcc_s_seh-1.dll libwinpthread-1.dll; do
  dll_path="$("${MINGW_TRIPLE}-g++-posix" -print-file-name="$dll" 2>/dev/null || true)"
  if [[ -n "$dll_path" && -f "$dll_path" ]]; then
    cp "$dll_path" "$STAGE_DIR/"
  else
    echo "Warning: could not locate $dll; it may already be on PATH for target users" >&2
  fi
done

# Helper launcher for convenience on Windows.
cat > "$STAGE_DIR/start.bat" <<'BAT'
@echo off
cd /d %~dp0
start "" "Project Aurora Zeta.exe"
BAT

rm -f "$ZIP_PATH"
(cd "$DIST_DIR" && zip -r "$(basename "$ZIP_PATH")" "$(basename "$STAGE_DIR")")

echo "Packaged to $ZIP_PATH"
