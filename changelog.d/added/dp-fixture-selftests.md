- **`dp_mf_test.h` and `dp_dsss_test.h` have self-tests — and doppler#689 is
    a gate instead of a paragraph.** These were the last two entries on
    `docs/design/rx-test.md` §5.2's list; the section is now closed, and it
    records what writing the tests found rather than only that they exist.

    `dp_dsss_test.h` carries a **known defect in its own docstring**: the
    noise line scales `dp_cgauss` by `sigma/sqrt(2)`, the factor for the other
    complex-Gaussian convention, so every capture is **3.01 dB quieter than it
    claims**. It is deliberately unfixed — removing the `/sqrt(2)` makes an
    async BER sweep go non-monotonic (6 dB decodes, 8 dB fails, 10 dB
    decodes), which is acquisition succeeding or failing per point rather than
    a threshold, so correcting it is a receiver investigation. Until now the
    defect was held in place by prose alone.

    The self-test makes it a **characterization**: the 3.01 dB is measured and
    asserted, reproducing the header's own `E|n|^2 = 0.4996` against a target
    of 1.0. So the magnitude is a fact rather than a recollection, the level
    cannot drift further unnoticed, and a one-character "fix" turns the test
    **red on purpose** — forcing the investigation #689 asks for instead of
    quietly re-tuning two BER sweeps that have been passing on 3 dB of noise
    they never had. Proven: removing the `/sqrt(2)` reads −0.02 dB against the
    pinned −3.01.

    `dp_mf_test.h`'s `mf_evm_db()` takes a **min over strobe alignment** — the
    shape `dp_ber_test.h` calls "the historic footgun", legitimate here
    because the loop is open and the strobe phase is genuinely arbitrary, but
    exposed to the false-PASS that footgun names. So it is measured rather
    than argued: a stream carrying an unrelated sequence must read badly at
    every alignment the search tries, or every EVM this header has reported is
    a search result rather than a measurement. Also pinned: the fitted complex
    gain normalises out rotation *and* scale (an 80 dB swing when removed),
    which is what lets one number score both the complex and the real chain,
    and both traps the header documents get an assertion each.
