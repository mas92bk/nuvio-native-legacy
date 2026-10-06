#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
exe=$(mktemp /tmp/nuvio-uiarabic.XXXXXX)
trap 'rm -f "$exe"' EXIT
flags=""
if [ "${SANITIZE:-0}" = 1 ]; then flags="-fsanitize=address,undefined -fno-omit-frame-pointer"; fi
${CC:-cc} -std=c11 -O1 -g -Wall -Wextra $flags -DNV_ASS_LIBASS \
  $(pkg-config --cflags sdl2 harfbuzz freetype2 fribidi) \
  tests/uiarabic.c src/uiarabic.c -o "$exe" \
  $(pkg-config --libs sdl2 harfbuzz freetype2 fribidi)
"$exe" "${UI_AR_PREVIEW:-}"
