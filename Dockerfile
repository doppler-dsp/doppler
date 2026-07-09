# Multi-stage Dockerfile for doppler library
FROM ubuntu:22.04 AS builder

# jb.toml is the single source of truth for doppler's system deps (dev +
# runtime groups) — install jbx and read it from there rather than
# hand-copying a package list here that would silently drift from it.
RUN apt-get update && apt-get install -y --no-install-recommends \
    curl ca-certificates sudo bash \
    && rm -rf /var/lib/apt/lists/*
ENV PATH="/root/.local/bin:${PATH}"

WORKDIR /build

# jb.toml first so this layer caches independently of source changes.
COPY jb.toml .
# process substitution (<()) needs bash — /bin/sh here is dash.
RUN bash -c ". <(curl -sSL https://just-buildit.github.io/get-jb.sh) && \
    jbx just-bashit:install-deps -g dev -s apt"

# Copy source files
COPY CMakeLists.txt .
COPY cmake/ cmake/
COPY native/ native/
COPY vendor/ vendor/
COPY examples/ examples/

# Build the library
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release -S . && \
    cmake --build build -j"$(nproc)"

# Runtime stage — the C core is pure C99 + libm; no system packages needed.
FROM ubuntu:22.04

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
