- **`doppler::stream-static` links**: its exported target now declares
    the core static library, so CMake puts `libdoppler.a` after
    `libdoppler_stream.a`. Before this, a static consumer of the stream
    component failed with undefined `dp_interrupt*` / `dp_tlm_read`, whatever
    order it linked the two in. The shared form and pkg-config were already
    right.
