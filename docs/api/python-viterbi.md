# Python Viterbi API

The `doppler.viterbi` module is soft-decision maximum-likelihood decoding of
convolutional codes. The CODE itself — the generator polynomials, the encoder
and the trellis arithmetic — lives in the C `conv` component; this is the
DECODER built over one, so a caller names the polynomials and gets a decoder
for them rather than picking from a fixed menu.

**Soft in, hard out.** `decode` takes log-likelihood ratios, one per channel
symbol, and returns information bits. Hard-decision decoding throws away most
of the coding gain the code exists to provide — roughly 2 dB of it — which is
why the input is LLRs and not sliced bits. The convention is the library's:
**positive means symbol 0**, which is what
[`doppler.mpsk.mpsk_demap`](python-mpsk.md) produces, so the two compose
without a sign fix in between.

Only the SIGN carries the decision and only the RATIO of magnitudes carries
the confidence, so an unscaled LLR stream decodes identically to a calibrated
one — a caller with no `N0` estimate loses nothing.

Two numbers size the decoder, and both are the caller's:

- **`k`**, the constraint length — the trellis has `2**(k-1)` states, so cost
    doubles with every step of `k`. CCSDS 131.0-B-3 section 3 uses `k = 7`.
- **`depth`**, the traceback depth in bits — how far back a survivor must
    agree before a decision is emitted. `5 * (k - 1)` is the usual rule of
    thumb; the first `depth - 1` bits of a stream are still owed when a block
    returns, which is why `len(decode(x))` is shorter than the symbol count
    divided by the rate.

State carries across calls, so a long capture can be fed in blocks and the
result is bit-identical to one call — and the decoder serializes, so a chain
that checkpoints can checkpoint through it (see
[State Serialization](../design/state-serialization.md)).

Source:
[`native/src/viterbi/viterbi_core.c`](https://github.com/doppler-dsp/doppler/blob/main/native/src/viterbi/viterbi_core.c)

The design, including how the butterfly and the survivor ring are laid out,
is [The Viterbi Decoder](../design/viterbi.md); the end-to-end coded link is
the [CCSDS Link gallery page](../gallery/ccsds-link.md).

```pycon
>>> import numpy as np
>>> from doppler.viterbi import Viterbi
>>> # The CCSDS inner code: G1 = 171, G2 = 133 octal, k = 7.
>>> v = Viterbi([0o171, 0o133], k=7, depth=35)
>>> # Soft symbols: positive means 0, negative means 1.
>>> llr = np.array([4.0, -4.0] * 512, dtype=np.float32)
>>> bits = v.decode(llr)
>>> bits.dtype
dtype('uint8')
>>> len(bits)  # 1024 symbols at rate 1/2, less the traceback still owed
478

```

______________________________________________________________________

## `Viterbi` — soft-decision convolutional decoder

::: doppler.viterbi.Viterbi

## Related pages

<!-- related-pages:start -->

**Gallery** — [A CCSDS CADU, as a Frame Description](../gallery/ccsds-link.md)
**Design** — [The FEC Receive Half](../design/fec-receive.md), [Design](../design/index.md), [Reed-Solomon](../design/reed-solomon.md)

<!-- related-pages:end -->
