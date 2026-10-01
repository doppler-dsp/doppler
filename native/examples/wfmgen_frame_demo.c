/**
 * wfmgen_frame_demo.c — wfmgen eats a frame YOU built.
 *
 * The flags `--acq-code`, `--sync` and `--crc` spell ONE frame, the common
 * one: `[preamble x reps | sync | payload | crc]`. Anything else — a field
 * of your own, a coding stage, a CCSDS CADU — is a description, and
 * `wfm_source_t.frame` is the way in. A caller builds one — named fields in
 * wire order, named stages with the span each covers — points a source at
 * it, and composes. `wfmgen --frame FILE` and a scene's "frame" key reach
 * the same member.
 *
 * What this demonstrates, in order:
 *
 *   1. A layout no flag spells: 16 bits of the caller's own header, then the
 *      payload -- a data:LEN field the source's data fills -- then a CRC-16 a
 *      stage derives over a span it NAMES.
 *   2. The frame reaches the SAMPLES — a framed source and the same data
 *      sent unframed do not compose to the same waveform.
 *   3. The samples carry the DESCRIPTION's bits: demodulated back, the first
 *      frame is dp_wfm_frame_assemble_data() over the first chunk.
 *   4. One description, a multi-frame record: each frame carries the NEXT
 *      chunk of the data, and the run is exactly those frames -- nothing is
 *      cycled (doppler#1718).
 *   5. The common frame is a description too: a flag-spelled source and a
 *      second one carrying dp_wfm_frame_fixed() of the same fields compose
 *      BYTE-IDENTICALLY.
 *   6. A stage kind that is YOURS: a kind from WFM_STAGE_USER up, its kernel
 *      supplied through wfm_frame_ops_t, refused when absent and reversed
 *      through the same open lookup when present.
 *
 * Ownership, which is the one thing easy to get wrong here: a description is
 * BORROWED by the source, and the sequences inside it are borrowed in turn.
 * Everything the composer reads must outlive the compose call — which is why
 * the description and its bit arrays below live in `main`, not in the helper
 * that fills them.
 *
 * Every check is explicit and returns non-zero on failure. `assert()` is
 * deliberately not used: examples build Release, where NDEBUG would compile
 * the checks out and leave a demo that validates nothing while still exiting
 * 0 — the exact shape `make test-examples-c` exists to prevent.
 *
 * Build:
 *   make build
 *   ./build/native/examples/wfmgen_frame_demo
 */

#include "doppler/dp_complex.h"
#include <doppler/wfm/wfm_compose.h>
#include <doppler/wfm/wfm_frame.h>
#include <doppler/wfm_synth/wfm_synth_core.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FS 1.0e6 /* sample rate, Hz */
#define SPS 4    /* samples per symbol; rectangular, so a symbol is 4 copies */

/* The frame, stated once. Every length below is DERIVED from these, so a
   change here moves the checks with it rather than leaving them pinned to a
   number that used to be right. */
#define HDR_BITS 16u
#define PAYLOAD_BITS 24u
#define FRAME_BITS (HDR_BITS + PAYLOAD_BITS + WFM_FRAME_CRC_BITS)

/* The data: three chunks, so the description makes three frames, each
   carrying its own chunk (section 4). */
#define FRAMES 3u
#define TOTAL ((size_t)FRAME_BITS * SPS * FRAMES)

static int failures = 0;

/** @brief Report one named check; the first failure sets the exit status. */
static void
check (int ok, const char *what)
{
  printf ("  [%s] %s\n", ok ? "ok" : "FAIL", what);
  if (!ok)
    failures++;
}

/** @brief A `wfm_seq_t` over bits the CALLER owns and keeps. */
static wfm_seq_t
literal (const uint8_t *bits, size_t len)
{
  wfm_seq_t s = { 0 };
  s.kind      = WFM_SEQ_LITERAL;
  s.bits      = bits;
  s.len       = len;
  return s;
}

/** @brief Unpack the low @p n bits of @p v, MSB first, one bit per byte. */
static void
unpack (uint64_t v, unsigned n, uint8_t *out)
{
  for (unsigned i = 0; i < n; i++)
    out[i] = (uint8_t)((v >> (n - 1u - i)) & 1u);
}

/**
 * @brief Recover the bits a clean, rectangular BPSK stream carries.
 *
 * Legitimate only because the source is CLEAN (snr >= WFM_SYNTH_SNR_CLEAN, so
 * no AWGN), rectangular (`pulse` left 0, so no filter delay to hunt for) and
 * at zero offset (`freq` left 0, so no carrier rotation): symbol `i` is then
 * literally samples `[i*sps, (i+1)*sps)` and one of them is the whole story.
 * `bpsk_map`'s convention is 0 -> +1, 1 -> -1, so the SIGN is the bit.
 */
static void
demod (const float complex *x, size_t nbits, uint8_t *bits)
{
  for (size_t i = 0; i < nbits; i++)
    bits[i] = crealf (x[i * (size_t)SPS]) < 0.0f ? 1u : 0u;
}

/**
 * @brief Compose one single-source segment into @p out.
 *
 * @param src  borrowed for the whole call, along with anything it points at.
 * @return samples written, or 0 if the composer could not be built.
 */
static size_t
compose_one (const wfm_source_t *src, float complex *out, size_t n)
{
  wfm_segment_t seg = { 0 };
  /* The cast is the API's shape, not a const violation: `sources` is the
     mutable list a caller usually owns, and create() only reads it. */
  seg.sources     = (wfm_source_t *)src;
  seg.n_sources   = 1u;
  seg.fs          = FS;
  seg.num_samples = n;
  seg.off_samples = 0u;
  seg.gap_noise   = 0;

  dp_wfm_compose_state_t *c = dp_wfm_compose_create (&seg, 1u, 0, 0);
  if (!c)
    return 0;

  size_t total = 0;
  for (;;)
    {
      size_t got = dp_wfm_compose_execute (c, out + total, n - total);
      if (got == 0 || total >= n)
        break;
      total += got;
    }
  dp_wfm_compose_destroy (c);
  return total;
}

/** @brief The source every section starts from: clean, rectangular, BPSK. */
static wfm_source_t
bits_source (const uint8_t *payload_bits)
{
  /* `= { 0 }` then named fields, never a positional initialiser list:
     wfm_source_t carries 30-odd members and a positional list silently
     shifts the moment one is inserted. */
  wfm_source_t src = { 0 };
  src.type         = WFM_SYNTH_BITS;
  src.data         = literal (payload_bits, FRAMES * PAYLOAD_BITS);
  src.data_len     = PAYLOAD_BITS; /* a frame's data:LEN, one chunk */
  src.modulation   = 1;            /* bpsk */
  src.sps          = SPS;
  src.snr          = WFM_SYNTH_SNR_CLEAN; /* >= 100 dB: AWGN skipped */
  src.snr_mode     = 1;                   /* fs */
  src.seed         = 1u;
  return src;
}

/* ── A stage kind doppler has never heard of ────────────────────────────────
 *
 * `wfm_stage_kind_t` stops at WFM_STAGE_USER = 0x1000, and the header says
 * why: above it the kinds are the CALLER's, so "a mission that is not CCSDS"
 * is a configuration rather than a pull request against wfm_frame.h. Section
 * 6 is that sentence, executed.
 *
 * The transform is a whitener — XOR the span against a fixed pattern — for
 * two reasons. It is what a real randomiser stage does, and it is its own
 * inverse, so `undo` is the same function and the RECEIVE half of the claim
 * costs no extra kernel to show.
 */
#define MY_WHITEN (WFM_STAGE_USER + 1u)

/** @brief The pattern, one bit per byte, repeating over the span. */
static uint8_t
whiten_bit (size_t i)
{
  /* An 8-bit period the eye can check against the printed bits below; a real
     randomiser uses an LFSR, and `WFM_STAGE_RANDOMISE` is that one. */
  static const uint8_t pattern[8] = { 1, 1, 0, 1, 0, 0, 1, 0 };
  return pattern[i % 8u];
}

/**
 * @brief XOR the stage's span in place. Its own inverse, hence used for both.
 *
 * @param st    the stage as declared; unused here, but a real kernel reads
 *              `depth` / `unit_bits` from it rather than from a global.
 * @param bits  the whole span, one bit per byte.
 * @param n     bits in the span.
 * @param user  the ops table's `user` pointer; NULL here.
 */
static int
whiten_in_unit (const wfm_stage_t *st, uint8_t *bits, size_t n, void *user)
{
  (void)st;
  (void)user;
  for (size_t i = 0; i < n; i++)
    bits[i] ^= whiten_bit (i);
  return 0;
}

/** @brief Undo has the wider signature; the transform is the same one. */
static int
whiten_undo (const wfm_stage_t *st, uint8_t *bits, size_t n,
             wfm_frame_stage_rx_t *rx, void *user)
{
  whiten_in_unit (st, bits, n, user);
  /* A whitener repairs nothing, so it reverses exactly one unit and
     corrects none. Saying so is what keeps `checked` honest: a stage that
     reported nothing would be indistinguishable from one with no undo. */
  if (rx)
    {
      rx->units     = 1u;
      rx->ok        = 1u;
      rx->corrected = 0u;
      rx->symbols   = 0u;
      rx->checked   = 1;
    }
  return 0;
}

/* ONE entry, not three. The table EXTENDS the built-ins rather than
   replacing them, so supplying a new kind does not mean restating the CRC —
   which is what makes an open kind cheap enough to actually use. */
static const wfm_stage_op_t my_ops_table[] = {
  { MY_WHITEN, whiten_in_unit, NULL, whiten_undo },
};

/** @brief The caller's kernels, as `dp_wfm_frame_assemble` and `_check` take
 * them. */
static wfm_frame_ops_t
my_ops (void)
{
  wfm_frame_ops_t o = { 0 };
  o.op              = my_ops_table;
  o.n_op            = (unsigned)(sizeof my_ops_table / sizeof *my_ops_table);
  o.user            = NULL;
  return o;
}

int
main (void)
{
  printf ("=== wfmgen eats a frame you built (the C caller's view) ===\n\n");

  /* Borrowed by everything below, so they outlive every compose call. */
  static uint8_t hdr_bits[HDR_BITS];
  static uint8_t payload_bits[FRAMES * PAYLOAD_BITS]; /* three chunks */
  unpack (0x5C5Cu, HDR_BITS, hdr_bits);
  for (unsigned i = 0; i < FRAMES * PAYLOAD_BITS; i++)
    payload_bits[i] = (uint8_t)(((i * 7u + 1u) ^ (i / 5u)) & 1u);

  /* ── 1. A layout no flag spells ─────────────────────────────────────── */
  printf ("--- 1. The description: named fields, a stage over a named span "
          "---\n");

  wfm_frame_desc_t d;
  memset (&d, 0, sizeof d);
  wfm_seq_t hdr = literal (hdr_bits, HDR_BITS);
  /* The payload is a data:LEN field: the description says WHERE the data
     goes and how much of it a frame takes, never what it is. */
  const wfm_seq_t dq = { .kind = WFM_SEQ_DATA, .len = PAYLOAD_BITS };

  int ok
      = dp_wfm_frame_add_field (&d, "hdr", &hdr, 0u) == 0
        && dp_wfm_frame_add_field (&d, "payload", &dq, 0u) == 1
        && dp_wfm_frame_add_derived (&d, "crc", WFM_FRAME_CRC_BITS) == 2
        /* The cover names its ends, and it REACHES the derived field:
           a code occupies its information and the check symbols it
           derives, so "payload".."crc" is one declaration of both. That
           is also what wires the derived field's producer, so nothing
           states it a second, disagreeing way. The header is deliberately
           outside the cover — a receiver finds it before it can check
           anything. */
        && dp_wfm_frame_add_stage (&d, WFM_STAGE_CRC16, "payload", "crc") == 0;
  check (ok, "three named fields and one named cover build a description");
  if (!ok)
    return 1;

  wfm_frame_desc_layout_t lay;
  check (dp_wfm_frame_desc_layout (&d, &lay) == 0,
         "dp_wfm_frame_desc_layout accepts it");
  check (lay.frame_bits == FRAME_BITS,
         "the frame is header + payload + CRC, exactly");
  printf ("  hdr     %2zu bits @ %2zu\n", lay.field_bits[0], lay.field_off[0]);
  printf ("  payload %2zu bits @ %2zu\n", lay.field_bits[1], lay.field_off[1]);
  printf ("  crc     %2zu bits @ %2zu  (derived by the stage below)\n",
          lay.field_bits[2], lay.field_off[2]);
  printf ("  stage 0 covers [%zu, %zu) — the payload and its check bits\n\n",
          lay.stage[0].first, lay.stage[0].first + lay.stage[0].n);

  /* ── 2. The frame reaches the samples ───────────────────────────────── */
  printf ("--- 2. A carried frame changes the waveform ---\n");

  float complex *framed   = calloc (TOTAL, sizeof *framed);
  float complex *unframed = calloc (TOTAL, sizeof *unframed);
  float complex *sugar    = calloc (TOTAL, sizeof *sugar);
  float complex *relayed  = calloc (TOTAL, sizeof *relayed);
  if (!framed || !unframed || !sugar || !relayed)
    {
      fprintf (stderr, "wfmgen_frame_demo: out of memory\n");
      free (framed);
      free (unframed);
      free (sugar);
      free (relayed);
      return 1;
    }

  /* The same data, sent as given: no frame, no check. */
  wfm_source_t plain = bits_source (payload_bits);
  plain.data_len     = 0;
  plain.crc          = 0;

  /* The description is the WHOLE frame; the source's data fills its
     data:LEN field, one chunk a frame. */
  wfm_source_t src = bits_source (payload_bits);
  src.frame        = &d;
  check (dp_wfm_source_has_frame (&src),
         "carrying a description IS what makes a source framed");
  check (dp_wfm_source_error (&src) == NULL,
         "this type can honour a frame (type=bits, its data in a field)");

  size_t n_framed   = compose_one (&src, framed, TOTAL);
  size_t n_unframed = compose_one (&plain, unframed, TOTAL);
  check (n_framed == TOTAL
             && n_unframed == (size_t)FRAMES * PAYLOAD_BITS * SPS,
         "each composes the length its data declares: 3 frames, or the "
         "bare 72 bits");
  check (memcmp (framed, unframed, n_unframed * sizeof *framed) != 0,
         "framed and unframed are DIFFERENT waveforms");
  printf ("\n");

  /* ── 3. The samples carry the description's own bits ────────────────── */
  printf ("--- 3. The bits on the wire are the description's ---\n");

  uint8_t want[FRAME_BITS];
  size_t  n_bits
      = dp_wfm_frame_assemble_data (&d, NULL, payload_bits, want, FRAME_BITS);
  check (n_bits == FRAME_BITS, "dp_wfm_frame_assemble_data materialises the "
                               "first frame independently");

  uint8_t got[FRAME_BITS];
  demod (framed, FRAME_BITS, got);
  check (n_bits == FRAME_BITS && memcmp (got, want, FRAME_BITS) == 0,
         "demodulated, the first frame IS dp_wfm_frame_assemble_data's "
         "output over the first chunk");

  /* The header is the half no flag could have placed, so name it. */
  check (memcmp (got, hdr_bits, HDR_BITS) == 0,
         "the caller's own 16-bit header leads the frame, as described");
  printf ("  frame bits: ");
  for (unsigned i = 0; i < FRAME_BITS; i++)
    printf ("%u", got[i]);
  printf ("\n\n");

  /* ── 4. One description, a multi-frame record ───────────────────────── */
  printf ("--- 4. Each frame carries the next chunk of the data ---\n");

  int chunks = 1;
  for (unsigned f = 1; f < FRAMES; f++)
    {
      uint8_t next[FRAME_BITS], wantf[FRAME_BITS];
      demod (framed + (size_t)f * FRAME_BITS * SPS, FRAME_BITS, next);
      if (dp_wfm_frame_assemble_data (&d, NULL,
                                      payload_bits + (size_t)f * PAYLOAD_BITS,
                                      wantf, FRAME_BITS)
              != FRAME_BITS
          || memcmp (next, wantf, FRAME_BITS) != 0)
        chunks = 0;
    }
  check (chunks, "frames 1 and 2 are the description over chunks 1 and 2, "
                 "each with its own CRC -- not frame 0 again");
  printf ("  %u chunks -> %u frames x %u bits x %d sps = %zu samples from "
          "ONE description\n\n",
          FRAMES, FRAMES, FRAME_BITS, SPS, TOTAL);

  /* ── 5. The common frame is a description too ───────────────────────── */
  printf ("--- 5. The flags spell the common frame; so can you ---\n");

  /* Spelled the old way: a sync word, a payload, and a CRC trailer. */
  wfm_source_t flags = bits_source (payload_bits);
  flags.sync         = literal (hdr_bits, HDR_BITS);
  flags.crc          = 1;
  check (dp_wfm_source_has_frame (&flags), "the flat fields frame it too");

  /* The same frame, as the description dp_wfm_frame_fixed() builds — the
     one function the flags go through, so there is no second layout. */
  wfm_frame_desc_t from_flags;
  check (dp_wfm_frame_fixed (&from_flags, NULL, 0, &flags.sync, &dq, flags.crc)
             == 0,
         "dp_wfm_frame_fixed describes the common frame");

  wfm_source_t carried = bits_source (payload_bits);
  carried.frame        = &from_flags;

  size_t n_sugar   = compose_one (&flags, sugar, TOTAL);
  size_t n_relayed = compose_one (&carried, relayed, TOTAL);
  check (n_sugar == TOTAL && n_relayed == TOTAL,
         "both compose the declared length");
  check (memcmp (sugar, relayed, TOTAL * sizeof *sugar) == 0,
         "flag-spelled and description-carried are BYTE-IDENTICAL");

  /* And the two descriptions differ, which is what makes section 3's
     header check a demonstration rather than a coincidence: the flat
     fields put a sync word where this description puts a header, and
     nothing in the flags can produce the stage cover built above. */
  check (memcmp (&from_flags, &d, sizeof d) != 0,
         "yet it is not the same description — the flags cannot spell this "
         "one");
  printf ("\n");

  /* ── 6. A stage kind that is YOURS ──────────────────────────────────── */
  printf ("--- 6. A transform doppler has never heard of ---\n");

  /* The same three fields and the same CRC cover, plus one stage of a kind
     no version of doppler will ever allocate. The description does not know
     what MY_WHITEN does and does not need to: it names a kind and a span. */
  /* Its payload is chunk 0's bits written into the description: a frame of
     fixed bits, which section 3's `want` is the plain assembly of. */
  wfm_seq_t        pay = literal (payload_bits, PAYLOAD_BITS);
  wfm_frame_desc_t mine;
  memset (&mine, 0, sizeof mine);
  int built
      = dp_wfm_frame_add_field (&mine, "hdr", &hdr, 0u) == 0
        && dp_wfm_frame_add_field (&mine, "payload", &pay, 0u) == 1
        && dp_wfm_frame_add_derived (&mine, "crc", WFM_FRAME_CRC_BITS) == 2
        && dp_wfm_frame_add_stage (&mine, WFM_STAGE_CRC16, "payload", "crc")
               == 0
        /* Applied AFTER the CRC and over the WHOLE frame, which is
           where a randomiser belongs: the check symbols are whitened
           too, and the receiver unwhitens before it checks. */
        && dp_wfm_frame_add_stage (&mine, MY_WHITEN, "hdr", "crc") == 1;
  check (built, "a description accepts a kind from the caller's own range");
  check (MY_WHITEN > WFM_STAGE_INTERLEAVE,
         "the kind is above every kind doppler names");

  /* Refused, not skipped. This is the half that matters: a stage that
     quietly did not run produces a frame that still assembles, still
     decodes against itself, and syncs to nothing at the far end. */
  uint8_t no_kernel[FRAME_BITS];
  check (dp_wfm_frame_assemble (&mine, NULL, no_kernel, FRAME_BITS) == 0,
         "with no kernel for that kind, assembly REFUSES — never a silent "
         "skip");

  /* Supplied, and it assembles. The table has one entry; the CRC stage
     still runs, because a caller's table extends the built-ins. */
  wfm_frame_ops_t ops = my_ops ();
  uint8_t         theirs[FRAME_BITS];
  size_t n_mine = dp_wfm_frame_assemble (&mine, &ops, theirs, FRAME_BITS);
  check (n_mine == FRAME_BITS,
         "with the kernel supplied, the same description assembles");
  check (memcmp (theirs, want, FRAME_BITS) != 0,
         "and the bits differ from the unwhitened frame — the stage RAN");

  printf ("  plain:    ");
  for (unsigned i = 0; i < FRAME_BITS; i++)
    printf ("%u", want[i]);
  printf ("\n  whitened: ");
  for (unsigned i = 0; i < FRAME_BITS; i++)
    printf ("%u", theirs[i]);
  printf ("\n");

  /* And it REVERSES through the same open lookup, which is the receive half
     of the claim: one description, both directions, a kind neither end of
     doppler has heard of. */
  wfm_frame_rx_t rx;
  memset (&rx, 0, sizeof rx);
  int good = dp_wfm_frame_check (&mine, &ops, theirs, &rx);
  check (good == 1, "dp_wfm_frame_check reverses it and the CRC passes");
  check (rx.checked == 2u, "both stages were reversed here, not one");
  check (memcmp (theirs, want, FRAME_BITS) == 0,
         "unwhitened in place, the frame is the plain one again, bit for "
         "bit");
  printf ("\n");

  free (framed);
  free (unframed);
  free (sugar);
  free (relayed);

  if (failures)
    {
      printf ("=== %d check(s) FAILED ===\n", failures);
      return 1;
    }
  printf ("=== all checks passed ===\n");
  return 0;
}
