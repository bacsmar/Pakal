#!/bin/bash

# Pakal Docker Build Script
# Usage: ./docker-build.sh [build|shell|rebuild|clean]

COMMAND=${1:-build}
DOCKER_IMAGE="pakal-engine"
DOCKER_TAG="latest"
OUTPUT_DIR="$(pwd)/bin"

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
    
    echo "✅ Build complete! Extracting binaries..."
    mkdir -p "$OUTPUT_DIR"
    
    # Create temporary container to extract files
    CONTAINER_ID=$(docker create $DOCKER_IMAGE:$DOCKER_TAG)
    docker cp "$CONTAINER_ID":/workspace/build/bin/ "$OUTPUT_DIR" 2>/dev/null || true
    docker cp "$CONTAINER_ID":/workspace/build/lib/ "$OUTPUT_DIR" 2>/dev/null || true
    docker rm "$CONTAINER_ID" > /dev/null
    
    echo "✅ Binaries extracted to $OUTPUT_DIR/"
    ls -lah "$OUTPUT_DIR"/bin/ 2>/dev/null || echo "   (binaries directory)"
    ;;
  shell)
    echo "🐚 Starting interactive shell in Docker container..."
    docker run --rm -it $DOCKER_IMAGE:$DOCKER_TAG /bin/bash
    ;;
  rebuild)
    echo "🔄 Rebuilding Docker image (no cache)..."
    docker build --no-cache -t $DOCKER_IMAGE:$DOCKER_TAG .
    
    echo "✅ Rebuild complete! Extracting binaries..."
    mkdir -p "$OUTPUT_DIR"
    
    CONTAINER_ID=$(docker create $DOCKER_IMAGE:$DOCKER_TAG)
    docker cp "$CONTAINER_ID":/workspace/build/bin/ "$OUTPUT_DIR" 2>/dev/null || true
    docker cp "$CONTAINER_ID":/workspace/build/lib/ "$OUTPUT_DIR" 2>/dev/null || true
    docker rm "$CONTAINER_ID" > /dev/null
    
    echo "✅ Binaries extracted to $OUTPUT_DIR/"
    ;;
  clean)
    echo "🧹 Cleaning Docker image and binaries..."
    docker rmi -f $DOCKER_IMAGE:$DOCKER_TAG 2>/dev/null || true
    rm -rf "$OUTPUT_DIR"
    echo "✅ Cleaned!"
    ;;
  *)
    echo "❌ Unknown command: $COMMAND"
    echo "Usage: $0 [build|shell|rebuild|clean]"
    exit 1
    ;;
esac
