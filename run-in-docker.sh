#!/bin/bash
# Run Pakal application inside Docker with VNC display

BUILD_DIR="$(pwd)/docker-build"

find_free_port() {
  python3 - <<'PY'
import socket
import sys

for port in range(5900, 6000):
  with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
      s.bind(("0.0.0.0", port))
      print(port)
      sys.exit(0)
    except OSError:
      continue

print("0")
sys.exit(1)
PY
}

VNC_PORT=$(find_free_port)
if [[ -z "$VNC_PORT" || "$VNC_PORT" == "0" ]]; then
  echo "❌ No hay puertos VNC libres en el rango 5900-5999"
  exit 1
fi

echo "🎮 Iniciando ContraGame en Docker con VNC..."
echo "📺 Conecta con: vncviewer localhost:${VNC_PORT}"
echo "🔧 O abre: http://localhost:6080"
echo "⏱️  Timeout: 60 segundos"
echo ""

docker run --rm \
  -e DISPLAY=:99 \
  -e LIBGL_ALWAYS_SOFTWARE=1 \
  -e GALLIUM_DRIVER=llvmpipe \
  -v "$BUILD_DIR:/workspace/build" \
  -p ${VNC_PORT}:5900 \
  pakal-engine bash -c "
    # Iniciar servidor X virtual
    Xvfb :99 -screen 0 1280x720x24 -dpi 96 -ac +extension GLX -noreset &
    XVFB_PID=\$!
    sleep 1

    # Iniciar window manager ligero y fondo visible
    openbox-session &
    WM_PID=\$!
    xsetroot -solid "#202020" || true
    xclock -geometry 200x200+10+10 &
    CLOCK_PID=\$!
    
    # Iniciar VNC server (forzar refresco continuo)
    x11vnc -display :99 -forever -nopw -rfbport 5900 -noxdamage -nowf -ncache 10 -shared -noxrandr -geometry 1280x720 &
    VNC_PID=\$!
    sleep 2
    
    echo '===== STARTING APPLICATION (60s TIMEOUT) ====='
    echo 'Using software OpenGL rendering (Mesa llvmpipe)'
    echo 'VNC Server running on port 5900 (host ${VNC_PORT})'
    cd /workspace/build/bin
    timeout 60 ./ContraGame
    EXIT_CODE=\$?
    
    kill \$CLOCK_PID \$WM_PID \$VNC_PID \$XVFB_PID 2>/dev/null || true
    
    echo ''
    echo '===== APPLICATION EXIT CODE: '\$EXIT_CODE' ====='
"
