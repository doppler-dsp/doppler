- **Every installed header compiles and links from C++.** `dp_tlm_core.h`
    and `dp_lgamma` were declared outside `extern "C"`, so a C++ caller
    linked mangled names the library does not define. The async DSSS
    receiver and pool, the despreader, the DSSS and M-PSK receivers, DLL,
    Costas and `doppler.h` now parse as C++. Their per-sample inlines
    (`costas_wipeoff`/`_update`, `dll_update`, the M-PSK TED steps) never
    compiled from C++, so hiding them breaks no caller: one uses the block
    function (`dp_costas_steps`, `dp_mpsk_receiver_steps`, `dll_steer`).
    `make cxx-headers-check` reads linkage too
    ([#3108](https://github.com/doppler-dsp/doppler/issues/3108)).
