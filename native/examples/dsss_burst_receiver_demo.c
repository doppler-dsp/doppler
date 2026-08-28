/**
 * dsss_burst_receiver_demo.c — the DSSS burst chain as ONE object, in C.
 *
 * `DsssBurstReceiver` composes search -> refine -> demod behind a single
 * `push()`. The Python twin
 * (`src/doppler/examples/dsss_burst_receiver_demo.py`) shows the same
 * properties with plots; this one shows what the binding does FOR you and
 * therefore hides: the lifecycle you manage yourself, who owns the output
 * buffer, and how big it has to be.
 *
 * **The waveform is not built here.** It is one declarative wfmgen scene --
 * a `wfm_segment_t` carrying one `wfm_source_t` of `WFM_SYNTH_DSSS`, handed
 * to `wfm_compose_create()`. Same engine as the Python example's
 * `Composer`/`Segment` and as the `wfmgen` CLI, so all three render the
 * identical capture. Nothing here tiles a preamble, spreads a frame, appends
 * a CRC or draws noise, and the codes come from wfm's own PN generator: a
 * second implementation of any of those would drift from the one that ships.
 *
 * A burst TRAIN is `repeats`, not a loop building four segments. One
 * declaration, and the engine spaces them.
 *
 * Four sections, each printing a number a reader can check:
 *
 *   §1  One burst in a noisy capture: decoded, CRC valid, bits exact.
 *   §2  Block-size independence — the same capture pushed whole, in 64 KiB,
 *       1000 and 333-sample blocks, gives the same answer.
 *   §3  A burst split across two push() calls is HELD, not lost, and
 *       `pending` says so. Read it before you stop feeding a stream.
 *   §4  Bursts closer than `refine_span` coalesce — that span IS the
 *       minimum burst spacing, not a suggestion.
 *   §5  Every read-back, for EVERY burst: the nine fields of
 *       `dsss_br_event_t`, each checked against what the scene says it must
 *       be. The scalar members describe only the LAST burst — that is why
 *       the record exists — and this section prints both and asserts the
 *       relation between them.
 *
 * Build:
 *   cmake --build build
 *   ./build/native/examples/dsss_burst_receiver_demo
 */

#include <dsss_burst_receiver/dsss_burst_receiver_core.h>
#include <frame/frame_core.h>
#include <pn/pn_core.h>
#include <wfm/wfm_compose.h>

#include <complex.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── geometry ──────────────────────────────────────────────────────────────
 * 255 chips at 2 samples/chip is 510 = 2*3*5*17 correlation bins. The length
 * is chosen on the TRANSFORM as well as the code: acq transforms sf*spc
 * verbatim -- the code axis is a circular correlation, so it cannot be padded
 * -- and a 127-chip m-sequence at spc=4 gives 508 = 2^2*127, where the FFT
 * falls back to Bluestein and costs 12x. 255 is smooth AND keeps the ideal
 * m-sequence autocorrelation. spc >= 2 always. */
#define ACQ_SF 255u
#define DATA_SF 31u
#define REPS 5u
#define SPC 2u
#define PAYLOAD 96u
#define SYNC_LEN 13u
#define CHIP_RATE 1.0e6
#define FS (CHIP_RATE * SPC)
#define ESN0_DB 12.0
#define CN0_DBHZ 60.0

#define FRAME_SYMS (SYNC_LEN + PAYLOAD + 16u) /* sync | payload | CRC-16 */
#define BURST_LEN ((REPS * ACQ_SF + FRAME_SYMS * DATA_SF) * SPC)
#define CAP_MAX 200000u

static const char SYNC_BITS[SYNC_LEN + 1] = "0000011001010";

/** @brief `2^stages - 1` chips of an m-sequence, from wfm's PN generator.
 *
 * The same generator `doppler.wfm.PN` binds, so the C and Python examples
 * build the same code rather than two that merely look alike. Not fussiness:
 * a code whose peak-to-worst-sidelobe ratio is near 1 lets the CFAR reference
 * read the code's own autocorrelation instead of the noise, and this
 * receiver's certification measured what that costs -- 47% of burst offsets
 * lost at ratio 1.07, against every one found at 31.
 */
static uint8_t *
mls (unsigned stages, uint32_t seed, uint8_t *out)
{
  size_t      n  = ((size_t)1u << stages) - 1u;
  pn_state_t *pn = pn_create (pn_mls_poly (stages), seed, stages, 0);
  for (size_t i = 0; i < n; i++)
    out[i] = (uint8_t)(pn_step (pn) & 1u);
  pn_destroy (pn);
  return out;
}

/** @brief One DSSS source: the burst geometry, as the C API declares it. */
static wfm_source_t
dsss_source (uint8_t *acq, uint8_t *data, uint8_t *sy, uint8_t *payload)
{
  wfm_source_t src = { 0 };
  src.type         = WFM_SYNTH_DSSS;
  src.freq         = 0.0;
  src.snr          = ESN0_DB;
  src.snr_mode     = 3; /* esno -- Es/N0 of the DATA_SF-chip data symbol */
  src.seed         = 1u;
  src.sps          = (int)SPC; /* samples per CHIP */
  src.acq_code     = acq;
  src.n_acq_code   = ACQ_SF;
  src.acq_reps     = REPS;
  src.data_code    = data;
  src.n_data_code  = DATA_SF;
  src.sync         = sy;
  src.n_sync       = SYNC_LEN;
  src.crc          = 1;       /* crc16 trailer, appended by the engine */
  src.bits         = payload; /* the payload rides `bits` */
  src.n_bits       = PAYLOAD;
  return src;
}

/** @brief Compose `repeats` bursts spaced `gap` apart, into `out`. */
static size_t
compose (wfm_source_t *src, size_t repeats, size_t gap, float complex *out,
         size_t max)
{
  wfm_segment_t seg = { 0 };
  seg.sources       = src;
  seg.n_sources     = 1u;
  seg.fs            = FS;
  /* num_samples left 0: a single-source DSSS segment sizes its own on-time
     to exactly one burst, so the geometry is stated once, in the source. */
  seg.off_samples = gap;
  seg.repeats     = repeats;
  seg.gap_noise   = 0; /* auto -- the floor runs through the gaps, as a
                          real capture does, rather than digital silence */

  wfm_compose_state_t *c = wfm_compose_create (&seg, 1u, 0, 0);
  if (!c)
    return 0;
  size_t total = 0, got;
  while (total < max
         && (got = wfm_compose_execute (c, out + total, max - total)) > 0)
    total += got;
  wfm_compose_destroy (c);
  return total;
}

static dsss_burst_receiver_state_t *
make_rx (const uint8_t *acode, const uint8_t *dcode, const uint8_t *sy)
{
  return dsss_burst_receiver_create (
      acode, ACQ_SF, dcode, DATA_SF, sy, SYNC_LEN, REPS, SPC, CHIP_RATE,
      FRAME_SYMS, CN0_DBHZ, 0.0, 1e-3, 0.9, 0.0, 0.0, 10);
}

/** @brief Push `cap` in `block` chunks; return payloads decoded. */
static size_t
decode_in_blocks (const float complex *cap, size_t cap_len, size_t block,
                  const uint8_t *acode, const uint8_t *dcode,
                  const uint8_t *sy, uint8_t *first_payload)
{
  dsss_burst_receiver_state_t *rx = make_rx (acode, dcode, sy);
  if (!rx)
    return 0;
  /* The buffer is the CALLER's, and its size comes from push_max_out on the
     block being pushed -- NOT from payload_len, because one call may complete
     several bursts. That is the whole reason the bound scales with the input
     (doppler#1008). */
  size_t   cap_out = dsss_burst_receiver_push_max_out (rx, block);
  uint8_t *out     = malloc (cap_out ? cap_out : 1u);
  size_t   total   = 0;
  int      kept    = 0;
  for (size_t off = 0; off < cap_len; off += block)
    {
      size_t n   = cap_len - off < block ? cap_len - off : block;
      size_t got = dsss_burst_receiver_push (rx, cap + off, n, out, cap_out);
      if (got && !kept && first_payload)
        {
          memcpy (first_payload, out, FRAME_SYMS);
          kept = 1;
        }
      total += got / FRAME_SYMS;
    }
  free (out);
  dsss_burst_receiver_destroy (rx);
  return total;
}

int
main (void)
{
  static uint8_t acode[ACQ_SF], dcode[DATA_SF], payload[PAYLOAD];
  static uint8_t sy[SYNC_LEN] = { 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 1, 0 };
  mls (8u, 1u, acode);
  mls (5u, 3u, dcode);
  for (size_t i = 0; i < PAYLOAD; i++)
    payload[i] = (uint8_t)((i * 7u + 3u) & 1u);

  wfm_source_t src = dsss_source (acode, dcode, sy, payload);

  /* The two spans a caller must respect, read from the object rather than
     restated: `refine_span` is the minimum burst spacing (anchors closer are
     coalesced as one preamble) and `retain_span` is the history kept per
     anchor. Both were internal until gh-1011. */
  dsss_burst_receiver_state_t *probe = make_rx (acode, dcode, sy);
  if (!probe)
    {
      fprintf (stderr, "create failed\n");
      return 1;
    }
  const size_t refine_span = probe->refine_span;
  const size_t retain_span = probe->retain_span;
  dsss_burst_receiver_destroy (probe);

  printf ("=== DsssBurstReceiver — the burst chain as one object ===\n");
  printf ("  waveform    one wfmgen segment via wfm_compose_create()\n");
  printf ("  burst_len   %6zu samples   (%u-chip preamble x%u, spc %u)\n",
          (size_t)BURST_LEN, ACQ_SF, REPS, SPC);
  printf ("  refine_span %6zu           minimum burst spacing\n", refine_span);
  printf ("  retain_span %6zu           history kept per anchor\n\n",
          retain_span);

  static float complex cap[CAP_MAX];
  int                  rc = 1;

  /* ── §1  one burst in a noisy capture ────────────────────────────────── */
  {
    size_t n = compose (&src, 1u, retain_span * 2u, cap, CAP_MAX);
    if (!n)
      {
        fprintf (stderr, "compose failed\n");
        return 1;
      }
    uint8_t got[FRAME_SYMS];
    size_t  d     = decode_in_blocks (cap, n, n, acode, dcode, sy, got);
    int     exact = (d == 1u);
    /* push() hands back the FRAME — this receiver stops at decisions
       (doppler#1022) — so the payload is a slice, and the trailer is
       checked by `frame_deframe()` in §5 below. */
    for (size_t i = 0; i < PAYLOAD && exact; i++)
      exact = (got[SYNC_LEN + i] == payload[i]);
    printf ("§1  %zu-sample capture, one burst: %zu decoded, payload %s\n", n,
            d, exact ? "bit-exact" : "WRONG");
    if (!exact)
      return 1;

    /* ── §2  block size does not change the answer ─────────────────────── */
    printf ("§2  block-size independence:\n");
    const size_t blocks[] = { n, 65536u, 1000u, 333u };
    for (size_t b = 0; b < sizeof blocks / sizeof *blocks; b++)
      {
        size_t got_n
            = decode_in_blocks (cap, n, blocks[b], acode, dcode, sy, NULL);
        printf ("      %6zu-sample blocks -> %zu burst(s)\n", blocks[b],
                got_n);
        if (got_n != 1u)
          {
            fprintf (stderr, "block size changed the answer (gh-1008)\n");
            return 1;
          }
      }

    /* ── §3  a split burst is held, and `pending` says so ──────────────── */
    dsss_burst_receiver_state_t *rx  = make_rx (acode, dcode, sy);
    size_t                       cut = BURST_LEN / 2u;
    size_t   cap_out = dsss_burst_receiver_push_max_out (rx, n);
    uint8_t *out     = malloc (cap_out);
    size_t   a1      = dsss_burst_receiver_push (rx, cap, cut, out, cap_out);
    size_t   held    = rx->pending;
    size_t   a2
        = dsss_burst_receiver_push (rx, cap + cut, n - cut, out, cap_out);
    printf ("§3  split mid-burst: push 1 -> %zu payload(s), pending %zu;"
            "  push 2 -> %zu, pending %zu\n",
            a1 / FRAME_SYMS, held, a2 / FRAME_SYMS, rx->pending);
    int ok = (a1 == 0 && held == 1u && a2 == FRAME_SYMS && rx->pending == 0);
    for (size_t i = 0; i < PAYLOAD && ok; i++)
      ok = (out[SYNC_LEN + i] == payload[i]);
    free (out);
    dsss_burst_receiver_destroy (rx);
    printf ("      -> held, then returned whole. Read pending before you"
            " stop feeding.\n");
    if (!ok)
      {
        fprintf (stderr, "a split burst was not recovered\n");
        return 1;
      }
  }

  /* ── §4  closer than refine_span, and they coalesce ──────────────────── */
  {
    /* Between one burst and one refine_span: the bursts do not overlap, they
       are simply nearer than the window that decides "same preamble". */
    size_t tight = (BURST_LEN + refine_span) / 2u;
    size_t want  = 4u;
    size_t n     = compose (&src, want, tight - BURST_LEN, cap, CAP_MAX);
    size_t d     = decode_in_blocks (cap, n, n, acode, dcode, sy, NULL);
    printf (
        "§4  %zu bursts spaced %zu (inside refine_span %zu): %zu decoded\n",
        want, tight, refine_span, d);
    printf ("      -> coalesced. refine_span IS the minimum spacing.\n");
    if (d >= want)
      {
        fprintf (stderr, "bursts inside refine_span were all decoded — the"
                         " rule this demonstrates has changed\n");
        return 1;
      }
  }

  /* ── §5  every read-back, for EVERY burst ─────────────────────────────── */
  {
    /* What the scene says each read-back must be, DERIVED rather than
       remembered: acq transforms sf*spc verbatim, so the bin width is fs
       over that, and a segment generated at Es/N0 with a DATA_SF-chip
       symbol carries C/N0 = Es/N0 + 10log10(chip_rate / DATA_SF). */
    const double bin_hz = FS / (double)(ACQ_SF * SPC);
    const double cn0_true
        = ESN0_DB + 10.0 * log10 (CHIP_RATE / (double)DATA_SF);

    const size_t spacing = refine_span + refine_span / 5u;
    const size_t want    = 4u;
    size_t       n = compose (&src, want, spacing - BURST_LEN, cap, CAP_MAX);

    dsss_burst_receiver_state_t *rx = make_rx (acode, dcode, sy);
    if (!rx || !n)
      {
        fprintf (stderr, "§5 setup failed\n");
        return 1;
      }
    size_t   cap_out = dsss_burst_receiver_push_max_out (rx, n);
    uint8_t *out     = malloc (cap_out);
    size_t   got     = dsss_burst_receiver_push (rx, cap, n, out, cap_out);

    /* One record per burst the push returned -- ask the object how many,
       never assume: a single push can complete several. */
    dsss_br_event_t ev[8];
    size_t          nev = dsss_burst_receiver_events_max_out (rx);
    nev = dsss_burst_receiver_events (rx, nev, ev, sizeof ev / sizeof *ev);

    printf ("§5  every read-back, per burst (%zu payload(s), %zu event(s)):\n",
            got / FRAME_SYMS, nev);
    printf ("       # %8s %8s %8s %9s %8s %8s %8s %7s\n", "start", "dopp_hz",
            "res_hz", "cn0_dBHz", "freq_hz", "rate_hz", "conf_dB", "margin");
    for (size_t i = 0; i < nev; i++)
      printf ("      %2zu %8llu %8.1f %8.1f %9.2f %8.2f %8.2f %8.2f %7.3f\n",
              i, (unsigned long long)ev[i].preamble_start,
              ev[i].doppler_hz_est, ev[i].doppler_res_hz, ev[i].cn0_dbhz_est,
              ev[i].est_freq_hz, ev[i].est_rate_hz, ev[i].est_snr_db,
              ev[i].refine_margin);
    printf ("      scene: bin %.1f Hz, C/N0 %.2f dB-Hz, true offset 0 Hz,"
            " no chirp\n",
            bin_hz, cn0_true);

    int ok = (nev == want);
    for (size_t i = 0; i < nev && ok; i++)
      {
        const dsss_br_event_t *e = &ev[i];
        ok = e->preamble_start == (uint64_t)(i * spacing)
             /* the search grid: width derived, estimate inside the bin
                that contains the true 0 Hz */
             && e->doppler_res_hz == bin_hz
             && fabs (e->doppler_hz_est) <= bin_hz / 2.0
             /* and the reason the chain does not stop at acquisition */
             && fabs (e->est_freq_hz) < bin_hz / 100.0
             /* zero here is a CONFIGURATION fact: max_rate = 0 switches
                the chirp axis off, it is not a measured absence */
             && e->est_rate_hz == 0.0
             /* cn0_dbhz_est is documented as a LOWER bound */
             && e->cn0_dbhz_est <= cn0_true + 1.5
             && e->cn0_dbhz_est >= cn0_true - 3.0
             /* est_snr_db is the estimator's peak-to-mean confidence, NOT
                a link SNR -- do not compare it with Es/N0 */
             && e->est_snr_db > 10.0 && e->refine_margin > 0.0
             && e->refine_margin < 1.0;
      }
    /* The scalar members are not a second source: they ARE the last row. */
    if (ok && nev)
      {
        const dsss_br_event_t *last = &ev[nev - 1];
        ok = rx->preamble_start == last->preamble_start

             && rx->doppler_hz_est == last->doppler_hz_est
             && rx->doppler_res_hz == last->doppler_res_hz
             && rx->cn0_dbhz_est == last->cn0_dbhz_est
             && rx->est_freq_hz == last->est_freq_hz
             && rx->est_rate_hz == last->est_rate_hz
             && rx->est_snr_db == last->est_snr_db
             && rx->refine_margin == last->refine_margin;
      }
    dsss_burst_receiver_destroy (rx);
    printf ("      -> every row checks out against the scene; the scalar"
            " read-backs equal the last row, which is all they claim.\n");
    if (!ok)
      {
        fprintf (stderr, "a read-back disagreed with the scene that"
                         " produced it\n");
        return 1;
      }

    /* ── §6  the frame is undone one layer up ──────────────────────────── */
    {
      /* The receiver stopped at decisions: §5's rows are FRAME BITS and no
         opinion about them (doppler#1022). Turning those into a payload —
         and into a verdict — needs the frame's description, which is what
         `frame_create()` builds and `frame_deframe()` reads. */
      frame_state_t *f = frame_create (
          0, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,       /* no preamble    */
          0, sy, SYNC_LEN, 0, 0, 0, 0, 0, 0, 0, 0, 0,     /* literal sync   */
          0, payload, PAYLOAD, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* the payload   */
          1);                                             /* crc16 trailer */
      if (!f)
        {
          fprintf (stderr, "frame_create failed\n");
          return 1;
        }
      const size_t nbits  = frame_deframe_max_out (f, 0);
      uint8_t     *undone = malloc (nbits ? nbits : 1u);
      int          all_ok = (nbits == FRAME_SYMS) && undone != NULL;
      printf (
          "§6  deframed by frame_deframe() (the receiver has no opinion):\n");
      for (size_t i = 0; i < nev && all_ok; i++)
        {
          const size_t got_n = frame_deframe (f, out + i * FRAME_SYMS,
                                              FRAME_SYMS, undone, nbits);
          /* The NAMED view, because this frame was built the four-field
             way: [preamble | sync | payload | crc]. A field-by-field
             description would index its own fields instead. */
          const size_t poff = frame_layout (f).payload_off;
          int          same = (got_n == FRAME_SYMS);
          for (size_t k = 0; k < PAYLOAD && same; k++)
            same = (undone[poff + k] == payload[k]);
          /* `checked` is the load-bearing half: a frame carrying NO check
             reports 0 and 0, which is not the same fact as a failed one. */
          all_ok = same && f->rx_checked == 1 && f->rx_ok == f->rx_units;
        }
      printf ("      %zu/%zu frames check out, payloads bit-exact\n",
              all_ok ? nev : 0u, nev);
      printf ("      -> decide, then deframe. Two objects, one frame.\n");
      free (undone);
      free (out);
      frame_destroy (f);
      if (!all_ok)
        {
          fprintf (stderr, "a returned frame did not deframe cleanly\n");
          return 1;
        }
    }
    rc = 0;
  }

  printf ("\nall sections passed\n");
  return rc;
}
