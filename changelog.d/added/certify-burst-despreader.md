- **`BurstDespreader` is certified, and its lock gate is measured rather than
    asserted** — over 4000 noise-only bursts the realized false-alarm rate is
    within **7%** of the priced rate across a decade of `pfa`, closing
    `detection`'s `det_threshold_f` against a real consumer. Certifying it
    found the header's own `lock_stat` example asserting a false result, and
    that the `@code` block on a `*_get_*` accessor never reaches the doctest
    gate — [46 header examples library-wide are in that blind
    spot](https://github.com/doppler-dsp/doppler/issues/1000) (all extracted
    and run by hand; the other 45 pass). Evidence:
    `src/doppler/dsss/tests/validation/burst_despreader/results.md`.
