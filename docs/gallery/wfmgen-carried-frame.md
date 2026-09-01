# A Frame You Built, Generated

wfmgen has always been able to frame a waveform. What it could not do was
take a frame *you* described.

Framing reached the generator as flat flags — `--sync`, `--acq-code`,
`--crc`, `--rs-depth`, `--asm`, `--conv` — and between them they spell the
frames doppler already knows, at the positions doppler already puts them. A
layout outside that shape needed a new flag, which is one more spelling of
something [`wfm_frame_desc_t`](ccsds-link.md) could already describe: fields
in wire order, stages with the span each covers.

`wfm_source_t.frame` is the way in. Point a source at a description and that
description **is** the frame; the flat fields stay, as sugar that builds one
of these, so every scene, flag and JSON key written before this keeps working
unchanged.

## The whole thing, in one program

A 16-bit header of the caller's own bits, a payload, and a CRC-16 over a span
named rather than counted — then the samples, checked back against the
description that produced them.

```c
#include <complex.h>
#include <stdio.h>
#include <string.h>
#include <wfm/wfm_compose.h>
#include <wfm/wfm_frame.h>
#include <wfm_synth/wfm_synth_core.h>

#define NBITS 56u /* 16 header + 24 payload + 16 CRC */
#define SPS 4u
#define NS (NBITS * SPS)

int
main (void)
{
  /* Bits the caller owns. A description BORROWS them, so they must outlive
     every compose call that reads it. */
  static uint8_t hdr[16], payload[24];
  for (unsigned i = 0; i < 16u; i++)
    hdr[i] = (uint8_t)((0x5C5Cu >> (15u - i)) & 1u);
  for (unsigned i = 0; i < 24u; i++)
    payload[i] = (uint8_t)((i * 7u + 1u) & 1u);

  wfm_seq_t h = { 0 }, p = { 0 };
  h.kind = WFM_SEQ_LITERAL, h.bits = hdr, h.len = 16u;
  p.kind = WFM_SEQ_LITERAL, p.bits = payload, p.len = 24u;

  /* Fields in WIRE order; the stage names the span it covers. The cover
     reaches the derived field, which is what wires that field's producer --
     so a CRC's position and the fact that a CRC produces it are one
     declaration rather than two that can disagree. */
  wfm_frame_desc_t d;
  memset (&d, 0, sizeof d);
  if (wfm_frame_add_field (&d, "hdr", &h, 0u) < 0
      || wfm_frame_add_field (&d, "payload", &p, 0u) < 0
      || wfm_frame_add_derived (&d, "crc", WFM_FRAME_CRC_BITS) < 0
      || wfm_frame_add_stage (&d, WFM_STAGE_CRC16, "payload", "crc") < 0)
    return 1;

  /* One source, carrying it. A frame needs an explicit payload, which is
     what `type=bits` gives it. */
  wfm_source_t src = { 0 };
  src.type = WFM_SYNTH_BITS;
  src.payload = p;
  src.modulation = 1; /* bpsk */
  src.sps = (int)SPS;
  src.snr = WFM_SYNTH_SNR_CLEAN; /* no AWGN, so the checks are equalities */
  src.snr_mode = 1;
  src.frame = &d;
  if (wfm_source_frame_error (&src) != NULL)
    return 1;

  wfm_segment_t seg = { 0 };
  seg.sources = &src;
  seg.n_sources = 1u;
  seg.fs = 1.0e6;
  seg.num_samples = NS;

  wfm_compose_state_t *c = wfm_compose_create (&seg, 1u, 0, 0);
  if (!c)
    return 1;
  float complex out[NS];
  size_t n = 0, got_n;
  while (n < NS && (got_n = wfm_compose_execute (c, out + n, NS - n)) > 0)
    n += got_n;
  wfm_compose_destroy (c);

  /* The samples carry the DESCRIPTION's bits. `wfm_frame_assemble` builds
     them independently; bpsk_map's convention is 0 -> +1, 1 -> -1, so with a
     clean rectangular source at zero offset the sign is the bit. */
  uint8_t want[NBITS], got[NBITS];
  if (wfm_frame_assemble (&d, NULL, want, NBITS) != NBITS)
    return 1;
  for (unsigned i = 0; i < NBITS; i++)
    got[i] = crealf (out[i * SPS]) < 0.0f ? 1u : 0u;

  printf ("%zu samples, %u frame bits, match: %s\n", n, NBITS,
          memcmp (got, want, NBITS) == 0 ? "yes" : "NO");
  return memcmp (got, want, NBITS) == 0 ? 0 : 1;
}
```

```text
224 samples, 56 frame bits, match: yes
```

## What the description bought

Three things, none of which a flag offers:

**A field at a position you choose.** The flat fields offer a preamble, a
sync word and a payload, in that order. `hdr` above is neither of the first
two and sits ahead of the payload because the description says so.

**A cover that is named, not counted.** `wfm_frame_add_stage(&d, WFM_STAGE_CRC16, "payload", "crc")` says what three integers used to. The
header is deliberately *outside* the CRC's cover — a receiver has to find the
header before it can check anything — and that choice is one argument, not an
offset arithmetic exercise.

**A kind that is open.** `wfm_stage_kind_t` stops at `WFM_STAGE_USER = 0x1000`; above that the kinds are yours, and the kernel comes in through
`wfm_frame_ops_t`. A transform doppler has never heard of is a stage, not a
pull request against a header.

## The flags are sugar for exactly this

`wfm_source_describe_frame()` is the one place every consumer funnels
through, whichever way a source spelled its frame. Ask a flag-spelled source
for its description, hand that description to a second source, and the two
compose **byte-identically** — which is what makes "the flat fields are
sugar" a measurement rather than a claim.

The worked version of that check, and four others, is
[`native/examples/wfmgen_frame_demo.c`](https://github.com/doppler-dsp/doppler/blob/main/native/examples/wfmgen_frame_demo.c):
it composes framed against unframed, demodulates the frame back to the
description's own bits, shows one description cycling to fill a three-frame
record, and proves the sugar equivalence above. It self-validates and exits
non-zero if any of it stops holding.

```sh
make build
./build/native/examples/wfmgen_frame_demo
```

## Reaching it from the other interfaces

The C struct is the primary interface, and the other two carry the same
description rather than reimplementing it — see
[Scenes: a frame the caller built](../guide/wfmgen/scenes.md#a-frame-the-caller-built)
for the scene JSON's `frame` key and what Python does with it.

## Related pages

- [A CCSDS CADU](ccsds-link.md) — the description on its own: what a real
    standard's fields and covers look like, and why the marker is covered by
    the inner code and by neither the outer code nor the randomiser.
- [Name Your Own Code](coding.md) — an outer and an inner code that are
    nobody's standard, run end to end.
- [5-Burst DSSS Link](dsss-burst-pipeline.md) — the flag-spelled frame,
    through every wfmgen production path.
