- **A program with its own cJSON or nats.c now links doppler statically**
    ([#1565](https://github.com/doppler-dsp/doppler/issues/1565)): the
    vendored cJSON, pffft, pocketfft and nats.c inside `libdoppler.a` /
    `libdoppler_stream.a` are respelled `dp__v_*` at build time, so they no
    longer collide with, or silently resolve to, a consumer's copy. Linux
    static archives; macOS, Windows and the shared libraries follow in
    [#1577](https://github.com/doppler-dsp/doppler/issues/1577).
