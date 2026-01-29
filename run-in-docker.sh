#!/bin/bash
# Run Pakal application inside Docker with virtual display

docker run --rm -e DISPLAY=:99 pakal-engine bash -c "
    Xvfb :99 -screen 0 1024x768x24 &
    sleep 1
    PakalBasicExample
"
