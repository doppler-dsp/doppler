- **`dp_fftfreq()` and `dp_fftfreq_index()`** — the FFT bin→frequency mapping,
    in `clib_common.h` where everything can inline it, with
    `doppler.dsss.bin_to_signed` a thin wrapper over the same code. It arrived
    as an acquisition-private helper and disagreed with numpy at exactly one
    index — an even grid's Nyquist bin — so every formula ported in from numpy
    disagreed with the engine at the one bin it was most careful about. The
    fold had been restated in four call sites, three mutually inconsistent
    ways; see `docs/design/dsss-burst-receiver.md`.
