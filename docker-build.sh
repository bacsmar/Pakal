#!/bin/bash

# Pakal Docker Build Script
# Usage: ./docker-build.sh [build|shell|rebuild|clean]

COMMAND=${1:-build}
DOCKER_IMAGE="pakal-engine"
DOCKER_TAG="latest"
BUILD_DIR="$(pwd)/docker-build"
SOURCE_DIR="$(pwd)/source"
EXAMPLES_DIR="$(pwd)/examples"

# Try docker compose (new) first, fallback to docker-compose (old)
if command -v docker &> /dev/null; then
    if docker compose version &> /dev/null 2>&1; then
        COMPOSE_CMD="docker compose"
    elif docker-compose --version &> /dev/null 2>&1; then
        COMPOSE_CMD="docker-compose"
    else
        COMPOSE_CMD=""
    fi
else
    echo "❌ Docker is not installed. Please install Docker first:"
    echo "   sudo apt install docker.io docker-compose"
    exit 1
fi

case $COMMAND in
  build)
    echo "🔨 Building Pakal Engine with Docker (using host volume mount)..."
    mkdir -p "$BUILD_DIR"
    
    # Build Docker image first (once)
    echo "📦 Building Docker image..."
    docker build -t $DOCKER_IMAGE:$DOCKER_TAG . || exit 1
    
    # Compile with mounted volumes (much faster for rebuilds)
    echo "🔧 Compiling Pakal..."
    docker run --rm \
      -v "$BUILD_DIR:/workspace/build" \
      $DOCKER_IMAGE:$DOCKER_TAG \
      bash -c "cd /workspace/build && cmake -DCMAKE_BUILD_TYPE=Release .. && make -j\$(nproc)"
    
    echo "✅ Build complete! Binaries in $BUILD_DIR/bin/"
    ls -lah "$BUILD_DIR"/bin/ 2>/dev/null || echo "   (checking build directory)"
    ;;
    
  shell)
    echo "🐚 Starting interactive shell in Docker container..."
    mkdir -p "$BUILD_DIR"
    docker run --rm -it \
      -v "$BUILD_DIR:/workspace/build" \
      $DOCKER_IMAGE:$DOCKER_TAG /bin/bash
    ;;
    
  rebuild)
    echo "🔄 Clean rebuild (removing $BUILD_DIR)..."
    rm -rf "$BUILD_DIR"
    mkdir -p "$BUILD_DIR"
    
    docker run --rm \
      -v "$BUILD_DIR:/workspace/build" \
      $DOCKER_IMAGE:$DOCKER_TAG \
      bash -c "cd /workspace/build && cmake -DCMAKE_BUILD_TYPE=Release .. && make -j\$(nproc)"
    
    echo "✅ Rebuild complete!"
    ;;
    
  clean)
    echo "🧹 Cleaning build artifacts..."
    rm -rf "$BUILD_DIR"
    echo "✅ Cleaned!"
    ;;
    
  run)
    echo "🚀 Running PakalBasicExample in Docker..."
    docker run --rm -e DISPLAY=:99 \
      -v "$BUILD_DIR:/workspace/build" \
      $DOCKER_IMAGE:$DOCKER_TAG \
      bash -c "Xvfb :99 -screen 0 1024x768x24 & sleep 1 && /workspace/build/bin/PakalBasicExample"
    ;;
    
  *)
    echo "❌ Unknown command: $COMMAND"
    echo ""
    echo "Usage: $0 [build|shell|rebuild|clean|run]"
    echo ""
    echo "Commands:"
    echo "  build   - Build (reuses .o files, very fast)"
    echo "  shell   - Open bash in container"
    echo "  rebuild - Full clean rebuild"
    echo "  clean   - Remove build artifacts"
    echo "  run     - Run PakalBasicExample"
    exit 1
    ;;
esac
