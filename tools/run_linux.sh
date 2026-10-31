#!/usr/bin/env bash
# Run the Linux player with the same runtime dependencies as the build container.
set -euo pipefail
cd "$(dirname "$0")/.."
: "${DISPLAY:?An X11 display (or XWayland) is required}"
args=(--rm --user "$(id -u):$(id -g)" -e DISPLAY
      -e ALSOFT_DRIVERS=pulse -e LIBGL_ALWAYS_SOFTWARE=1
      -v "$PWD:/workspace" -v /tmp/.X11-unix:/tmp/.X11-unix:ro
      -w "/workspace/${PAKAL_LINUX_BIN_DIR:-build-linux/bin}")
auth=${XAUTHORITY:-$HOME/.Xauthority}
if [[ -f "$auth" ]]; then
    args+=(-v "$auth:/tmp/pakal-xauth:ro" -e XAUTHORITY=/tmp/pakal-xauth)
fi
# PulseAudio/PipeWire's Pulse compatibility socket, when present.
if [[ -S "${XDG_RUNTIME_DIR:-/run/user/$(id -u)}/pulse/native" ]]; then
    args+=(-v "${XDG_RUNTIME_DIR:-/run/user/$(id -u)}/pulse/native:/tmp/pakal-pulse" -e PULSE_SERVER=unix:/tmp/pakal-pulse)
else
    args+=(-e ALSOFT_DRIVERS=null)
fi
module=${1:-./libContraGameModule.so}
if (( $# )); then shift; fi
exec docker run "${args[@]}" pakal-linux-build:local ./PakalPlayer "$module" "$@"
