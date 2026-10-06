#!/bin/bash
# Reproducible x86_64 Linux remote build. No personal credentials required.
set -euo pipefail
cd "$(dirname "$0")/.."
root=$PWD
build=${NUVIO_CI_BUILD:-/tmp/nuvio-webos-ci}
# The SDK supplies its own host Python, without runner-installed modules.
# Capture the requested host interpreter before prepending the SDK to PATH.
python=${NUVIO_CI_PYTHON:-$(command -v python3)}
mkdir -p "$build" "$root/dist"
fetch_checked() {
  local url=$1 path=$2 sha=$3
  if [ ! -f "$path" ]; then curl --retry 3 -fL "$url" -o "$path"; fi
  printf '%s  %s\n' "$sha" "$path" | sha256sum -c -
}
fetch_checked https://github.com/openlgtv/buildroot-nc4/releases/download/webos-a38c582/arm-webos-linux-gnueabi_sdk-buildroot-x86_64.tar.gz \
  "$build/sdk.tar.gz" 04ad3311b48b4557a7002aef56ae2e167478e8e129f37daac04649bddf813616
fetch_checked https://github.com/iqui27/nuvio-native-legacy/releases/download/v2.0.0/space.nuvio.native.legacy_2.0.0_arm.ipk \
  "$build/original.ipk" c5e1d8d283fd06f914ea9e0a64bc0cafef06f119aa0693b1c8c8a6f921d78fcd
sdk=${NUVIO_CI_SDK:-$build/sdk/arm-webos-linux-gnueabi_sdk-buildroot}
if [ ! -x "$sdk/bin/arm-webos-linux-gnueabi-gcc" ]; then
  mkdir -p "$build/sdk"
  tar --no-same-owner -xzf "$build/sdk.tar.gz" -C "$build/sdk"
  "$sdk/relocate-sdk.sh"
fi
export PATH="$sdk/bin:$PATH"
export NUVIO_SYSROOT="$sdk/arm-webos-linux-gnueabi/sysroot"
export NUVIO_ASS_ROOT=${NUVIO_ASS_ROOT:-$build/ass}
export NUVIO_ASS_BUILD=${NUVIO_ASS_BUILD:-$build/ass-build}
export CC=arm-webos-linux-gnueabi-gcc TAR_OPTIONS=--no-same-owner
if [ ! -f "$NUVIO_ASS_ROOT/lib/libass.a" ]; then bash tools/build-ass-arm.sh; fi
p2p="$build/p2p"
bash tools/ci-p2p-arm.sh "$p2p"
stage="$build/stage"
rm -rf "$stage"
mkdir -p "$stage"
ar p "$build/original.ipk" data.tar.gz | tar --no-same-owner -xz -C "$stage"
app="$stage/usr/palm/applications/space.nuvio.native.legacy"
"$python" tools/ci-public-config.py "$app/nuvio-proto" "$build/config.h"
trap 'rm -f "$build/config.h"' EXIT
"$CC" src/*.c -o "$app/nuvio-proto" -O2 -DNV_WEBOS -DNV_ASS_LIBASS -DNV_P2P_MOTOR \
  -I"$p2p/nuvio-engine/include" \
  -include "$build/config.h" -I"$NUVIO_ASS_ROOT/include" \
  -I"$NUVIO_SYSROOT/usr/include" -I"$NUVIO_SYSROOT/usr/include/SDL2" \
  "$p2p/build-arm/libnuvio_engine.a" \
  "$p2p/build-arm/_deps/nuvio_libtorrent-build/libtorrent-rasterbar.a" \
  "$NUVIO_SYSROOT/usr/lib/libssl.a" "$NUVIO_SYSROOT/usr/lib/libcrypto.a" \
  -static-libgcc -Wl,-Bstatic -lstdc++ -latomic -Wl,-Bdynamic -lrt -Wl,--gc-sections \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lGLESv2 -lEGL -ldl -lpthread -lz -lm \
  -L"$NUVIO_ASS_ROOT/lib" -Wl,--start-group -lass -lharfbuzz -lfribidi -lfreetype -Wl,--end-group
chmod 755 "$app/nuvio-proto"
cp deploy/app/fonts/NotoNaskhArabic-* "$app/fonts/"
"$python" - "$app/appinfo.json" <<'PY'
import json, pathlib, sys
p=pathlib.Path(sys.argv[1]); info=json.loads(p.read_text())
# Same ID preserves existing webOS permissions and application data paths.
# Numeric version permits Homebrew Channel to offer this test as an update.
info.update(version="2.0.1",title="Nuvio Legacy Arabic Test")
p.write_text(json.dumps(info,indent=2)+"\n")
PY
# Stage is sourced only from the pinned public package, never a user's app
# directory. Also refuse known user-state filenames before packaging.
"$python" - "$app" <<'PY'
import pathlib,sys,fnmatch
names='trakt.txt addons.txt tmdb.txt mdblist.txt ajustes.txt progresso.txt nuvem.txt sessao.txt perfil.txt cliente.txt listas.txt guia-fav.txt debrid.txt fanart.txt p2p.txt collections.json catalogo-rede.bin* stalker-p*.txt xtream-p*.txt listas-p*.txt trakt-p*.txt trakt-fluxo*.txt simkl*.txt conta-*.txt* discord-p*.txt*'.split()
for p in pathlib.Path(sys.argv[1]).rglob('*'):
 if p.is_file() and any(fnmatch.fnmatch(p.name,n) for n in names):
  raise SystemExit('Refusing user-state file: '+p.name)
PY
if [ ! -x "$build/cli/node_modules/.bin/ares-package" ]; then
  npm install --prefix "$build/cli" --no-audit --no-fund @webos-tools/cli@3.2.6
fi
"$build/cli/node_modules/.bin/ares-package" "$app" -o "$root/dist"
ipk="$root/dist/space.nuvio.native.legacy_2.0.1_arm.ipk"
test -s "$ipk"
"$python" - "$app/nuvio-proto" <<'PY'
from elftools.elf.elffile import ELFFile
import sys
with open(sys.argv[1],'rb') as f:
 e=ELFFile(f); assert e['e_machine']=='EM_ARM'
 needed=[t.needed for t in e.get_section_by_name('.dynamic').iter_tags() if t.entry.d_tag=='DT_NEEDED']
 assert not any(x in n.lower() for n in needed for x in ('libass','libharfbuzz','libfribidi','libfreetype','libtorrent','libstdc++','libatomic','libssl','libcrypto')), needed
 assert any(s.name=='nuvio_engine_create' for s in e.get_section_by_name('.symtab').iter_symbols()), 'Missing native P2P engine'
 print('Verified ARM executable; subtitle libraries are linked statically')
PY
cp tests/fixtures/arabic-mixed.srt "$root/dist/arabic-mixed.srt"
{
  printf 'Candidate: 2.0.0-arabic.1 (webOS package version 2.0.1)\n'
  printf 'Source: %s\n' "$(git rev-parse HEAD)"
  printf 'Baseline: upstream v2.0.0 / 3018d194\n'
  printf 'TV validation: pending LG C3 test\n'
} > "$root/dist/build-info.txt"
(cd "$root/dist" && sha256sum ./*.ipk arabic-mixed.srt build-info.txt > SHA256SUMS)
