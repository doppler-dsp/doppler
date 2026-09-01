/**
 * wfmgen_frame_demo.c — wfmgen eats a frame YOU built.
 *
 * Every other way into wfmgen's framing spells the frame with FLAGS:
 * `--sync`, `--acq-code`, `--crc`, `--rs-depth`, `--asm`. Between them they
 * cover the frames doppler already knows, at the positions doppler already
 * puts them — and a frame outside that shape needed a new flag, which is one
 * more spelling of a layout `wfm_frame_desc_t` could already describe.
 *
 * `wfm_source_t.frame` is the way in. A caller builds a description — named
 * fields in wire order, named stages with the span each covers — points a
 * source at it, and composes. The flat fields stay, as SUGAR that builds one
 * of these, so every scene, flag and JSON key written before this keeps
 * working unchanged.
 *
 * What this demonstrates, in order:
 *
 *   1. A layout no flag spells: 16 bits of the caller's own header, then the
 *      payload, then a CRC-16 a stage derives over a span it NAMES.
 *   2. The frame reaches the SAMPLES — a framed source and an otherwise
 *      identical unframed one do not compose to the same waveform.
 *   3. The samples carry the DESCRIPTION's bits: demodulated back, they are
 *      wfm_frame_assemble() of the same description, bit for bit.
 *   4. The frame CYCLES, so one description fills whatever length is asked
 *      for and a one-frame description is a multi-frame record.
 *   5. The flags really are sugar: wfm_source_describe_frame() turns a
 *      flag-spelled source into a description, and a second source carrying
 *      that description composes BYTE-IDENTICALLY to the first.
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

#include <complex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wfm/wfm_compose.h>
#include <wfm/wfm_frame.h>
#include <wfm_synth/wfm_synth_core.h>

#define FS 1.0e6 /* sample rate, Hz */
#define SPS 4    /* samples per symbol; rectangular, so a symbol is 4 copies */

/* The frame, stated once. Every length below is DERIVED from these, so a
   change here moves the checks with it rather than leaving them pinned to a
   number that used to be right. */
#define HDR_BITS 16u
#define PAYLOAD_BITS 24u
#define FRAME_BITS (HDR_BITS + PAYLOAD_BITS + WFM_FRAME_CRC_BITS)

/* Three whole frames, so section 4 has something to compare frame 2 against.
   A partial frame would still be legal — the description cycles and the
   record simply stops mid-frame — but it would make the cycling check read
   as an accident of the length. */
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

  wfm_compose_state_t *c = wfm_compose_create (&seg, 1u, 0, 0);
  if (!c)
    return 0;

  size_t total = 0;
  for (;;)
    {
      size_t got = wfm_compose_execute (c, out + total, n - total);
      if (got == 0 || total >= n)
        break;
      total += got;
    }
  wfm_compose_destroy (c);
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
  src.payload      = literal (payload_bits, PAYLOAD_BITS);
  src.modulation   = 1; /* bpsk */
  src.sps          = SPS;
  src.snr          = WFM_SYNTH_SNR_CLEAN; /* >= 100 dB: AWGN skipped */
  src.snr_mode     = 1;                   /* fs */
  src.seed         = 1u;
  return src;
}

int
main (void)
{
  printf ("=== wfmgen eats a frame you built (the C caller's view) ===\n\n");

  /* Borrowed by everything below, so they outlive every compose call. */
  static uint8_t hdr_bits[HDR_BITS];
  static uint8_t payload_bits[PAYLOAD_BITS];
  unpack (0x5C5Cu, HDR_BITS, hdr_bits);
  for (unsigned i = 0; i < PAYLOAD_BITS; i++)
    payload_bits[i] = (uint8_t)((i * 7u + 1u) & 1u);

  /* ── 1. A layout no flag spells ─────────────────────────────────────── */
  printf ("--- 1. The description: named fields, a stage over a named span "
          "---\n");

  wfm_frame_desc_t d;
  memset (&d, 0, sizeof d);
  wfm_seq_t hdr = literal (hdr_bits, HDR_BITS);
  wfm_seq_t pay = literal (payload_bits, PAYLOAD_BITS);

  int ok = wfm_frame_add_field (&d, "hdr", &hdr, 0u) == 0
           && wfm_frame_add_field (&d, "payload", &pay, 0u) == 1
           && wfm_frame_add_derived (&d, "crc", WFM_FRAME_CRC_BITS) == 2
           /* The cover names its ends, and it REACHES the derived field:
              a code occupies its information and the check symbols it
              derives, so "payload".."crc" is one declaration of both. That
              is also what wires the derived field's producer, so nothing
              states it a second, disagreeing way. The header is deliberately
              outside the cover — a receiver finds it before it can check
              anything. */
           && wfm_frame_add_stage (&d, WFM_STAGE_CRC16, "payload", "crc") == 0;
  check (ok, "three named fields and one named cover build a description");
  if (!ok)
    return 1;

  wfm_frame_desc_layout_t lay;
  check (wfm_frame_desc_layout (&d, &lay) == 0,
         "wfm_frame_desc_layout accepts it");
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

  wfm_source_t plain = bits_source (payload_bits);
  check (!wfm_source_has_frame (&plain),
         "the source is unframed before a description is attached");

  wfm_source_t src = plain;
  src.frame        = &d;
  check (wfm_source_has_frame (&src),
         "carrying a description IS what makes a source framed");
  check (wfm_source_frame_error (&src) == NULL,
         "this type can honour a frame (type=bits, explicit payload)");

  size_t n_framed   = compose_one (&src, framed, TOTAL);
  size_t n_unframed = compose_one (&plain, unframed, TOTAL);
  check (n_framed == TOTAL && n_unframed == TOTAL,
         "both scenes compose the length their geometry declares");
  check (memcmp (framed, unframed, TOTAL * sizeof *framed) != 0,
         "framed and unframed are DIFFERENT waveforms");
  printf ("\n");

  /* ── 3. The samples carry the description's own bits ────────────────── */
  printf ("--- 3. The bits on the wire are the description's ---\n");

  uint8_t want[FRAME_BITS];
  size_t  n_bits = wfm_frame_assemble (&d, NULL, want, FRAME_BITS);
  check (n_bits == FRAME_BITS,
         "wfm_frame_assemble materialises the description independently");

  uint8_t got[FRAME_BITS];
  demod (framed, FRAME_BITS, got);
  check (n_bits == FRAME_BITS && memcmp (got, want, FRAME_BITS) == 0,
         "demodulated, the first frame IS wfm_frame_assemble's output");

  /* The header is the half no flag could have placed, so name it. */
  check (memcmp (got, hdr_bits, HDR_BITS) == 0,
         "the caller's own 16-bit header leads the frame, as described");
  printf ("  frame bits: ");
  for (unsigned i = 0; i < FRAME_BITS; i++)
    printf ("%u", got[i]);
  printf ("\n\n");

  /* ── 4. One description, however long the record ────────────────────── */
  printf ("--- 4. The frame cycles to fill the segment ---\n");

  int cycles = 1;
  for (unsigned f = 1; f < FRAMES; f++)
    {
      uint8_t next[FRAME_BITS];
      demod (framed + (size_t)f * FRAME_BITS * SPS, FRAME_BITS, next);
      if (memcmp (next, want, FRAME_BITS) != 0)
        cycles = 0;
    }
  check (cycles, "frames 1 and 2 repeat frame 0, bit for bit");
  printf ("  %u frames x %u bits x %d sps = %zu samples from ONE "
          "description\n\n",
          FRAMES, FRAME_BITS, SPS, TOTAL);

  /* ── 5. The flat flags are sugar for exactly this ───────────────────── */
  printf ("--- 5. The flags build a description; so can you ---\n");

  /* Spelled the old way: a sync word, a payload, and a CRC trailer. */
  wfm_source_t flags = bits_source (payload_bits);
  flags.sync         = literal (hdr_bits, HDR_BITS);
  flags.crc          = 1;
  check (wfm_source_has_frame (&flags), "the flat fields frame it too");

  /* And read back OUT as a description — the one every consumer funnels
     through, whichever way the source spelled its frame. */
  wfm_frame_desc_t from_flags;
  check (wfm_source_describe_frame (&flags, &from_flags) == 0,
         "wfm_source_describe_frame turns the flags into a description");

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
