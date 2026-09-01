/**
 * burst_capture_demo.c — the lifecycle a caller has to manage.
 *
 * `BurstCapture` turns a detector's output into bursts: it searches the
 * stream, resolves WHICH preamble repetition a detection landed in — the
 * question acquisition structurally cannot answer, because its code phase is
 * a lag modulo one code period — and hands back the burst's samples once they
 * have all arrived. It stops there; demodulating is `BurstDemod`'s job.
 *
 * This is the C face, so it shows what the Python binding does for you and
 * hides: who owns the output buffer, how big it has to be, that a window is
 * borrowed rather than given, and that a burst is HELD until its last sample
 * arrives rather than guessed at.
 *
 * Feeds a noisy stream carrying three bursts in 4096-sample blocks — the
 * shape a real caller has, where a burst routinely straddles two calls — and
 * prints one line per captured burst. Writes the first window to
 * burst_capture_window.csv so it can be plotted.
 *
 * Build:
 *   make build
 *   ./build/native/examples/burst_capture_demo
 */

#include <burst_capture/burst_capture_core.h>
#include <pn/pn_core.h>

#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define ACQ_SF 31u       /* preamble code length, chips            */
#define DATA_SF 8u       /* payload spreading factor               */
#define REPS 4u          /* preamble repetitions                   */
#define SPC 4u           /* samples per chip                       */
#define PAYLOAD_SYMS 61u /* payload symbols after the preamble     */
#define CHIP_RATE 1.0e6  /* Hz                                     */
#define BLOCK 4096u      /* the caller's block size                */
#define N_TOTAL 200000u  /* samples in the stream                  */
#define SIGMA 0.02       /* noise floor, per component             */

/* Preamble + spread payload, in samples: what one window holds. */
#define BURST_LEN ((REPS * ACQ_SF + PAYLOAD_SYMS * DATA_SF) * SPC)

static float
csign (uint8_t c)
{
  return (c & 1u) ? -1.0f : 1.0f;
}

/** @brief A deterministic Gaussian-ish sample, so the demo is repeatable. */
static double
noise (uint32_t *st)
{
  double s = 0.0;
  for (int i = 0; i < 4; i++)
    {
      *st = *st * 1664525u + 1013904223u;
      s += (double)((*st >> 8) & 0xFFFFu) / 65535.0 - 0.5;
    }
  return s;
}

int
main (void)
{
  /* An m-sequence, not an arithmetic pattern: a code whose worst
     autocorrelation sidelobe is near its peak sets the CFAR reference from
     its own structure rather than from noise, and roughly halves the burst
     offsets that are found at all. */
  uint8_t     acq_code[ACQ_SF], data_code[DATA_SF];
  pn_state_t *pn = pn_create (pn_mls_poly (5), 1u, 5u, 0);
  if (!pn)
    return 1;
  for (size_t i = 0; i < ACQ_SF; i++)
    acq_code[i] = pn_step (pn);
  pn_destroy (pn);
  for (size_t i = 0; i < DATA_SF; i++)
    data_code[i] = (uint8_t)((i >> 1) & 1u);

  /* ── The scene: noise, plus three bursts ──────────────────────────── */
  const size_t   at[3] = { 9000u, 60000u, 120000u };
  float complex *x     = malloc (N_TOTAL * sizeof *x);
  if (!x)
    return 1;
  uint32_t st = 7u;
  for (size_t i = 0; i < N_TOTAL; i++)
    x[i] = (float)(SIGMA * noise (&st)) + (float)(SIGMA * noise (&st)) * I;
  for (size_t k = 0; k < 3u; k++)
    {
      size_t j = at[k];
      for (size_t r = 0; r < REPS; r++)
        for (size_t c = 0; c < ACQ_SF; c++)
          for (size_t s = 0; s < SPC; s++)
            x[j++] += csign (acq_code[c]);
      for (size_t m = 0; m < PAYLOAD_SYMS; m++)
        {
          float a = csign ((uint8_t)(m & 1u));
          for (size_t c = 0; c < DATA_SF; c++)
            for (size_t s = 0; s < SPC; s++)
              x[j++] += a * csign (data_code[c]);
        }
    }

  /* ── Create ───────────────────────────────────────────────────────────
   * The look-back is NOT a parameter: its span is derived from the geometry,
   * because every term is known here and a caller asked to size a history
   * buffer is a caller handed a way to lose bursts silently. */
  burst_capture_state_t *cap = burst_capture_create (
      acq_code, ACQ_SF, BURST_LEN, REPS, SPC, CHIP_RATE, 55.0, 0.0, 1e-3, 0.9);
  if (!cap)
    {
      fprintf (stderr, "burst_capture_create failed\n");
      free (x);
      return 1;
    }

  printf ("BurstCapture demo\n");
  printf ("  burst_len   %6u samples  (what a window holds)\n",
          (unsigned)cap->burst_len);
  printf ("  refine_span %6u samples  (two detections this close are ONE "
          "burst)\n",
          (unsigned)cap->refine_span);
  printf ("  retain_span %6u samples  (minimum trailing context)\n\n",
          (unsigned)cap->retain_span);

  /* ── Size the output buffer ───────────────────────────────────────────
   * push_max_out() USES its argument: distinct bursts cannot overlap, so a
   * block of n samples completes at most n/burst_len + 1 of them, plus
   * whatever is already queued. Sizing by hand is how a caller silently
   * truncates. */
  size_t         cap_out = burst_capture_push_max_out (cap, BLOCK);
  float complex *out     = malloc (cap_out * sizeof *out);
  if (!out)
    {
      burst_capture_destroy (cap);
      free (x);
      return 1;
    }

  printf ("  %-4s %-12s %-12s %-10s %s\n", "#", "start", "C/N0 (dB-Hz)",
          "margin", "note");

  /* ── Feed ─────────────────────────────────────────────────────────────
   * Any block size is accepted: the ring is a contiguous window over the
   * stream and is never reset between bursts, so a burst whose tail falls
   * outside one call is completed by a later one. */
  size_t n_found = 0;
  for (size_t off = 0; off < N_TOTAL; off += BLOCK)
    {
      size_t blk = N_TOTAL - off < BLOCK ? N_TOTAL - off : BLOCK;
      size_t n   = burst_capture_push (cap, x + off, blk, out, cap_out);

      /* n is a whole number of windows, always -- half a burst is not a
         burst, so a short buffer truncates at a window boundary. */
      for (size_t i = 0; i < n / BURST_LEN; i++)
        {
          /* The event row that belongs to THIS window. The scalar
             read-backs describe only the last one, which is why a call
             completing several needs the list. */
          const burst_capture_event_t *ev = burst_capture_event_at (cap, i);
          printf ("  %-4zu %-12llu %-12.1f %-10.3f %s\n", ++n_found,
                  (unsigned long long)ev->preamble_start, ev->cn0_dbhz_est,
                  ev->refine_margin,
                  ev->cn0_dbhz_est > 52.0 ? "burst" : "likely spurious");

          if (n_found == 1u)
            {
              /* burst_capture_window() BORROWS out of the capture's own
                 scratch -- contiguous, burst_len long, and valid only until
                 the next push(). Copy it if it has to outlive the call. */
              const float complex *w = burst_capture_window (cap, i);
              FILE                *f = fopen ("burst_capture_window.csv", "w");
              if (f && w)
                {
                  fprintf (f, "n,i,q\n");
                  for (size_t s = 0; s < 512u; s++)
                    fprintf (f, "%zu,%.6f,%.6f\n", s, (double)crealf (w[s]),
                             (double)cimagf (w[s]));
                  fclose (f);
                }
            }
        }
    }

  /* ── Drain ────────────────────────────────────────────────────────────
   * `pending` is the only signal that a burst was FOUND but not yet
   * complete. A caller closing a file or a socket while it is non-zero is
   * discarding a burst that would have been captured, and every other
   * read-back looks identical to "nothing was ever there". */
  printf ("\n  captured %zu window(s); %zu detection(s) still awaiting "
          "samples; %llu sample(s) dropped\n",
          n_found, cap->pending,
          (unsigned long long)burst_capture_get_dropped (cap));
  printf ("  wrote burst_capture_window.csv (first 512 samples of burst 1)\n");

  /* ── Destroy ──────────────────────────────────────────────────────── */
  burst_capture_destroy (cap);
  free (out);
  free (x);
  return 0;
}
