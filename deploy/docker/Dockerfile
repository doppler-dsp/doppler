# doppler streaming producer/consumer image for the k8s pipeline.
#
#   # build into minikube's docker daemon (no registry needed):
#   eval $(minikube docker-env)
#   docker build -t doppler-stream:dev -f deploy/docker/Dockerfile .
#
# Builds the static-embedded stream_tool (deploy/docker/stream_tool.c) against
# the doppler core (libdoppler.a, lo object) + the stream tier
# (libdoppler_stream.a, with vendored nats.c folded in).  The whole build is
# pure C, so the runtime image needs only libc.

FROM debian:bookworm AS builder
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake ca-certificates \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
RUN cmake -B build -DBUILD_PYTHON=OFF -DCMAKE_BUILD_TYPE=Release \
 && cmake --build build --target doppler_lib_static doppler_stream_static \
        -j"$(nproc)" \
 && gcc -c deploy/docker/stream_tool.c -Inative/inc -O2 -o /tmp/tool.o \
 && gcc /tmp/tool.o build/libdoppler_stream.a build/libdoppler.a \
        -lm -lpthread \
        -o /usr/local/bin/stream_tool

FROM debian:bookworm-slim
COPY --from=builder /usr/local/bin/stream_tool /usr/local/bin/stream_tool
ENTRYPOINT ["stream_tool"]
