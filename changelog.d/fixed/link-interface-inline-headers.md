- **A program that calls doppler's inline C API links against the shared
    library.** `pkg-config --libs doppler` and `doppler::doppler` handed out
    `-ldoppler` alone, but every `step()`, `dp_thread.h` and the ring buffer
    are inline, so their libm and pthread calls land in *your* object: an
    inline `agc_step()` failed with `DSO missing from command line` on every
    distro, `dp_thread.h` wherever glibc < 2.34. Both are now in the link
    interface ([#1450](https://github.com/doppler-dsp/doppler/issues/1450)).
