# Multi-stage Dockerfile for doppler library
FROM ubuntu:22.04 AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    pkg-config \
    libzmq3-dev \
    libfftw3-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build

# Copy source files
COPY CMakeLists.txt .
COPY cmake/ cmake/
COPY native/ native/
COPY examples/ examples/

# Build the library
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release -S . && \
    cmake --build build -j"$(nproc)"

# Runtime stage
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y --no-install-recommends \
    libzmq5 \
    libfftw3-3 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy shared library
COPY --from=builder /build/build/libdoppler.so /usr/local/lib/

# Copy headers for development use
COPY --from=builder /build/native/inc/ /usr/local/include/doppler/

# Copy build tree so we can extract test and example binaries, then discard it
COPY --from=builder /build/build /tmp/doppler-build
RUN find /tmp/doppler-build -maxdepth 4 -name 'test_*' -type f \
        -exec install -m 755 {} /app/ \; \
    && find /tmp/doppler-build/examples -maxdepth 2 -type f -executable \
        -exec install -m 755 {} /app/ \; \
    && rm -rf /tmp/doppler-build

RUN ldconfig

# Graceful shutdown via SIGTERM
STOPSIGNAL SIGTERM

CMD ["/bin/sh"]
