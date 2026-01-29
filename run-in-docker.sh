#!/bin/bash
# Run Pakal application inside Docker with virtual display

BUILD_DIR="$(pwd)/docker-build"

docker run --rm \
  -e DISPLAY=:99 \
  -v "$BUILD_DIR:/workspace/build" \
  pakal-engine bash -c "
    Xvfb :99 -screen 0 1024x768x24 &
    sleep 1
    /workspace/build/bin/PakalBasicExample
"
