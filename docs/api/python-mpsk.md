# Python M-PSK Constellation API

The `doppler.mpsk` module is the **M-ary PSK constellation** layer over the C
`mpsk` core: Gray-coded map / demap for BPSK, QPSK, and 8PSK. It is the decision
primitive (and its transmit inverse) that the
[`track.Carrier.Mpsk`](python-track.md) carrier loop and the MPSK receiver
compose; the inline `mpsk_slice()` / `mpsk_constellation()` helpers in
`mpsk_core.h` are the C composition API those loops inline per symbol.

A symbol carries `log2(M)` bits packed LSB-first into one `uint8` (0..M−1); that
byte **is** the Gray-coded label, so a slip to an adjacent constellation point
flips exactly one bit. Constellations are unit amplitude: BPSK `{+1, −1}`, QPSK
`(±1 ± j)/√2` (axis-separable, at π/4), 8PSK `exp(j·k·π/4)`. `m` defaults to QPSK
and is keyword-capable.

See the [M-PSK gallery page](../gallery/mpsk.md) for the constellations and the
BER-vs-Eb/N0 validation against theory.

## Memoryless map / demap

`mpsk_map` and `mpsk_demap` are element-wise (one label byte ↔ one cf32 point),
absolute-phase. `mpsk_demap` is a hard decision (nearest point by phase;
amplitude-invariant).

::: doppler.mpsk.mpsk_map

::: doppler.mpsk.mpsk_demap

## Differential map / demap

The differential variants carry phase state across the array — information rides
on phase *differences*, so an unknown constant carrier rotation cancels at the
receiver. This resolves the M-fold phase ambiguity a decision-directed carrier
loop leaves, at ~2× the symbol-error rate. Every symbol after the implicit
zero-phase reference (the first) is rotation-invariant.

::: doppler.mpsk.mpsk_diff_map

::: doppler.mpsk.mpsk_diff_demap

## Helpers

::: doppler.mpsk.mpsk_bits_per_symbol
