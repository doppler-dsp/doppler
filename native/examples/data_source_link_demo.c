/**
 * @file data_source_link_demo.c
 * @brief A message over DSSS, received and deframed.
 *
 * The C twin of `src/doppler/examples/data_source_link_demo.py`. A message
 * is split into 48-bit chunks, one chunk per frame, and each frame is
 * spread into a DSSS burst; the receiver finds every burst; the SAME frame
 * description the transmitter was given then checks each frame and returns
 * its chunk. The message comes back from the capture alone -- no
 * hand-slicing of bit positions, no second copy of the frame layout.
 *
 * What it shows, in order:
 *
 *  1. transmit: the message's bits are the source's `data`; the
 *     description `[sync | data:48 | crc16]` says how each frame is made;
 *     the short last chunk is padded from `fill`. The run is exactly the
 *     frames' length;
 *  2. receive: the receiver is built from the description, finds every
 *     burst at its exact sample, and every frame's CRC-16 checks;
 *  3. deframe: the chunks reproduce the message byte for byte, and the
 *     spare bits of the last frame are the fill;
 *  4. the check is a real detector: one flipped payload bit fails it.
 *
 * Every claim is a check; the program exits non-zero if any fails.
 *
 * Build and run (from the repo root, after `make build`):
 *
 *     ./build/native/examples/data_source_link_demo
 */
#include <doppler/cvt/cvt_core.h>
#include <doppler/dsss_burst_receiver/dsss_burst_receiver_core.h>
#include <doppler/frame/frame_core.h>
#include <doppler/wfm/wfm_compose.h>

#include "doppler/dp_complex.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHUNK 48u
#define REPS 5u
#define SPC 2u
#define CHIP_RATE 1.0e6
#define NCAP_MAX 400000u

static int failures = 0;

static void
check (int ok, const char *what)
{
  printf ("  [%s] %s\n", ok ? "ok" : "FAIL", what);
  if (!ok)
    failures++;
}

/* Bits of a field spec ("pn:255:8:1", "0000011001010"): size, then render.
   The caller frees. */
static uint8_t *
field (const char *spec, size_t *n)
{
  const char *why = NULL;
  *n              = dp_wfm_field_bits (spec, NULL, 0, &why);
  uint8_t *b      = *n ? malloc (*n) : NULL;
  if (!b || dp_wfm_field_bits (spec, b, *n, NULL) != *n)
    {
      fprintf (stderr, "field %s: %s\n", spec, why ? why : "failed");
      free (b);
      return NULL;
    }
  return b;
}

int
main (void)
{
  size_t   n_acq, n_dat, n_sync;
  uint8_t *acq  = field ("pn:255:8:1", &n_acq);
  uint8_t *dat  = field ("pn:31:5:3", &n_dat);
  uint8_t *sync = field ("0000011001010", &n_sync); /* Barker-13, inverted */
  if (!acq || !dat || !sync)
    return 1;

  static const char message[] = "a message, one chunk per DSSS burst";
  const size_t      nmsg      = sizeof message - 1u;
  const size_t      nbits     = 8u * nmsg;
  uint8_t          *bits      = malloc (nbits);
  if (!bits)
    return 1;
  (void)dp_bytes_to_bin ((const uint8_t *)message, nmsg, bits, nbits,
                         DP_BITORDER_BIG); /* MSB first, as a file is read */
  static const uint8_t fill[2] = { 0, 1 }; /* pads the short last frame */

  /* The frame, once: a sync word, one chunk of the source, a CRC-16 over
     it. The frame state owns the description both sides read. */
  dp_frame_state_t *d = dp_frame_create_desc (NULL, 0, NULL, 0, NULL, 0, 0);
  if (!d || dp_frame_add_field (d, "sync", sync, n_sync) < 0
      || dp_frame_add_data (d, "payload", CHUNK) < 0
      || dp_frame_add_derived (d, "crc", 16) < 0
      || dp_frame_add_stage_over (d, WFM_STAGE_CRC16, "payload", "crc", 0, 0)
             < 0
      || dp_frame_build (d) != 0)
    {
      fprintf (stderr, "the frame does not lay out\n");
      return 1;
    }
  const size_t NB = d->nbits;

  const char                     *why = NULL;
  dp_dsss_burst_receiver_state_t *rx  = dp_dsss_burst_receiver_create_desc (
      acq, n_acq, dat, n_dat, &d->d, REPS, SPC, CHIP_RATE, 60.0, 0.0, 1e-3,
      0.9, 0.0, 0.0, 10, &why);
  if (!rx)
    {
      fprintf (stderr, "receiver: %s\n", why ? why : "create failed");
      return 1;
    }
  const size_t min_gap = dp_dsss_burst_receiver_get_min_gap (rx);

  wfm_source_t src   = { 0 };
  src.type           = WFM_SYNTH_DSSS;
  src.sps            = (int)SPC;
  src.snr            = 12.0;
  src.snr_mode       = 3; /* esno */
  src.seed           = 1u;
  src.acq_code.bits  = acq;
  src.acq_code.len   = n_acq;
  src.acq_reps       = REPS;
  src.data_code.bits = dat;
  src.data_code.len  = n_dat;
  src.data
      = (wfm_seq_t){ .kind = WFM_SEQ_LITERAL, .bits = bits, .len = nbits };
  src.data_len = CHUNK;
  src.fill  = (wfm_seq_t){ .kind = WFM_SEQ_LITERAL, .bits = fill, .len = 2 };
  src.frame = &d->d; /* borrowed: d outlives the source */

  wfm_segment_t seg = { 0 };
  seg.sources       = &src;
  seg.n_sources     = 1u;
  seg.fs            = CHIP_RATE * SPC;
  seg.off_samples   = min_gap; /* dead air after the train */

  if (dp_wfm_scene_error (&seg, 1, 0, 0))
    {
      fprintf (stderr, "refused: %s\n", dp_wfm_scene_error (&seg, 1, 0, 0));
      return 1;
    }
  dp_wfm_compose_state_t *c = dp_wfm_compose_create (&seg, 1u, 0, 0);
  float complex          *x = malloc (NCAP_MAX * sizeof *x);
  if (!c || !x)
    return 1;
  size_t nx = 0, got;
  while (nx < NCAP_MAX
         && (got = dp_wfm_compose_execute (c, x + nx, NCAP_MAX - nx)) > 0)
    nx += got;
  dp_wfm_compose_destroy (c);

  const size_t frames = (nbits + CHUNK - 1u) / CHUNK;
  const size_t burst  = (REPS * n_acq + NB * n_dat) * SPC;
  printf ("%zu bits -> %zu frames of %zu bits, %zu samples\n", nbits, frames,
          NB, nx);
  check (nx == frames * burst + min_gap,
         "1. the run is exactly the bursts and the dead air, no "
         "num_samples");

  const size_t cap_out = dp_dsss_burst_receiver_push_max_out (rx, nx);
  uint8_t     *out     = malloc (cap_out ? cap_out : 1u);
  if (!out)
    return 1;
  const size_t nout   = dp_dsss_burst_receiver_push (rx, x, nx, out, cap_out);
  size_t       nev    = dp_dsss_burst_receiver_events_max_out (rx);
  dsss_br_event_t *ev = malloc ((nev ? nev : 1u) * sizeof *ev);
  if (!ev)
    return 1;
  nev = dp_dsss_burst_receiver_events (rx, nev, ev, nev);

  size_t valid = 0;
  int    exact = 1;
  for (size_t i = 0; i < nev; i++)
    {
      if (!ev[i].frame_valid)
        continue;
      if (ev[i].preamble_start != (uint64_t)(valid * burst))
        exact = 0;
      valid++;
    }
  check (valid == frames && nout == frames * NB, "2. every burst decoded");
  check (exact && valid == frames,
         "2. every burst is found at its exact sample");

  /* Deframe through the SAME description: the payload is found by name. */
  const size_t off
      = dp_frame_field_off (d, (size_t)dp_frame_field_index (d, "payload"));
  uint8_t *un   = malloc (dp_frame_deframe_max_out (d, 0));
  uint8_t *gotb = malloc (frames * CHUNK);
  if (!un || !gotb)
    return 1;
  int crcs = 1;
  for (size_t k = 0; k < frames && (k + 1u) * NB <= nout; k++)
    {
      uint8_t frame[256]; /* a copy: checking repairs in place */
      memcpy (frame, out + k * NB, NB);
      frame_check_t v = dp_frame_check (d, frame, NB);
      if (!(v.passed && v.passed == (int)v.checked))
        crcs = 0;
      (void)dp_frame_deframe (d, out + k * NB, NB, un,
                              dp_frame_deframe_max_out (d, 0));
      memcpy (gotb + k * CHUNK, un + off, CHUNK);
    }
  check (crcs, "2. every frame's CRC-16 checks");

  check (memcmp (gotb, bits, nbits) == 0,
         "3. the chunks are the message's bits");
  uint8_t back[64] = { 0 };
  for (size_t i = 0; i < nbits; i++)
    back[i / 8u] |= (uint8_t)(gotb[i] << (7u - i % 8u));
  check (memcmp (back, message, nmsg) == 0,
         "3. the message comes back byte for byte");
  int spare = 1;
  for (size_t i = nbits; i < frames * CHUNK; i++)
    if (gotb[i] != fill[(i - nbits) % 2u])
      spare = 0;
  check (spare, "3. the last frame's spare bits are the fill");

  uint8_t bad[256];
  memcpy (bad, out, NB);
  bad[off] ^= 1u;
  check (!dp_frame_check (d, bad, NB).passed,
         "4. a flipped payload bit fails the check");

  free (un);
  free (gotb);
  free (ev);
  free (out);
  free (x);
  dp_dsss_burst_receiver_destroy (rx);
  dp_frame_destroy (d);
  free (bits);
  free (acq);
  free (dat);
  free (sync);
  printf ("%s\n", failures ? "FAILED" : "all checks passed");
  return failures ? 1 : 0;
}
