- **A C function or type named in the docs is one the tree defines.**
    `make doc-symbols-check` checks every `dp_*` name in a page's code spans
    and fences against the C sources. It found eight stale names, including a
    `dp_cf64_t` in a `skip=` fence the C snippet gate never compiles. Pages
    that name a symbol because it is gone sit on a list that may only shrink
    ([#2580](https://github.com/doppler-dsp/doppler/issues/2580)).
- **Facts the docs repeated wrongly are corrected on every page.** They
    include the CIC cap and its ±2.0 input bound, owning arrays called views,
    and removed MPSK and AWGN paths. Where a header or manifest carried the
    same text it is fixed there, so the docstrings agree too
    ([#2580](https://github.com/doppler-dsp/doppler/issues/2580)).
