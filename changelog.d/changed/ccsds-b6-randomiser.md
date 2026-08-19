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

- **The reported coding-gain bound moves 6.1 → 4.1 dB, and the reason is the
    measurement's shape rather than the receiver's.**
    `validate_rx_coding_gain`'s cleanest point moves Es/N0 0.0 → 2.0 dB on the
    same chain, same seeds, same code, so the gate is re-baselined to 4.0 dB
    with the measurement recorded beside it. The ≥6.1 dB half of that pair was
    only ever measured on the legacy waveform, so it is the figure this
    release does **not** ship, and `docs/design/fec-receive.md` §8 reports the
    B-6 sweep throughout.

    B-6 changed the sequence deliberately, to remove the 255-bit one's
    spectral lines at 1/255 of the symbol rate and its ITU power-flux-density
    problem. A maximal-length sequence of degree *D* has a maximum run of
    exactly *D*, so the legacy randomiser guaranteed a transition every ≤ 8
    symbols and B-6's only every ≤ 17 — but both have the **same 50.00 %
    transition density** and the same run distribution below 8, and the whole
    difference is ~20 events per CADU, or 0.2 % of symbols.

    Isolated, that costs about **0.02 dB** of implementation loss, not 2 dB
    (gh-866, closed with the data). What moves the reported clean point two
    whole grid steps is a concatenated code on its cliff amplifying a ~3 %
    relative change in channel SER, read on a 1 dB sweep grid — B-6 at +1 dB
    was already at 1.08e-3 payload BER, so the true threshold shift is well
    under 2 dB.
