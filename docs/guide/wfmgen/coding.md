# Channel coding — the stages over a frame

Six `wfmgen` flags turn a framed waveform into a **coded** one:

```text
--rs-depth   --randomise   --asm   --conv   --interleave   --interleave-unit
```

They are not a chain of filters applied to "the frame". Each is a **stage**
that carries the **span it covers**, and the spans deliberately disagree — a
marker, a preamble and a sync word are things a receiver *finds* by
correlation, so they must look the same in every frame and the stages that
scramble or code the data must reach past them. Setting any of them frames
the waveform, exactly as
[`--sync` does](waveforms.md#framing-preamble-sync-word-crc).

!!! warning "This is **block** interleaving, not I/Q interleaving"

    Everywhere else in these guides, *interleaved* describes the **sample**
    layout on disk — `raw` files store I, Q, I, Q, … and `--symbols-file`
    reads that layout back. That is a memory format and has nothing to do
    with this page.

    `--interleave` is a **block interleaver over bits**: it permutes the
    frame's data group before transmission so that a burst of channel errors
    arrives spread across the outer code's codewords instead of destroying
    one of them. It changes *where* errors land and never touches the sample
    layout. The two meanings share a word and no code.

______________________________________________________________________

## The six flags

| Flag                  | Value                         | What it adds                                                     |
| --------------------- | ----------------------------- | ---------------------------------------------------------------- |
| `--rs-depth I`        | `1 2 3 4 5 8`                 | Reed-Solomon (255,223), `E = 16`, interleaved `I` codewords deep |
| `--randomise [G]`     | `ccsds` (bare) `legacy` `off` | XOR a pseudo-random sequence over the data group                 |
| `--asm`               | —                             | Prepend the `0x1ACFFC1D` attached sync marker                    |
| `--conv`              | —                             | Convolutional `K = 7`, rate 1/2 — **doubles** the bit count      |
| `--interleave R`      | depth, in rows                | Block-interleave the data group `R` deep; length-preserving      |
| `--interleave-unit N` | bits (default `1`)            | Bits per permuted unit — use `8` with `--rs-depth`               |

All six apply to `--type bits` and `--type dsss`. Each is separately optional
because the standards make it so.

## Where each stage reaches

The frame is a list of fields and a list of stages, and every stage names the
fields it covers. In application order:

| #   | Stage         | Flag           | Covers                                                    |
| --- | ------------- | -------------- | --------------------------------------------------------- |
| 1   | CRC-16        | `--crc`        | the payload                                               |
| 2   | Reed-Solomon  | `--rs-depth`   | payload + CRC → appends check symbols                     |
| 3   | randomiser    | `--randomise`  | the **data group**: payload, CRC, check symbols           |
| 4   | interleaver   | `--interleave` | the data group — **last**, so it is what the channel sees |
| 5   | convolutional | `--conv`       | **every** field, marker and sync word included            |

The disagreement in the last column is the whole reason a frame is a
description rather than a pipeline: a chain gets three of these boundaries
right and the fourth wrong, in the direction that still decodes against a
receiver of your own construction and syncs to nothing. The standard's own
sections, and the measurement behind each cover, are in
[A CCSDS CADU, as a Frame Description](../../gallery/ccsds-link.md).

______________________________________________________________________

## The stages, one at a time

A framed waveform with no coding, and the same frame scrambled and permuted:

```sh
# [ sync | payload | CRC-16 ] -- no coding
wfmgen --type bits --bits-hex b25a --sync 10110 --count 512 -o plain.cf32

# the data group scrambled, then permuted 4 deep in octets
wfmgen --type bits --bits-hex b25a --sync 10110 \
       --randomise --interleave 4 --interleave-unit 8 \
       --count 512 -o coded.cf32

# identical sizes: neither stage adds a bit
ls -l plain.cf32 coded.cf32
```

### `--rs-depth` — the outer code

Reed-Solomon (255,223) over `GF(256)`, correcting `E = 16` octets per
codeword, with `I` codewords interleaved by the codeblock layout itself.

The payload plus its CRC must be **exactly** `223 × I` octets. A short frame
is refused rather than padded — virtual fill is not implemented, and padding
would change a length both ends have to agree on. `--crc none` makes the
payload alone the `223 × I`.

Depth-`I` here is *intrinsic to the codeblock*, fused into encode and decode.
It is **not** `--interleave`, which is a permutation applied afterwards over
whatever span it is given — the two share a name and no implementation. See
[Interleaving, way 2](../../design/interleaving.md#2-it-is-not-the-outer-codes-own-depth).

### `--randomise` — which generator, not whether

```sh
wfmgen --type bits --bits-hex b25a --sync 10110 --randomise \
       --count 512 -o r-default.cf32
wfmgen --type bits --bits-hex b25a --sync 10110 --randomise legacy \
       --count 512 -o r-legacy.cf32
```

A bare `--randomise` selects `ccsds`: the 131071-bit sequence from
`h(x) = x^17 + x^14 + 1`, which 131.0-B-6 §10.4.1 requires. `legacy` selects
§10.4.2's 255-bit `h(x) = x^8 + x^7 + x^5 + x^3 + 1`, kept for backward
compatibility only — it puts spectral lines at 1/255 of the symbol rate.
`off` is the same as omitting the flag.

The sequence is its own inverse, so the receiver runs the identical call. The
two generators are **not interchangeable on the air**: only the matching
receiver derandomises a given waveform, which is why `--record` stores *which*
one rather than a bare `true`.

`--randomize` is accepted as an alias, spelled the other way. CCSDS 131.0-B-6
says "randomizer", this guide says randomiser, and the parser declines to have
an opinion; the two are the same flag and either may be typed.

### `--asm` — the marker

Prepends the 32-bit `0x1ACFFC1D` attached sync marker. It is a field, not a
transform: nothing scrambles or outer-codes it, because it is the pattern the
receiver correlates against to find the frame at all. The inner code does
cover it.

### `--conv` — the inner code

Convolutional `K = 7`, rate 1/2, over the **whole** frame including the
marker, so the emitted bit count doubles:

```sh
wfmgen --type bits --bits-hex b25a --asm --count 512 -o uncoded.cf32
wfmgen --type bits --bits-hex b25a --asm --conv --count 512 -o inner.cf32
```

`--count` is samples, not frame bits, so both files are 512 samples — the
doubling shows up as **half as many frames** in the same span: the frame here
is 32 marker + 16 payload + 16 CRC = 64 bits, so the capture carries 8 frames
uncoded and 4 coded. The decoder,
the node synchronization it needs first, and what it can and cannot recover
are in [The FEC Receive Half](../../design/fec-receive.md).

### `--interleave` and `--interleave-unit`

Write the data group by rows into an `R × C` matrix, read it back by columns.
`R` is `--interleave`; `C` follows from the span the stage covers, because
the stage's cover is what says how much there is to permute.

```sh
# 4 deep over a 32-bit data group: it divides, so 8 columns
wfmgen --type bits --bits-hex b25a --sync 10110 --interleave 4 \
       --count 512 -o ok.cf32

# 5 deep does not divide 32 -- refused, never padded
wfmgen --type bits --bits-hex b25a --sync 10110 --interleave 5 \
       --count 512 -o bad.cf32 || echo "refused, as documented"
```

With one codeword per row, reading by columns transmits one symbol from each
codeword in turn, so a burst of up to `R` consecutive units costs each
codeword at most one and an outer code correcting `t` per codeword survives a
burst of `t × R`. It multiplies the corrigible burst by the depth; it does
not remove the bound.

`--interleave-unit` is the size of a permuted unit in bits. Use **8** with
`--rs-depth`: RS is a code over `GF(256)`, so spreading a burst across
codewords means permuting octets — bit-interleaving an octet code spreads a
burst *inside* symbols that are already wrong. It is also an 8× throughput
parameter, one `memcpy` per eight bits rather than one per bit.

The transform, the three ways the reasoning goes wrong, and the measured
frame-error rates that put a number on all of it are in
[Interleaving — spreading a burst across codewords](../../design/interleaving.md).

______________________________________________________________________

## The canonical arrangement: a CCSDS CADU

Four of the flags — `--rs-depth`, `--randomise`, `--asm`, `--conv` — over a
`223 × I`-octet payload, with no preamble and no sync word, **is** a CADU.
That is a configuration of these flags, not a mode `wfmgen` switches into:

```sh
# a 223-octet Transfer Frame, as hex
python3 -c "print('5a'*223, end='')" > tf.hex

wfmgen --type bits --bits-hex "$(cat tf.hex)" --crc none \
       --rs-depth 1 --randomise --asm --conv \
       --modulation bpsk --sps 1 --count 4144 -o cadu.cf32
```

`--count 4144` is exactly one frame: 32 marker bits plus a 2040-bit data
group (`223 + 32` octets under RS(255,223)) is 2072, doubled by the inner
code, at one sample per symbol.

The gallery page renders what this repairs and what it refuses:
[A CCSDS CADU, as a Frame Description](../../gallery/ccsds-link.md).

### Adding an interleaver on top

`--interleave` is **not** part of that chain. CCSDS gets its burst protection
from the codeblock depth `I` alone, which is why the standard specifies no
separate permutation. Adding one is a valid arrangement — just not a CCSDS
one — and it is where `--interleave-unit 8` earns its keep, because the code
it is protecting has octets for symbols:

```sh
wfmgen --type bits --bits-hex "$(cat tf.hex)" --crc none \
       --rs-depth 1 --randomise --asm \
       --interleave 5 --interleave-unit 8 --conv \
       --modulation bpsk --sps 1 --count 4144 -o cadu-ilv.cf32
```

Length-preserving, so the count is unchanged; the interleaver divides the
2040-bit data group into `2040 / (5 × 8) = 51` columns and the corrigible
burst goes from `E` = 16 octets to `E × 5` = 80.

______________________________________________________________________

## The coding survives `--record`

`--record` writes the fully-resolved run, coding stages included, and
`--from-file` on that record reproduces the samples byte for byte:

```sh
wfmgen --type bits --bits-hex b25a --sync 10110 \
       --randomise --interleave 4 --interleave-unit 8 \
       --count 512 --record run.json -o a.cf32
wfmgen --from-file run.json -o b.cf32
python3 -c "print(open('a.cf32','rb').read()==open('b.cf32','rb').read())"
cat run.json
```

The record carries `randomise` as the generator's **name**, and `interleave`
/ `interleave_unit` as the geometry — a replay that guessed either would
produce a different waveform of the same length, with nothing to notice it.
The scene schema (`docs/schema/wfmgen.schema.json`) sets
`additionalProperties: false`, so an unlisted key is a validation failure
rather than a silent pass.

______________________________________________________________________

## On `--type dsss`

The stages cover the **spread** frame and not the acquisition preamble: a
preamble is transmitted unmodulated, because it is what a receiver correlates
raw chips against.

```sh
wfmgen --type dsss --bits-hex b25a --acq-code 1101100 --acq-reps 4 \
       --data-code 10110 --sync 10110 \
       --randomise --interleave 4 --sps 2 -o dsss.cf32
```

Everything else `type="dsss"` means — the second code, the chip clock, the
Es/N0 that refers to the outer data symbol — is in
[DSSS bursts](dsss-bursts.md).

______________________________________________________________________

## See also

- [Interleaving](../../design/interleaving.md) — the permutation, the three
    ways the reasoning goes wrong, and the measured burst gain.
- [A CCSDS CADU, as a Frame Description](../../gallery/ccsds-link.md) — why a
    stage carries a span, with the standard's section numbers.
- [The FEC Receive Half](../../design/fec-receive.md) — the decoder and the
    synchronization it needs before it can run.
- [Reed-Solomon](../../design/reed-solomon.md) and
    [Viterbi](../../design/viterbi.md) — the two codes themselves.
- [Waveforms](waveforms.md) — `--type`, the framing flags these stages
    extend, and the full flag reference.
