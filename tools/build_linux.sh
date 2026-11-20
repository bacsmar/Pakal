#!/usr/bin/env bash
# Linux build inside the Dockerfile.linux image (what CI runs). bgfx is built from the submodules by CMake.
# Outputs stay in build-linux; nothing is installed on the host.
set -euo pipefail
cd "$(dirname "$0")/.."
image=pakal-linux-build:local
jobs=${PAKAL_BUILD_JOBS:-4}
docker build -f Dockerfile.linux -t "$image" .
docker run --rm --user "$(id -u):$(id -g)" \
    -e PAKAL_BUILD_JOBS="$jobs" -v "$PWD:/workspace" "$image" bash -euc '
    cd /workspace
    cmake --preset linux-release
    cmake --build --preset linux-release -j "$PAKAL_BUILD_JOBS"
    ln -sfn ../lib/libContraGameModule.so build-linux/bin/libContraGameModule.so
    ctest --test-dir build-linux -E Mcp --output-on-failure
'
