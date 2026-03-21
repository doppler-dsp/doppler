# Multi-stage Dockerfile for doppler library
FROM ubuntu:22.04 AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    pkg-config \
    libzmq3-dev \
    libfftw3-dev \
    python3 \
    python3-dev \
    python3-numpy \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build

# Copy source files
COPY CMakeLists.txt .
COPY c/ c/
COPY python/ python/

# Build the library and examples
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
COPY --from=builder /build/build/c/libdoppler.so /usr/local/lib/

# Copy headers for development use
COPY --from=builder /build/c/include/doppler.h /usr/local/include/
COPY --from=builder /build/c/include/dp/ /usr/local/include/dp/

# Copy all example binaries and tests
COPY --from=builder /build/build/c/transmitter \
                    /build/build/c/receiver \
                    /build/build/c/spectrum_analyzer \
                    /build/build/c/pipeline_demo \
                    /build/build/c/fft_demo \
                    /build/build/c/simd_demo \
                    /build/build/c/test_stream \
                    /build/build/c/fft_testbench \
                    /app/

RUN ldconfig

# Graceful shutdown via SIGTERM
STOPSIGNAL SIGTERM

EXPOSE 5555

CMD ["./transmitter", "tcp://*:5555", "cf64"]
