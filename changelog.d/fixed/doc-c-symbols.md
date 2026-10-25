- **A C function or type named in the docs is one the tree defines.**
    `make doc-symbols-check` checks every `dp_*` name in a page's code spans
    and fences against the C sources. It found eight stale names, including a
    `dp_cf64_t` in a `skip=` fence the C snippet gate never compiles. Pages
    that name a symbol because it is gone sit on a list that may only shrink
    ([#2580](https://github.com/doppler-dsp/doppler/issues/2580)).

- **Facts the docs repeated wrongly across pages are corrected.** The fixes are listed below.

    - Results are fresh owning arrays, not views.
    - The CIC cap is 2048 and its input bound is ±2.0.
    - The CIC takes cf32, not `I16U64`.
    - The cf32 FFT runs single-precision PFFFT at the lengths it supports.
    - The MPSK handover, `nda_tap` and AVX-512 AWGN paths are gone.
    - Node sync lives in `viterbi`.
    - `benchmark.yml` is gone.
    - Most `_ext` fragments are generated.
    - Telemetry is a jm module.
    - The framed `sync`/`crc` keys are retired.
    - The Writer writes BLUE `SF` and SigMF, with `t0`.

    The ±2.0 bound and the writer's `t0` are fixed in the headers and manifests, so the docstrings carry them too ([#2580](https://github.com/doppler-dsp/doppler/issues/2580)).
