- **`deploy/docker/stream_tool.c` builds with the library**, so a change
    that breaks it fails `make build` and CI. Its only recipe was the
    Dockerfile, which nothing builds, and it went ten weeks without
    compiling. It is a `stream_tool` target wherever the stream layer
    builds, except Windows, linked against the same `libdoppler_stream.a`
    archive the image ships (#2100).
