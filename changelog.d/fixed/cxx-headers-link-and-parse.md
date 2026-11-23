- **Every installed header compiles and links from C++.** `dp_tlm_core.h`
    declared the telemetry API outside `extern "C"`, so a C++ caller linked
    mangled names the library does not define (also `dp_lgamma`). The async DSSS receiver, its pool,
    the despreader, the DSSS and M-PSK receivers, the DLL, Costas and
    `doppler.h` now parse as C++: their C-only per-sample inlines are not
    declared there, as `lo_step` is not. `make cxx-headers-check` reads
    linkage too ([#3108](https://github.com/doppler-dsp/doppler/issues/3108)).
