#!/bin/bash
# Native runner equivalent of upstream tools/p2p-motor/build-arm.sh.
set -euo pipefail
work=$1
commit=02938d75cbc5823dbf47c9006ca6d51a4a08aaf2
mkdir -p "$work"
if [ ! -d "$work/nuvio-engine/.git" ]; then
  git clone -q https://github.com/NuvioMedia/nuvio-engine "$work/nuvio-engine"
fi
git -C "$work/nuvio-engine" checkout -q "$commit"
test "$(git -C "$work/nuvio-engine" rev-parse HEAD)" = "$commit"
cat > "$work/arm.cmake" <<'CMAKE'
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR armv7)
set(CMAKE_C_COMPILER arm-webos-linux-gnueabi-gcc)
set(CMAKE_CXX_COMPILER arm-webos-linux-gnueabi-g++)
set(CMAKE_SYSROOT $ENV{NUVIO_SYSROOT})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
set(OPENSSL_USE_STATIC_LIBS TRUE)
CMAKE
/usr/bin/cmake -S "$work/nuvio-engine" -B "$work/build-arm" \
  -DCMAKE_TOOLCHAIN_FILE="$work/arm.cmake" -DCMAKE_BUILD_TYPE=MinSizeRel \
  -DNUVIO_ENGINE_ENABLE_LIBTORRENT=ON -DNUVIO_ENGINE_BUILD_TESTS=OFF \
  -DCMAKE_C_FLAGS='-ffunction-sections -fdata-sections' \
  -DCMAKE_CXX_FLAGS='-ffunction-sections -fdata-sections -Wno-psabi'
/usr/bin/cmake --build "$work/build-arm" -j"${NUVIO_CI_JOBS:-2}"
