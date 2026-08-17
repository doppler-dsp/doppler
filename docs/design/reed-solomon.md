# Reed-Solomon

A general Reed-Solomon code over `GF(2^J)`: the description, the encoder, the
syndromes, and the decoder that **corrects** — Berlekamp-Massey, Chien and
Forney, all reading the same description.

Phase 1 of [Adding an Algorithm](../dev/adding-algorithms.md). This page owns
the outer code the way [The Viterbi Decoder](viterbi.md) owns the inner one,
and for the same reason: [The FEC Receive Half](fec-receive.md) is a *chain*,
and a chain is the wrong home for the algebra of one of its links.

**Nothing here is CCSDS.** 131.0-B-3's (255,223) `E = 16` is a *configuration*
— five numbers — and it is held in `fec` beside the randomiser and the ASM,
with the dual basis and the interleaver that are the standard's and not the
code's. Point this at RS(204,188) for DVB, at RS(15,11) to check something by
hand, or at whatever a caller brings; the arithmetic is identical and only the
table changes.

______________________________________________________________________

## 1. The description

A Reed-Solomon code is five numbers, and `rs_code_t` holds them:

| field         |                                      | CCSDS 4.3 | textbook |
| ------------- | ------------------------------------ | --------- | -------- |
| `symbol_bits` | `J`, bits per symbol                 | 8         | 8        |
| `field_poly`  | `F(x)`, low `J` bits, `x^J` implicit | `0x87`    | `0x1D`   |
| `nroots`      | parity symbols, `2E`                 | 32        | any      |
| `first_root`  | `j0` — the first root is `a^(s*j0)`  | 112       | 0 or 1   |
| `root_stride` | `s` in `g(x) = prod (x - a^(s*j))`   | **11**    | 1        |

Everything else follows: `n = 2^J - 1`, `k = n - nroots`, `E = nroots / 2`.
Symbols are **packed, one per byte** — a Reed-Solomon symbol *is* a byte at
`J = 8`, and at `J < 8` it is a byte with the top bits clear.

Two of those five are validated rather than trusted, because both failures are
silent:

- **`field_poly` must be primitive.** The table build walks `a^i` for `i` in
    `[0, n)`; if it returns to 1 early the polynomial generates a subgroup, not
    the field, and `rs_init` refuses. A non-primitive polynomial produces
    perfectly self-consistent arithmetic over a smaller set.
- **`gcd(root_stride, n)` must be 1.** Otherwise `a^s` is not primitive, the
    `nroots` "roots" are not distinct, and the code corrects fewer errors than
    its parity count claims — while still encoding, still checking, and still
    passing any test that only round-trips.

CCSDS 4.3.4 states the second condition itself, as a note that `a^11` is
primitive. It is a note because for the standard it is a fact; for a general
implementation it is an argument that has to be checked.

______________________________________________________________________

## 2. What belongs to the code, and what belongs to the standard

The split is the whole point of the file boundary, so it is worth stating as a
table rather than leaving it to be inferred:

| thing                        | whose          | where          |
| ---------------------------- | -------------- | -------------- |
| field, roots, stride         | the code's     | `rs_code_t`    |
| encode / syndromes / correct | the code's     | `rs_core.c`    |
| the **dual basis** (4.3.9)   | the standard's | `fec/fec_rs.h` |
| the **interleaver** (4.4.1)  | the standard's | `fec/fec_rs.h` |
| `E = 16`, `J = 8`, `s = 11`  | the standard's | `FEC_CCSDS_RS` |

The dual basis is the one most likely to be argued into the wrong file,
because it looks like arithmetic. It is not: it is a **representation of a
symbol on the wire**, chosen by 4.3.9.1 so that a hardware multiplier over the
dual basis is cheaper. The code is the same code in either basis. So `rs`
works in the conventional basis throughout and `fec` transforms at its own
boundary — which is also the honest place for it, since a decoder that works
in the wrong basis decodes its own encoder perfectly and interoperates with
nothing. That failure has now appeared three times in this slice, in three
different guises.

______________________________________________________________________

## 3. Syndromes are the definition, and there is one copy of them

`S_m = C(a^(s*(j0+m)))` for `m` in `[0, nroots)`, evaluated by Horner over the
codeword with position `i` carrying `x^(n-1-i)`. All zero **is** what "is a
codeword" means — it needs no decoder, no encoder and no round trip, which is
what makes it usable as a test oracle.

`rs_codeword_ok` is that test, and `rs_decode` starts with the same function
rather than a second copy of the loop. The issue this closes
([#826](https://github.com/doppler-dsp/doppler/issues/826)) asked for exactly
that, and it is the general rule from `conv_outputs`: the arithmetic is never
what drifts between two implementations of a primitive — the convention is.

______________________________________________________________________

## 4. The decode, and the two offsets a textbook will not warn about

Let `b = a^s` (primitive, by §1), and let an error of value `Y_p` sit at
codeword index `i_p`, i.e. at `x^(L_p)` with `L_p = n - 1 - i_p`. Write
`X_p = b^(L_p)`. Then

```text
S_m = sum_p  Y_p * X_p^(j0 + m)                 m = 0 .. nroots-1
```

**Offset one — the syndromes are a power sum only after a substitution.**
Define `T_m = S_m` and `Yt_p = Y_p * X_p^(j0)`; then `T_m = sum_p Yt_p X_p^m`,
which is the sequence Berlekamp-Massey is written for. Running BM on `S`
while *assuming* `j0 = 1` yields the right error **positions** — they do not
depend on `j0` — and magnitudes wrong by a factor `X_p^(j0-1)`. Every
syndrome then still checks out against the decoder's own model, so the bug
survives any test that decodes what it encoded. It is caught only by a
codeword someone else produced, or by the `j0 = 112` of a real standard.

BM gives the error locator `Lambda(x) = prod (1 - X_p x)`, of degree `v`.

**Offset two — Chien iterates positions, not field elements.** For `L` in
`[0, n)` evaluate `Lambda(b^-L)`; a zero means an error at index `n - 1 - L`.
Iterating the *position exponent* is what makes `root_stride` a non-issue:
a search over field elements `a^e` has to invert the stride
(`L = e * s^-1 mod n`, and `11^-1 = 116` for CCSDS) to learn where the error
is, and that inverse is one more place for the stride to be silently dropped.
Since `b` is primitive, `L -> b^L` is a bijection: iterating positions covers
every element exactly once and costs the same.

Forney supplies the magnitudes. With `Omega = T * Lambda mod x^nroots` and
`y = X_p^-1`:

```text
Omega(y)   = Yt_p * prod_{q != p} (1 - X_q / X_p)
Lambda'(y) = X_p  * prod_{q != p} (1 - X_q / X_p)      (char 2: signs vanish)

  =>  Yt_p = X_p * Omega(y) / Lambda'(y)
  =>  Y_p  = Omega(y) * X_p^-(j0-1) / Lambda'(y)
```

The `X_p^-(j0-1)` is offset one, arriving in the only place it is visible.
For `j0 = 1` it is 1, which is why the textbook formula omits it — and why
transcribing the textbook formula gives a decoder that works on every example
in the textbook and on no spacecraft.

### When it refuses, and what a refusal is not

`rs_decode` returns `-1` when `deg Lambda > E`, or when Chien finds fewer than
`deg Lambda` distinct roots. Both mean the received word is not within `E`
symbols of any codeword the decoder can name.

It does **not** mean "more than `E` errors", and the converse does not hold
either: beyond `E` errors a bounded-distance decoder can land inside another
codeword's sphere and *miscorrect*, silently and correctly-by-its-own-lights.
That is a property of the code, not of this implementation, and the protection
against it is frame accounting — which is why `fec_frame_rx_t` reports counts
rather than folding them into a single verdict.

One thing it cannot do, and this **is** assertable: when it corrects, the
result is a codeword. The key equation zeroes every one of the `nroots`
syndromes by construction, so `rs_decode` either returns `-1` or returns a
word that passes `rs_codeword_ok`. There is no third outcome, and the test
asserts it over error patterns from 0 to `n` symbols.

______________________________________________________________________

## 5. External truth

Reed-Solomon is the worst offender in this slice for tests that cannot fail:
encode-then-decode inverts correctly over *any* field polynomial, *any* root
set and *any* basis, so long as both ends agree. Every check below is against
something the code cannot choose:

| claim                                              | why it cannot be self-satisfied           |
| -------------------------------------------------- | ----------------------------------------- |
| corrects any `E` symbol errors, exactly            | a boundary the code's `d = n-k+1` fixes   |
| never *recovers the sent word* at `E+1`            | impossible for a bounded-distance decoder |
| corrects or refuses — never returns a non-codeword | the key equation, §4                      |
| CCSDS `g(x)` = Annex G, coefficient by coefficient | a published value                         |
| the two 4.3.9.3 matrices invert across all 256     | a published pair                          |
| a burst of `B` becomes `ceil(B/I)` per codeword    | what interleaving is *for*                |

The last two live in `test_fec_rs.c`, with the configuration they belong to.
The first three are `test_rs_core.c`'s, at both `J = 8` and `J = 4`, where
RS(15,11) is small enough to sweep every position and every value.

**The zeros trap, again.** An all-zero information block has all-zero parity,
so every interleaved column is identical and a rotated de-interleave is the
identity. It has now hidden two separate defects in this slice. Correction is
tested on structured data, and the tests say so where they do it.

______________________________________________________________________

## 6. What it buys

Measured by `examples/c/ccsds_link_demo.c`, which runs the whole chain at
`I = 5` and reports per-codeword outcomes. The interesting row is the one
where the inner code does not clear the channel on its own:

| `Es/N0` | channel SER | post-Viterbi BER | R-S codewords good   | symbols repaired |
| ------- | ----------- | ---------------- | -------------------- | ---------------- |
| 0 dB    | 7.90 %      | 3.24e-04         | **10/10** (was 7/10) | 3                |
| 1 dB    | 5.58 %      | 1.50e-04         | 10/10                | 0                |
| ≥ 2 dB  | ≤ 3.66 %    | 0                | 10/10                | 0                |

At 0 dB the outer code now repairs the three symbols the Viterbi let through
and returns frames that are byte-exact; before, those three symbol errors sat
in three different codewords and cost three whole codewords, and the frame
came back wrong-but-known-wrong. That is the whole of
[#826](https://github.com/doppler-dsp/doppler/issues/826) in one row.

Note what the `repaired` column is *for*: at 1 dB and above it reads zero, so
the outer code is idle and every bit of margin is the inner code's. A rising
count with `10/10` still holding is the concatenation spending margin — and it
is spent before it is lost, which is the number a link budget wants.
Coding gain proper is [The FEC Receive Half](fec-receive.md) §7 step 4,
measured through the existing harness rather than a new sweep.

______________________________________________________________________

## 7. What is not here

- **Erasure decoding.** The `2E` budget can correct `2E` erasures instead of
    `E` errors when the demodulator says *where* the bad symbols are. Nothing
    in doppler produces that flag today, and a decoder with an erasure
    interface nobody can fill is an interface that goes untested. When a
    soft-output demapper can mark symbols, this is where it plugs in.
- **Shortened codes / virtual fill.** CCSDS 4.4.2's shortened codeblock is
    [#813](https://github.com/doppler-dsp/doppler/issues/813); a frame off the
    `223*I` grid is refused rather than padded.
- **The lazy table build's data race**
    ([#817](https://github.com/doppler-dsp/doppler/issues/817)) — `fec`'s
    CCSDS singleton builds its tables on first use. `rs_init` is explicit and
    has no such state; the race lives in the configuration layer and becomes
    reachable the moment a `nogil` binding or `dp_parallel` calls it.

______________________________________________________________________

## See also

- [The Viterbi Decoder](viterbi.md) — the inner code, same split: a general
    decoder with CCSDS as a configuration
- [The FEC Receive Half](fec-receive.md) — the chain this is step 2 of
- [Adding an Algorithm](../dev/adding-algorithms.md) — the lifecycle
