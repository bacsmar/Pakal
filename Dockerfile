# Build stage - using Ubuntu 24.04 for newer GLIBC (required by bgfx's genie tool)
FROM ubuntu:24.04 as builder

WORKDIR /workspace

# Install dependencies
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
    && rm -rf /var/lib/apt/lists/*

# === STAGE 1: Compile bgfx (cached, won't rebuild unless external/ changes) ===
# Copy only external dependencies (doesn't change often)
COPY external/ /workspace/external/

# Initialize git for submodules
RUN git init && git config user.email "docker@build" && git config user.name "Docker Build"

# Build bgfx with its own build system
WORKDIR /workspace/external/bgfx
RUN make linux-gcc-release64 -j$(nproc)

# === STAGE 2: Compile Pakal (uses cached bgfx) ===
WORKDIR /workspace

# Now copy the full source (only Pakal sources, bgfx already built)
COPY source/ /workspace/source/
COPY CMakeLists.txt /workspace/
COPY examples/ /workspace/examples/

# Build Pakal
RUN mkdir -p build && cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    make -j$(nproc)

# Runtime stage
FROM ubuntu:24.04

WORKDIR /workspace

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libsfml-dev \
    libx11-6 \
    libgl1 \
    liblua5.3-0 \
    && rm -rf /var/lib/apt/lists/*

# Copy built binaries and libraries from builder
COPY --from=builder /workspace/build/bin /workspace/bin
COPY --from=builder /workspace/build/lib /workspace/lib
COPY . .

# Make binaries easily accessible
ENV PATH="/workspace/bin:${PATH}"

# Default command
CMD ["/bin/bash"]
CMD ["/bin/bash"]
