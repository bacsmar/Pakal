#!/usr/bin/env bash
# Reproducible Linux build. Outputs stay in build-linux; no private assets enter the image.
set -euo pipefail
cd "$(dirname "$0")/.."
image=pakal-linux-build:local
jobs=${PAKAL_BUILD_JOBS:-4}
docker build -f Dockerfile.linux -t "$image" .
docker run --rm --user "$(id -u):$(id -g)" \
    -e PAKAL_BUILD_JOBS="$jobs" -v "$PWD:/workspace" "$image" bash -euc '
    cd external/bgfx
    ../bx/tools/bin/linux/genie --with-tools --gcc=linux-gcc gmake
    make -C .build/projects/gmake-linux-gcc config=release64 -j "$PAKAL_BUILD_JOBS" bgfx bx bimg bimg_decode shaderc
    cd /workspace
    cmake --preset linux-release
    cmake --build --preset linux-release -j "$PAKAL_BUILD_JOBS"
    ln -sfn ../lib/libContraGameModule.so build-linux/bin/libContraGameModule.so
    ctest --test-dir build-linux --output-on-failure
'
