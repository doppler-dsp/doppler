# Fields — bits, written as text

Every place wfmgen takes a run of bits takes it the same way: as one
**Field**, a short piece of text such as `0x1ACFFC1D` or `pn:1023:10`. The
command line's `--bits`, `--sync`, `--acq-code` and `--data-code`, a scene's
`"spec"`, Python's `field_bits()` and C's `dp_wfm_field_parse()` all read it
with **one** parser, so a Field means the same bits on every face. This page
is the grammar; the reasoning behind it is the design's
[§F](../../design/frame-description.md#f-the-field-one-text-form).

## The forms

| write                                  | means                                                                                   |
| -------------------------------------- | --------------------------------------------------------------------------------------- |
| `1101`                                 | the bits themselves, left to right                                                      |
| `0xA5`                                 | hex, 4 bits a digit, most significant first                                             |
| `pn:LEN:REG[:SEED[:POLY]][:fibonacci]` | `LEN` bits from one `REG`-bit LFSR. `SEED` 0 selects 1; `POLY` 0 the maximal-length one |
| `gold:LEN:REG:TA:SA:TB:SB`             | `LEN` bits of a Gold code: two `REG`-bit registers, taps and seeds                      |
| `dotted:LEN`                           | `1010…`, `LEN` bits, starting high                                                      |

Any of them may end in `*REPS`, the same bits again `REPS` times.

```pycon
>>> from doppler.wfm import field_bits
>>> field_bits("1101").tolist()
[1, 1, 0, 1]
>>> field_bits("0xA5").tolist()
[1, 0, 1, 0, 0, 1, 0, 1]
>>> field_bits("pn:15:4").tolist()
[1, 1, 1, 1, 0, 1, 0, 1, 1, 0, 0, 1, 0, 0, 0]
>>> field_bits("dotted:6").tolist()
[1, 0, 1, 0, 1, 0]
>>> field_bits("0101*3").tolist()
[0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1]
```

`LEN` is the number of bits that come **out**; `REG` is the width of the
register that makes them. They are different numbers: `pn:15:4` is one whole
period of a 4-bit register, and `pn:100:4` is the same period cycled.

A repetition is the same bits again, never a fresh draw — a preamble repeated
four times is one period four times, which is what a receiver integrates over:

```pycon
>>> import numpy as np
>>> np.array_equal(field_bits("pn:31:5*4"), np.tile(field_bits("pn:31:5"), 4))
True
```

## Numbers

Every number — `LEN`, `REG`, `SEED`, `POLY`, the Gold taps and seeds, `REPS` —
is decimal, or hex after `0x`, so a tap mask reads the way the literature
writes it. A leading `0` is decimal, never octal. A number is read **whole**:
`12abc`, `-1`, ` 5` and an empty `pn::10` are refused, not read as far as
they go.

```pycon
>>> np.array_equal(field_bits("pn:0x1f:5"), field_bits("pn:31:5"))
True
```

## The limits

| limit                             | why                                                                                                            |
| --------------------------------- | -------------------------------------------------------------------------------------------------------------- |
| `LEN` ≥ 1, `REPS` ≥ 1             | a Field is never empty                                                                                         |
| `REG` is 1 to 64                  | the register is a 64-bit word                                                                                  |
| `SEED`, `POLY`, taps fit in `REG` | a wider number would be masked, silently: `pn:31:5:32` is the all-zero register                                |
| a 1-bit `REG` needs a `POLY`      | one bit has no maximal-length polynomial to default to                                                         |
| `LEN × REPS` ≤ 261120             | a longer run is a stream, not a Field; [derived here](../../design/frame-description.md#the-limits-as-numbers) |

On the command line, `*REPS` belongs to `--acq-code` alone, because a
preamble is the one field that repeats: a sync word or a payload sent twice is
not what those flags mean, so `--sync 'pn:31:5*2'` is refused. Quote a Field
with a `*` in it, or the shell globs it.

## Refused, never repaired

A Field outside the grammar is refused with a sentence naming the rule, and
nothing is built. A `0`/`1` string with a stray character is the case that
matters most: a typo that quietly shortened a sync word would still assemble,
and sync to nothing.

```sh
# Each line exits 2 and says why.
! wfmgen --type bits --bits 01a1 --count 16 -o x.cf32
! wfmgen --type bits --bits pn:31:5:32 --count 16 -o x.cf32
! wfmgen --type bits --bits pn:4000000000:5 --count 16 -o x.cf32
```

```text
error: --bits 01a1: a binary literal holds a character other than 0 or 1
error: --bits pn:31:5:32: a pn SEED or POLY has a bit above its REG-bit register
error: --bits pn:4000000000:5: LEN * REPS is past the Field bound of 261120 bits
```

## One way to write each Field

Two spellings of one Field are the same Field, and it has **one** canonical
text: a literal as hex when its length is a multiple of 4 and as bits
otherwise; a generator with only what differs from its defaults; `*REPS` only
past one. `--record` writes that text, so a capture's metadata says what
produced its bits:

```sh
wfmgen --type bits --bits 0101010111001010 \
       --sync pn:0x1f:5:0:0:galois --acq-code '1111*2' \
       --count 256 --record run.json -o framed.cf32
grep -E '"(payload|acq_code|sync)"' run.json
```

```text
			"payload":	"0x55ca",
			"acq_code":	"0xf*2",
			"sync":	"pn:31:5",
```

## From C

The same parser, and the same refusals. This is
`native/examples/wfm_field_demo.c`, which `make test-examples-c` builds and
runs; its Python twin is `src/doppler/examples/wfm_field_demo.py`.

```c
--8<-- "native/examples/wfm_field_demo.c"
```

## Related

- [Waveforms](waveforms.md) — which flags take a Field, and framing
- [Options reference](options.md) — every flag and scene key
- [The design, §F](../../design/frame-description.md#f-the-field-one-text-form) — why one grammar
