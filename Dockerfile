# Single stage Dockerfile for development with volume mounts
FROM ubuntu:24.04

WORKDIR /workspace

# Install all dependencies (build + runtime)
RUN apt-get update && apt-get install -y \
    build-essential \
    g++ \
    cmake \
    git \
    pkg-config \
    libsfml-dev \
    libx11-dev \
    libgl1-mesa-dev \
    liblua5.3-dev \
    xvfb \
    && rm -rf /var/lib/apt/lists/*

# Copy external dependencies (bgfx, etc)
COPY external/ /workspace/external/

# Initialize git
RUN git init && git config user.email "docker@build" && git config user.name "Docker Build"

# Build bgfx once
WORKDIR /workspace/external/bgfx
RUN make linux-gcc-release64 -j$(nproc)

# Return to workspace
WORKDIR /workspace

# Copy source (will be mounted from host during docker run)
COPY source/ /workspace/source/
COPY CMakeLists.txt /workspace/
COPY examples/ /workspace/examples/
COPY .dockerignore /workspace/

# Make binaries accessible
ENV PATH="/workspace/build/bin:${PATH}"

# Default command
CMD ["/bin/bash"]

CMD ["/bin/bash"]
