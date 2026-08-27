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

## Soft demapping

`mpsk_soft_demap` is the same decision seen differently: instead of one label
byte per symbol it writes `log2(M)` log-likelihood ratios, one per bit, which
is what a soft-input decoder (a Viterbi, for a convolutional code) needs — a
hard decision throws away most of the coding gain such a decoder exists to
deliver.

`L = log(P(bit = 0) / P(bit = 1))`, so **positive means bit 0** and the hard
decision is `L < 0`. That is not a second decision rule: its sign reproduces
`mpsk_demap`'s label at every M and every SNR, which is asserted rather than
assumed. Bits are LSB-first within a symbol and symbols run in order, so
`llr[i * log2(M) + b]` is bit `b` of symbol `i`.

`llr` is a caller-provided output array rather than a returned one, because the
output expands by `log2(M)` — size it as `len(x) * mpsk_bits_per_symbol(m)`.
`n0` scales the result exactly and a Viterbi is invariant to it, so a caller
with no SNR estimate may pass `1.0`.

See [Soft Decisions for M-PSK](../design/mpsk.md#97-soft-decisions) for the derivation, the
closed forms BPSK and QPSK turn out to have, and what max-log costs at 8PSK.

::: doppler.mpsk.mpsk_soft_demap

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

## Related pages

<!-- related-pages:start -->

**Gallery** — [M-PSK Carrier Loop — Theory Validation](../gallery/carrier-mpsk.md), [DsssBurstReceiver — the Composed Burst Chain](../gallery/dsss-burst-receiver.md), [M-PSK constellation (Gray-coded map / demap)](../gallery/mpsk.md)
**Design** — [`DsssBurstReceiver`: the burst chain, composed in C](../design/dsss-burst-receiver.md), [The FEC Receive Half](../design/fec-receive.md), [MPSK Receiver](../design/mpsk.md), [The Viterbi Decoder](../design/viterbi.md)

<!-- related-pages:end -->
