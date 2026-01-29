# Build stage
FROM gcc:11 as builder

WORKDIR /workspace

# Install dependencies
RUN apt-get update && apt-get install -y \
    cmake \
    git \
    pkg-config \
    libsfml-dev \
    libx11-dev \
    libgl1-mesa-dev \
    liblua5.3-dev \
    && rm -rf /var/lib/apt/lists/*

# Copy project
COPY . .

# Initialize submodules
RUN git init && git config user.email "docker@build" && git config user.name "Docker Build" && \
    git add . && git commit -m "initial" --no-gpg-sign 2>/dev/null || true && \
    git submodule update --init --recursive 2>/dev/null || true

# Build
RUN mkdir -p build && cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    make -j$(nproc)

# Runtime stage (optional - for smaller final image)
FROM gcc:11

WORKDIR /workspace

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libsfml-2.5.1 \
    libx11-6 \
    libgl1-mesa-glx \
    liblua5.3-0 \
    && rm -rf /var/lib/apt/lists/*

# Copy built binaries from builder
COPY --from=builder /workspace/build/bin /workspace/bin
COPY --from=builder /workspace/build/lib /workspace/lib
COPY . .

# Default command
CMD ["/bin/bash"]
