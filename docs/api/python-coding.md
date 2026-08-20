# Python Coding API

The `doppler.coding` module is the general channel codes — the code families
a standard configures, rather than any standard's picks. `ConvEncoder` and
`Viterbi` are the two directions of a rate-1/n convolutional code, and both
take the generator polynomials, so a caller names their own code rather than
choosing from a menu. The CODE itself — the generator polynomials, the encoder
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

Both directions, on a code the caller chose:

```pycon
>>> import numpy as np
>>> from doppler.coding import ConvEncoder, Viterbi
>>> # The CCSDS inner code: G1 = 171, G2 = 133 octal, k = 7.
>>> rng = np.random.default_rng(0)
>>> bits = rng.integers(0, 2, 512).astype(np.uint8)
>>> sym = ConvEncoder([0o171, 0o133], k=7).encode(bits)
>>> sym.size  # rate 1/2: two symbols per information bit, no fill
1024
>>> # Positive LLR means symbol 0 -- the convention mpsk_soft_demap produces.
>>> llr = np.where(sym, -4.0, 4.0).astype(np.float32)
>>> out = Viterbi([0o171, 0o133], k=7, depth=35).decode(llr)
>>> out.size  # 1024 symbols at rate 1/2, less the traceback still owed
478
>>> bool(np.array_equal(out, bits[: out.size]))
True

```

______________________________________________________________________

## `ConvEncoder` — the encoder

::: doppler.coding.ConvEncoder

______________________________________________________________________

## `Viterbi` — soft-decision convolutional decoder

::: doppler.coding.Viterbi

______________________________________________________________________

## `ReedSolomon` — both directions of a block code

A Reed-Solomon code over `GF(2**J)` is five numbers: a symbol width, a field
polynomial, a parity count, a first root and a root stride. `ReedSolomon`
takes all five, so a caller names their own code — and both directions read
the same description, which is what stops an encoder and a decoder disagreeing
about what the code is.

```pycon
>>> import numpy as np
>>> from doppler.coding import ReedSolomon
>>> rs = ReedSolomon(nroots=32)          # RS(255,223) over the usual GF(256)
>>> rs.n, rs.k, rs.e
(255, 223, 16)
>>> word = rs.encode(np.arange(rs.k, dtype=np.uint8))
>>> word.size, rs.codeword_ok(word)
(255, 1)
>>> word[3] ^= 0xFF                       # a symbol, however many bits moved
>>> rs.decode(word)                       # corrected IN PLACE
1

```

`decode` corrects the array you hand it. That is the C contract reaching
Python rather than a convenience: a decode that quietly worked on a copy
would return the right count and leave your data wrong, so the binding
demands a writable, C-contiguous `uint8` array instead of accepting anything
convertible.

!!! warning "A refusal is safe. A miscorrection is not."

    `decode` returns `-1` when the word is too far from every codeword to
    name one — the receiver *knows*. Beyond `E` errors it can instead land
    inside a **different** codeword's sphere, return a positive count, and
    pass `codeword_ok`: silently wrong. That is a property of any
    bounded-distance code, not of this implementation, and the chance of it
    is about `sum(C(n, i)(q-1)**i for i <= E) / q**(n-k)` — **2e-05** for
    RS(255,223) and **0.36** for RS(15,11). Parity is what buys the silence;
    frame-level accounting is what catches the rest. Measured in
    [the gallery](../gallery/coding.md).

    `-2` is a different fact entirely: the word was not `n` symbols long,
    which is your bug rather than the channel's.

### Matching the algebra is not matching the wire

The five numbers are the **code**. A standard adds conventions that are not
properties of it, and CCSDS adds two — symbols travel in the **dual
(Berlekamp) basis** (131.0-B 4.3.9) and codewords are **interleaved** (4.4.1).
Construct `ReedSolomon` with CCSDS's five numbers and the arithmetic is right
while the wire format is not:

```pycon
>>> ccsds = ReedSolomon(nroots=32, field_poly=0x87, first_root=112,
...                     root_stride=11)          # 131.0-B 4.3
>>> g = np.empty(ccsds.nroots + 1, np.uint8)
>>> ccsds.generator(g)                           # Annex G prints all 33
33
>>> int(g[0]), int(g[-1])
(1, 1)
>>> textbook = np.empty(33, np.uint8)
>>> _ = ReedSolomon(nroots=32).generator(textbook)
>>> bool((g != textbook).any())                  # a different code entirely
True

```

`generator()` takes the buffer rather than handing you one, because its length
is a property of the code and not of the call — a self-sizing method would
carry a `count` parameter that could only mislead. It is there because
standards publish those coefficients: it is how you check that you read the
five numbers correctly, against the document rather than against this
implementation. Two of the five are **validated at
construction** for the same reason — a non-primitive `field_poly` and a
`root_stride` sharing a factor with `n` both produce arithmetic that encodes
and decodes against itself perfectly, so a round trip can never catch either.

For a whole CCSDS CADU — the dual basis, the interleaver and the coverage
table — describe one with
[`FrameDesc`](python-wfmgen.md#framedesc-the-same-frame-deferred) instead.

::: doppler.coding.ReedSolomon

## Related pages

<!-- related-pages:start -->

**Gallery** — [A CCSDS CADU, as a Frame Description](../gallery/ccsds-link.md), [Name Your Own Code — and What Happens Past the Radius](../gallery/coding.md)
**Design** — [The FEC Receive Half](../design/fec-receive.md), [Design](../design/index.md), [Reed-Solomon](../design/reed-solomon.md)

<!-- related-pages:end -->
