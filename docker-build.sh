#!/bin/bash

# Pakal Docker Build Script
# Usage: ./docker-build.sh [build|shell|rebuild|clean]

COMMAND=${1:-build}
DOCKER_IMAGE="pakal-builder"
DOCKER_TAG="latest"

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
    echo "🔨 Building Pakal Engine with Docker..."
    docker build -t $DOCKER_IMAGE:$DOCKER_TAG .
    echo "✅ Docker image built. Running compilation..."
    docker run --rm -v $(pwd):/workspace -w /workspace $DOCKER_IMAGE:$DOCKER_TAG \
      bash -c "mkdir -p build && cd build && cmake -DCMAKE_BUILD_TYPE=Release .. && make -j\$(nproc)"
    echo "✅ Build complete! Output in ./build/bin"
    ;;
  shell)
    echo "🐚 Starting interactive shell in Docker container..."
    docker run --rm -it -v $(pwd):/workspace -w /workspace $DOCKER_IMAGE:$DOCKER_TAG /bin/bash
    ;;
  rebuild)
    echo "🔄 Rebuilding Docker image..."
    docker build --no-cache -t $DOCKER_IMAGE:$DOCKER_TAG .
    docker run --rm -v $(pwd):/workspace -w /workspace $DOCKER_IMAGE:$DOCKER_TAG \
      bash -c "mkdir -p build && cd build && cmake -DCMAKE_BUILD_TYPE=Release .. && make -j\$(nproc)"
    echo "✅ Rebuild complete! Output in ./build/bin"
    ;;
  clean)
    echo "🧹 Cleaning Docker containers and build artifacts..."
    docker rmi -f $DOCKER_IMAGE:$DOCKER_TAG 2>/dev/null || true
    rm -rf build/
    echo "✅ Cleaned!"
    ;;
  *)
    echo "❌ Unknown command: $COMMAND"
    echo "Usage: $0 [build|shell|rebuild|clean]"
    exit 1
    ;;
esac
