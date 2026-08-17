- `rs` is certified. The Reed-Solomon header's claims were enumerated and
    mapped onto `test_rs_core.c`, which gained seven sections for the ones
    nothing ran: the derived sizes and the declared range, the consequence of
    the root-stride rule, the parity as a remainder by long division, the
    syndrome closed form, the packed-symbol convention at `J < 8`, every error
    count up to `E`, and that the description carries no running state. The
    evidence layer is
    [`src/doppler/tests/validation/rs/results.md`](https://github.com/doppler-dsp/doppler/blob/main/src/doppler/tests/validation/rs/results.md),
    measured by `native/validation/rs_certify.c` on the C-only track, and it
    establishes the number a caller sizes parity by: a failure past `E` is
    **silent** with probability `V(E)/q^(n-k)` — 0.99 at `E = 1`, 2.6e-14 at
    CCSDS's `E = 16` — and that probability does not fall as the damage grows.
- `rs_core.h` no longer offers RS(204,188) as a code to point the file at.
    `n` is `2^J - 1` by construction, so DVB's code is a *shortened*
    RS(255,239) and needs the virtual fill of
    [#813](https://github.com/doppler-dsp/doppler/issues/813); both the header
    and `docs/design/reed-solomon.md` now name the mother code.
