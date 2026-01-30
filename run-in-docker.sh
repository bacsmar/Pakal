#!/bin/bash
# Run Pakal application inside Docker with virtual display

BUILD_DIR="$(pwd)/docker-build"

docker run --rm \
  -e DISPLAY=:99 \
  -e LIBGL_ALWAYS_SOFTWARE=1 \
  -e GALLIUM_DRIVER=llvmpipe \
  -v "$BUILD_DIR:/workspace/build" \
  pakal-engine bash -c "
    Xvfb :99 -screen 0 1024x768x24 &
    sleep 1
    echo '===== STARTING APPLICATION (10s TIMEOUT) ====='
    echo 'Using software OpenGL rendering (Mesa llvmpipe)'
    timeout 10 /workspace/build/bin/PakalBasicExample
    EXIT_CODE=\$?
    echo ''
    echo '===== APPLICATION EXIT CODE: '\$EXIT_CODE' ====='
"
