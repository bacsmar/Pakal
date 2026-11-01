#!/usr/bin/env bash
# Run the Linux player with the same runtime dependencies as the build container.
set -euo pipefail
cd "$(dirname "$0")/.."
: "${DISPLAY:?An X11 display (or XWayland) is required}"
# Mesa software rendering uses X11 MIT-SHM with the host X server.
# Both processes must see the same System V shared-memory segments.
args=(--rm --ipc=host --user "$(id -u):$(id -g)" -e DISPLAY
      -e ALSOFT_DRIVERS=pulse
      -v "$PWD:/workspace" -v /tmp/.X11-unix:/tmp/.X11-unix:ro
      -w "/workspace/${PAKAL_LINUX_BIN_DIR:-build-linux/bin}")
# Docker remains a software-rendering fallback; native builds use the host GPU drivers.
args+=(-e LIBGL_ALWAYS_SOFTWARE=1)
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
# Forward the engine/game diagnostic switches set by the caller.
while IFS= read -r name; do
    case "$name" in PAKAL_*|ZOMBIES_*|ZOMBIMEX_*) args+=(-e "$name");; esac
done < <(compgen -e)
module=${1:-./libContraGameModule.so}
if (( $# )); then shift; fi
exec docker run "${args[@]}" pakal-linux-build:local ./PakalPlayer "$module" "$@"
