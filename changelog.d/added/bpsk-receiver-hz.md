- **`track.BpskReceiver` — the receiver asked for in the units a capture comes
    with.** Two required arguments, `sample_rate_hz` and `symbol_rate_hz`,
    against `MpskReceiver`'s seventeen parameters. `m` is carried by the class
    name; `sps` is `fs / Rs`, a ratio the library computes for its own use in
    planning a cascade, so a caller never states it; and `carrier_freq_hz`
    defaults to 0 for complex baseband. Nothing on the signature is normalised
    to anything ([#831](https://github.com/doppler-dsp/doppler/issues/831)).

    It is a **view** over the same core, not a second type — the rule being
    that a difference in constructor is a flavor — and a test asserts it
    produces bit-identical symbols to the equivalent `MpskReceiver`, because
    one core is the whole claim. `MpskReceiver` is unchanged.

    `m_out` deriving rather than being pinned is not cosmetic: `m_out=4`
    against the default I&D pulse is measured **3.11 dB** off the coherent
    bound where the derived 8 is 0.41 dB off. A parameter nobody needed was a
    way to lose most of a link's margin quietly, and the telemetry demo had
    been doing exactly that.

- **The receiver publishes the recovered SYMBOL, not just its loop state.**
    `sym.i` and `sym.q` join the fourteen existing probes
    ([#846](https://github.com/doppler-dsp/doppler/issues/846)). Every probe
    before them was an internal, so a filed capture held the scene around a
    number and not the thing the number is computed from — no constellation,
    and no error rate recomputable from the evidence. A telemetry record
    carries one `float`, so a complex value cannot be one probe; the pair
    lands on the sample index the format already stamps, and
    `sym.i + 1j*sym.q` reconstructs `steps()` output exactly.
