- **The pseudo-randomiser follows 131.0-B-6, which changed the default.**
    §10.4.1 makes the **131071-bit** sequence (`h(x) = x¹⁷ + x¹⁴ + 1`, preset
    `11000111000111000`) the requirement; §10.4.2 keeps the 255-bit one
    doppler shipped *"for backward compatibility with legacy systems"*. Both
    are now available as configurations — `CCSDS_TM_RAND` and
    `CCSDS_TM_RAND_LEGACY` — over one generator, and the default is B-6's.

    `wfmgen --randomise` takes an optional generator: bare means `ccsds`,
    and `--randomise legacy` selects the old one. `--record` carries **which**
    rather than a bare `true`, because the two are not interchangeable on the
    air — only the matching receiver derandomises a given waveform, so a
    record naming neither could not rebuild its own capture. A boolean is
    still read on input, as the default, so older records load.

    The frame decoder now **steps** the generator alongside the pack instead
    of indexing a 255-entry table. That table was free at the old period and
    would be 128 KB at the new one — and longer than any CADU, so it would
    never wrap.

- **Adopting it costs ~2 dB, and the reason is worth knowing (gh-866).**
    `validate_rx_coding_gain`'s cleanest point moves Es/N0 0.0 → 2.0 dB and
    its bound 6.1 → 4.1 dB, so the gate is re-baselined to 4.0 dB with the
    measurement recorded beside it. The earlier ≥6.1 dB figure was measured
    on the legacy waveform and is not comparable.

    The loss is **before the decoder** — channel SER is worse at the same
    Es/N0 — and the mechanism is a hard guarantee rather than a drift. A
    maximal-length sequence of degree *D* has a maximum run of exactly *D*,
    so the legacy randomiser **guaranteed a transition at least every 8
    symbols** and B-6's guarantees only every 17. Measured over one CADU: max
    run 8 → 15, with 20 runs longer than 8 where there were none.

    B-6 made that trade deliberately, to remove the 255-bit sequence's
    spectral lines at 1/255 of the symbol rate and its ITU power-flux-density
    problem. doppler's timing loop was drawing ~2 dB from a property of a
    randomiser chosen for unrelated reasons; gh-866 is that finding, and
    reverting would only make it invisible again.
