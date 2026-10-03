/**
 * @file framed_link_demo.c
 * @brief A framed BPSK / QPSK link, checked by its description.
 *
 * The C twin of `src/doppler/examples/framed_link_demo.py`. wfmgen sends
 * frames built from ONE frame description (a sync word, a payload, a
 * CRC-16); the MPSK receiver recovers the symbols; the SAME description then
 * finds and checks every frame. Nothing about the frame is written twice: the
 * sync word is the description's field 0, the frame length comes from its
 * layout, and the verdict from `dp_wfm_frame_check`.
 *
 * What it shows, in order:
 *
 *  1. Transmit: 200 frames of 48 payload bits at Es/N0 = 14 dB, once as BPSK
 *     and once as QPSK, with a small carrier offset.
 *  2. Receive: the receiver locks and hands back matched-filter symbols. The
 *     carrier loop settles on one of M equivalent phases, so each phase is
 *     tried and the one whose bits repeat the sync word at the frame period
 *     is kept (the sync word resolves the ambiguity, as it does on air).
 *  3. Check: every sliced frame is judged by the description. Every frame
 *     after the lock transient passes, and every passed frame carries exactly
 *     the payload that was sent.
 *  4. The check is a real detector: one flipped bit fails it, and at a low
 *     Es/N0 the damaged frames fail while no damaged frame passes.
 *
 * Every claim is a check; the program exits non-zero if any fails.
 *
 * Build and run (from the repo root, after `make build`):
 *
 *     ./build/native/examples/framed_link_demo
 */
#include "doppler/dp_complex.h"
#include "doppler/dp_syncword.h"
#include <doppler/mpsk/mpsk_core.h>
#include <doppler/mpsk_receiver/mpsk_receiver_core.h>
#include <doppler/wfm/wfm_compose.h>
#include <doppler/wfm/wfm_frame.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINK_SEED 4 /* QPSK's lock transient is seed-sensitive */
#define SPS 8
#define PAYLOAD 48
#define SYNC_BITS 32
#define MAX_NB 256 /* room for one frame; the real length is the layout's */

/* From the description's layout, set once it is built (run_link). */
static size_t NB, PAY_OFF;
#define FRAMES 200
#define SETTLE 12             /* frames the loops may take to settle */
#define WFM_PULSE_RRC_VALUE 1 /* index into PULSE_NAMES: rect, rrc */

static int failures = 0;

static void
check (int ok, const char *what)
{
  printf ("  [%s] %s\n", ok ? "ok" : "FAIL", what);
  if (!ok)
    failures++;
}

/* The sync word 0x1ACFFC1D, MSB first: field 0 of the description. */
static uint8_t sync_bits[SYNC_BITS];

static uint32_t rng_state = 3u;

static uint32_t
rng (void)
{
  rng_state ^= rng_state << 13;
  rng_state ^= rng_state >> 17;
  rng_state ^= rng_state << 5;
  return rng_state;
}

/* Everything one link run produces. */
typedef struct
{
  int              locked;
  int              k;    /* the carrier-phase ambiguity the sync word chose */
  size_t           n;    /* frames sliced */
  uint8_t         *bits; /* n * NB sliced bits */
  int             *passed; /* n verdicts */
  uint8_t         *sent;   /* FRAMES * PAYLOAD payload bits that were sent */
  wfm_frame_desc_t desc;   /* the description, shared by both ends */
} link_t;

/* A real sync hit repeats at the frame period: the next few frames start
   with the same word. A tolerant search can land on noise, once, in a
   rotation that is wrong; it will not do so three times in a row. */
static int
repeats (const uint8_t *bits, size_t nbit, size_t at, unsigned tol)
{
  for (size_t f = 1; f <= 3; f++)
    {
      dp_syncword_hit_t h;
      if (at + f * NB + SYNC_BITS > nbit
          || !dp_syncword_search (bits + at + f * NB, SYNC_BITS, sync_bits,
                                  SYNC_BITS, tol, &h)
          || h.offset != 0 || h.inverted)
        return 0;
    }
  return 1;
}

/* Symbols -> bits for ambiguity k (MSB first): rotate, demap, expand. */
static void
to_bits (const float complex *sym, size_t nsym, int m, int k, uint8_t *bits,
         float complex *rot, uint8_t *lab)
{
  const int           bps = m == 2 ? 1 : 2;
  const float complex w
      = cexpf (-2.0f * (float)M_PI * I * (float)k / (float)m);
  for (size_t i = 0; i < nsym; i++)
    rot[i] = sym[i] * w;
  dp_mpsk_demap (rot, nsym, lab, m);
  for (size_t i = 0; i < nsym; i++)
    for (int b = 0; b < bps; b++)
      bits[i * bps + b] = (lab[i] >> (bps - 1 - b)) & 1u;
}

static int
run_link (link_t *L, int wfm_type, int m, double esn0_db)
{
  memset (L, 0, sizeof *L);
  rng_state = 3u;

  /* The ONE description: [sync:32 | data:48 | crc16]. */
  const wfm_seq_t sync
      = { .kind = WFM_SEQ_LITERAL, .bits = sync_bits, .len = SYNC_BITS };
  const wfm_seq_t data = { .kind = WFM_SEQ_DATA, .len = PAYLOAD };
  if (dp_wfm_frame_fixed (&L->desc, NULL, 0, &sync, &data, 1) != 0)
    return -1;
  wfm_frame_desc_layout_t lay;
  if (dp_wfm_frame_desc_layout (&L->desc, &lay) != 0 || lay.n_fields != 3
      || lay.frame_bits > MAX_NB)
    return -1;
  NB      = lay.frame_bits;
  PAY_OFF = lay.field_off[1]; /* field 1 is the payload */

  L->sent = malloc (FRAMES * PAYLOAD);
  if (!L->sent)
    return -1;
  for (size_t i = 0; i < FRAMES * PAYLOAD; i++)
    L->sent[i] = (rng () >> 7) & 1u;

  wfm_source_t src  = { 0 };
  src.type          = wfm_type;
  src.sps           = SPS;
  src.freq          = 0.0002; /* a carrier offset the receiver must remove */
  src.pulse         = WFM_PULSE_RRC_VALUE;
  src.rrc_beta      = 0.35;
  src.rrc_span      = 8;
  src.level         = -6.0;
  src.snr           = esn0_db;
  src.snr_mode      = WFM_SNR_ESNO;
  src.seed          = LINK_SEED;
  src.pn_length     = 7;
  src.data          = (wfm_seq_t){ .kind = WFM_SEQ_LITERAL,
                                   .bits = L->sent,
                                   .len  = FRAMES * PAYLOAD };
  src.data_len      = PAYLOAD;
  src.frame         = &L->desc;
  wfm_segment_t seg = { .sources = &src, .n_sources = 1, .fs = 1.0 };
  const char   *why = dp_wfm_scene_error (&seg, 1, 0, 0);
  if (why)
    {
      fprintf (stderr, "refused: %s\n", why);
      return -1;
    }
  dp_wfm_compose_state_t *c = dp_wfm_compose_create (&seg, 1, 0, 0);
  if (!c)
    return -1;
  const size_t   total = (size_t)FRAMES * NB * SPS;
  float complex *iq    = malloc ((total + 64u) * sizeof *iq);
  if (!iq)
    return -1;
  const size_t nx = dp_wfm_compose_execute (c, iq, total + 64u);
  dp_wfm_compose_destroy (c);

  /* Receive: defaults match the Python MpskReceiver for everything unset. */
  dp_mpsk_receiver_state_t *rx = dp_mpsk_receiver_create (
      m, SPS, SPS /* m_out */, MPSK_RX_PULSE_RRC, 0.35, 8, 0.005, 0.0, 0.01,
      0.65, 0.0, 0, 0, 1, 0.0);
  if (!rx)
    return -1;
  const size_t   cap = nx / SPS + 64u;
  float complex *sym = malloc (cap * sizeof *sym);
  if (!sym)
    return -1;
  const size_t nsym = dp_mpsk_receiver_steps (rx, iq, nx, sym, cap);
  L->locked         = dp_mpsk_receiver_get_locked (rx);
  dp_mpsk_receiver_destroy (rx);
  free (iq);

  /* Resolve the phase ambiguity by the sync word. */
  const int      bps  = m == 2 ? 1 : 2;
  const size_t   nbit = nsym * bps;
  uint8_t       *bits = malloc (nbit), *best_bits = malloc (nbit);
  uint8_t       *lab = malloc (nsym);
  float complex *rot = malloc (nsym * sizeof *rot);
  if (!bits || !best_bits || !lab || !rot)
    return -1;
  /* The tolerance is the library's: the largest Hamming distance whose
     false-hit rate over this whole run stays under 1e-3. */
  const int tol = dp_syncword_max_errors (SYNC_BITS, nbit, 1e-3);
  if (tol < 0)
    return -1;
  int               best_k = -1;
  dp_syncword_hit_t best   = { 0 };
  for (int k = 0; k < m; k++)
    {
      to_bits (sym, nsym, m, k, bits, rot, lab);
      dp_syncword_hit_t hit;
      if (dp_syncword_search (bits, nbit, sync_bits, SYNC_BITS, (unsigned)tol,
                              &hit)
          && !hit.inverted && repeats (bits, nbit, hit.offset, (unsigned)tol)
          && (best_k < 0 || hit.errors < best.errors))
        {
          best_k = k;
          best   = hit;
          memcpy (best_bits, bits, nbit);
        }
    }
  L->k            = best_k < 0 ? 0 : best_k;
  const size_t s0 = best_k >= 0 ? best.offset % NB : 0;
  L->n            = (nbit - s0) / NB;
  L->bits         = malloc (L->n * NB);
  L->passed       = calloc (L->n, sizeof *L->passed);
  if (!L->bits || !L->passed)
    return -1;
  memcpy (L->bits, best_bits + s0, L->n * NB);
  for (size_t i = 0; i < L->n; i++)
    {
      uint8_t f[MAX_NB]; /* the check may repair in place: judge a copy */
      memcpy (f, L->bits + i * NB, NB);
      L->passed[i] = dp_wfm_frame_check (&L->desc, NULL, f, NULL) == 1;
    }
  free (sym);
  free (bits);
  free (best_bits);
  free (lab);
  free (rot);
  return 0;
}

static void
link_free (link_t *L)
{
  free (L->bits);
  free (L->passed);
  free (L->sent);
}

/* Was this payload one of the frames sent? */
static int
was_sent (const link_t *L, const uint8_t *payload)
{
  for (size_t j = 0; j < FRAMES; j++)
    if (memcmp (L->sent + j * PAYLOAD, payload, PAYLOAD) == 0)
      return 1;
  return 0;
}

int
main (void)
{
  const uint32_t word = 0x1ACFFC1Du;
  for (int i = 0; i < SYNC_BITS; i++)
    sync_bits[i] = (word >> (SYNC_BITS - 1 - i)) & 1u;

  static const struct
  {
    const char *name;
    int         type, m;
  } kinds[] = { { "bpsk", WFM_SYNTH_BPSK, 2 }, { "qpsk", WFM_SYNTH_QPSK, 4 } };

  for (int t = 0; t < 2; t++)
    {
      link_t L;
      printf ("%s @ 14 dB Es/N0\n", kinds[t].name);
      if (run_link (&L, kinds[t].type, kinds[t].m, 14.0) != 0)
        {
          fprintf (stderr, "link setup failed\n");
          return 1;
        }
      size_t first = L.n;
      for (size_t i = 0; i < L.n; i++)
        if (L.passed[i] && was_sent (&L, L.bits + i * NB + PAY_OFF))
          {
            first = i;
            break;
          }
      check (L.locked, "2. the receiver locked");
      check (first < L.n, "3. some frame passed the check");
      check (first < SETTLE, "3. the loops settle within SETTLE frames");
      size_t n_ok = 0;
      int    tail = 1, exact = 1;
      for (size_t i = 0; i < L.n; i++)
        {
          n_ok += L.passed[i];
          if (i >= SETTLE)
            tail = tail && L.passed[i];
        }
      for (size_t i = 0; i < L.n; i++)
        if (L.passed[i] && !was_sent (&L, L.bits + i * NB + PAY_OFF))
          exact = 0;
      check (tail, "3. every frame after the settling window passes");
      check (n_ok + SETTLE >= FRAMES,
             "3. all but the transient frames passed");
      check (exact, "3. every passed payload is one that was sent");
      if (first < L.n)
        {
          uint8_t bad[MAX_NB];
          memcpy (bad, L.bits + first * NB, NB);
          bad[PAY_OFF + 4] ^= 1u; /* one payload bit */
          check (dp_wfm_frame_check (&L.desc, NULL, bad, NULL) == 0,
                 "4. one flipped payload bit fails the check");
        }
      printf ("%s: phase k=%d, %zu/%d frames passed\n", kinds[t].name, L.k,
              n_ok, FRAMES);
      link_free (&L);
    }

  /* Low Es/N0: damaged frames fail; none passes with a wrong payload. */
  link_t L;
  printf ("bpsk @ 4 dB Es/N0\n");
  if (run_link (&L, WFM_SYNTH_BPSK, 2, 4.0) != 0)
    {
      fprintf (stderr, "link setup failed\n");
      return 1;
    }
  size_t n_fail = 0, wrong = 0;
  for (size_t i = 0; i < L.n; i++)
    {
      n_fail += !L.passed[i];
      wrong += L.passed[i] && !was_sent (&L, L.bits + i * NB + PAY_OFF);
    }
  check (2 * n_fail > L.n, "4. more than half the frames fail at 4 dB");
  check (wrong == 0, "4. no damaged frame passes with a wrong payload");
  printf ("bpsk @ 4 dB: %zu frames failed the check, %zu passed wrong\n",
          n_fail, wrong);
  link_free (&L);

  printf ("%s\n", failures ? "FAILED" : "all checks passed");
  return failures ? 1 : 0;
}
