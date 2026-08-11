/**
 * @file dp_tx_test.h
 * @brief The SSOT for harness STIMULUS: one shaped symbol stream, one place.
 *
 * This is the transmit half of the `dp_*_test.h` family, and the family is
 * the whole point — a harness gets its signal here, its symbol-quality
 * verdicts from dp_sym_test.h, its error rates from dp_ber_test.h, and its
 * serialization round-trip from dp_state_test.h. Nothing in any of them is
 * re-derived; each composes primitives the library already ships.
 *
 * ## Why this file exists
 *
 * The same analytic direct-form synthesis loop had been written three times,
 * and the copies did not differ in their mathematics. They differed in the
 * CONVENTIONS around it, which is the part that goes wrong silently:
 *
 *   | copy                              | symbol amplitude | symbol source  |
 *   | --------------------------------- | ---------------- | -------------- |
 *   | dp_mf_test.h::mf_tx               | hard-coded 0.25  | one-shot LCG   |
 *   | test_ratesync_core.c::_tx_amp     | parameter, 1.0   | xorshift32     |
 *   | test_RateConverter_core.c::_mf_tx | _MF_TX_AMP       | one-shot LCG   |
 *
 * A detector's slope goes as A^2, so two harnesses driving the same object at
 * 0.25 and at 1.0 are measuring loop bandwidths a factor of ~16 apart while
 * both look like "the RRC BPSK test". The Python layer had four more copies of
 * the same loop, one of which cost a session:
 * `src/doppler/examples/ratesync_demo.py` normalised its stream to 0.25 of its
 * PEAK, and since an RRC stream peaks at ~1.582x its symbol amplitude the
 * object received ~0.158 against a contract written in unit amplitude. See
 * `scripts/check_stimulus_sources.py`, the gate that stops new copies.
 *
 * ## The three conventions this file fixes, once
 *
 * 1. **Symbol amplitude is STATED, never derived from a peak.** `cfg.amp` is
 *    the amplitude of a symbol, not of the waveform. The shaped stream peaks
 *    above it by the pulse's PAPR (~1.582x for RRC at beta = 0.35) — that is a
 *    property of the pulse, and a receiver that cannot take it has a headroom
 *    bug worth failing on rather than a stimulus worth backing off.
 *
 *    The default `amp = 1.0` is **derived, not chosen**: a matched cascade's
 *    reference is `RateConverter_agc_ref_db()` = `10*log10(bank_e0/bank_sps)`,
 *    which is ~0 dB because the bank normalises by its own pulse energy. Unit
 *    symbol amplitude IS that reference.
 *
 *    **Which way to use `amp` depends on whether an AGC is in the chain**, and
 *    that is a property of the object, not a preference:
 *
 *      - **Level-critical** — an object presented raw symbols. RateSync is the
 *        example and says so (`ratesync_core.h`: it "carries no AGC, and that
 *        is deliberate", because a composing receiver already levels in its
 *        own front-end cascade via `RateConverter_enable_agc()`, one per
 *        receiver, and a second would integrate against the first). A TED's
 *        slope goes as `A^2`, so such a test must STATE `amp` and hold it.
 *      - **Level-agnostic** — an object behind that AGC (`MpskReceiver`, where
 *        `agc` is on by default and `bn_agc_ratio` sizes it against the
 *        slowest loop). Such a test should SWEEP `amp` and assert the metric
 *        does not move — a stronger claim than any single amplitude, and only
 *        expressible because amplitude is one named field here instead of a
 *        constant baked into each harness.
 *
 *    Under-drive is the dangerous direction because nothing reports it.
 *    Measured and on record in `ratesync_core.h`: at `sps = 17.333`,
 *    quarter-amplitude input reads -21.6 dB EVM against -37.0 dB at unit — 15
 *    dB — with `lock_stat` 0.70 either way, because the loop really does lock
 *    and only the demodulation degrades. Over-drive IS flagged
 *    (`ratesync_get_clipped()`, a CIC bounds its input to +-1.0); there is no
 *    under-drive twin, tracked as gh-661. Until there is, ONE stimulus home is
 *    the defence: a level nobody can invent per-file is a level that cannot
 *    silently go wrong.
 * 2. **One timing origin.** Symbol `k` is centred at input sample
 *    `(k + span) * sps * rate + tau * sps`. The `span`-symbol lead-in means
 *    the first symbol's pulse is fully contained rather than truncated;
 *    `tau` is a fractional timing offset in SYMBOLS (not samples); `rate`
 *    scales symbol SPACING, which is how a sample-clock offset presents.
 * 3. **The symbol source is the library's PRBS**, not a private PRNG. A
 *    one-shot LCG of the symbol index (`x = k*A + C`) looks random per sample
 *    but is strongly periodic across consecutive `k`, and a run pattern with
 *    few independent transitions starves a Gardner detector: the eye
 *    statistic sits near zero and the loop cannot declare lock even while its
 *    output EVM is excellent. An MLS has ~50% transition density by
 *    construction and a period (2^15 - 1 = 32767) far longer than any record
 *    here.
 *
 * ## Linking
 *
 * Header-only, and its whole dependency closure is ONE component. Add a
 * **bare** dependency (not `link = true` — the test and bench need the
 * symbols, the `.so` does not) in `objects/<obj>.toml`:
 *
 *     depends_on = [ ..., "pn" ]
 *
 * The pulse shapes cost nothing: `wfm_rrc_h`, `wfm_rc_h` and
 * `wfm_synth_mls_poly` are all header-inline, so there is no `wfm` edge.
 *
 * Because the closure is just `pn`, every harness above `pn` may use this —
 * the single exclusion is `test_pn_core.c` itself, which must not take its
 * stimulus from the object it is testing.
 *
 * ## The Python twin
 *
 * Python does not get a translation of this file. Its stimulus SSOT is the
 * shipped generator — `doppler.wfm.Synth` / `Composer` — and its measurement
 * SSOT is `doppler.ber`. `Synth` covers everything except a real-valued `sps`
 * and a fractional timing offset (its `sps` is an `int` and it has no `tau`),
 * which is exactly why the analytic direct form below still has a job.
 */
#ifndef DP_TX_TEST_H
#define DP_TX_TEST_H

#include "pn/pn_core.h"               /* the canonical PRBS               */
#include "wfm/wfm_dsp.h"              /* wfm_rrc_h / wfm_rc_h — inline    */
#include "wfm_synth/wfm_synth_core.h" /* wfm_synth_mls_poly — inline      */

#include <complex.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

/** @brief LFSR register width for the symbol source; period 2^15 - 1. */
#define DP_TX_PN_LENGTH 15u

/** @brief Pulse shape. */
typedef enum
{
  DP_TX_RRC = 0, /**< root raised cosine — the transmit half of a
                      matched-filter pair, `wfm_rrc_h` */
  DP_TX_RC,      /**< full raised cosine — already Nyquist at the receiver,
                      i.e. TX*RX collapsed, `wfm_rc_h` */
  DP_TX_NRZ      /**< rectangular sample-and-hold, one symbol wide */
} dp_tx_pulse_t;

/**
 * @brief A transmit stream, described completely.
 *
 * Fill from dp_tx_defaults() and override what the test is about, so that
 * every field a test does NOT mention is provably the shared convention
 * rather than an accident.
 */
typedef struct
{
  dp_tx_pulse_t pulse; /**< shape; default DP_TX_RRC                        */
  double        sps;   /**< samples per symbol; REAL-valued, 17.333 is as
                            valid as 4                                      */
  double   beta;       /**< roll-off in (0, 1]; ignored by DP_TX_NRZ        */
  int      span;       /**< one-sided truncation, symbols; also the lead-in */
  double   tau;        /**< fractional timing offset, SYMBOLS               */
  double   rate;       /**< symbol-spacing scale; 1.0 = nominal clock       */
  double   amp;        /**< SYMBOL amplitude (see convention 1)             */
  double   fc;         /**< carrier, normalised cycles/sample; 0 = baseband */
  size_t   nsym;       /**< symbols to transmit; no default, say it         */
  uint32_t seed;       /**< PRBS seed; non-zero                             */
} dp_tx_cfg_t;

/** @brief The shared conventions, as a starting struct. `nsym` is 0 — a
 *  record length is never a default. */
static inline dp_tx_cfg_t
dp_tx_defaults (void)
{
  dp_tx_cfg_t c = { DP_TX_RRC, 4.0, 0.35, 8, 0.0, 1.0, 1.0, 0.0, 0, 7u };
  return c;
}

/**
 * @brief Fill `out[0..n)` with +-1 BPSK symbols from the library's MLS.
 *
 * Exposed separately because a BER harness needs the transmitted sequence to
 * score against, and it must be the SAME sequence the waveform carries.
 *
 * @param out   Destination, `n` elements, each set to -1 or +1.
 * @param n     Symbol count.
 * @param seed  Non-zero LFSR seed; 0 is coerced to 1 (the all-zero register
 *              is a fixed point).
 * @return      0 on success, -1 if the generator could not be created.
 */
static inline int
dp_tx_symbols (int8_t *out, size_t n, uint32_t seed)
{
  uint8_t    *chips = (uint8_t *)malloc (n ? n : 1);
  pn_state_t *p     = pn_create (wfm_synth_mls_poly (DP_TX_PN_LENGTH),
                                 seed ? seed : 1u, DP_TX_PN_LENGTH, 0);
  if (!chips || !p)
    {
      free (chips);
      pn_destroy (p);
      return -1;
    }
  pn_generate (p, n, chips, n);
  for (size_t k = 0; k < n; k++)
    out[k] = chips[k] ? (int8_t)1 : (int8_t)-1;
  pn_destroy (p);
  free (chips);
  return 0;
}

/**
 * @brief Synthesize the stream `cfg` describes.
 *
 * Direct-form: every symbol's pulse is evaluated analytically at every sample
 * inside its span. That is O(nsym * span * sps) rather than a filter's O(n),
 * and it is the point — a real-valued `sps` and a fractional `tau` have no
 * polyphase bank to select from, and a harness that needs them cannot get its
 * stimulus from `wfm_synth_*` (whose `sps` is an `int`). Everything below the
 * loop is a shipped primitive.
 *
 * @param cfg    Stream description; `cfg->nsym` must be non-zero.
 * @param syms   Optional out-parameter, `cfg->nsym` elements, receiving the
 *               transmitted +-1 sequence. NULL if the test does not score
 *               against it.
 * @param n_out  Receives the sample count written.
 * @return       Heap buffer of `*n_out` samples (caller frees), or NULL.
 */
static inline float _Complex *
dp_tx_make (const dp_tx_cfg_t *cfg, int8_t *syms, size_t *n_out)
{
  if (!cfg || !cfg->nsym || cfg->sps <= 0.0 || cfg->span < 1)
    return NULL;

  /* The record runs from the start of the `span`-symbol lead-in to one symbol
     period past the LAST symbol's centre. No trailing dead zone: a steady-
     state window is the back of the record, so zeros past the final symbol
     are scored as recovered symbols and read as a receiver defect. (Measured:
     an 8-symbol tail cost RateSync 12 dB of EVM at sps = 4 while it still
     locked 8/8 — the shape of a stimulus bug, not a loop bug.)

     Every margin here is in SYMBOL periods for the same reason: a constant
     tail of samples is a different number of dead symbols at every sps (an
     inherited `+ 64` was 16 dead symbols at sps = 4 and 3.7 at sps = 17.3,
     which is how it hid). */
  /* Symbol 0's origin. Scaled by `rate` so the NRZ branch below lands on the
     SAME origin as the shaped branch's `(k + span) * sps * rate`; an unscaled
     lead-in agrees only at rate == 1 and drifts silently away from it. */
  const double lead = (double)cfg->span * cfg->sps * cfg->rate;
  size_t       n = (size_t)(ceil (((double)cfg->nsym - 1.0 + (double)cfg->span)
                                  * cfg->sps * cfg->rate)
                            + ceil (cfg->sps))
                   + 2;
  float _Complex *x  = (float _Complex *)calloc (n, sizeof *x);
  int8_t         *sy = (int8_t *)malloc (cfg->nsym);
  if (!x || !sy || dp_tx_symbols (sy, cfg->nsym, cfg->seed) != 0)
    {
      free (x);
      free (sy);
      return NULL;
    }

  for (size_t i = 0; i < n; i++)
    {
      double a = 0.0;
      if (cfg->pulse == DP_TX_NRZ)
        {
          /* Sample-and-hold: the symbol whose one-symbol-wide rectangle
             covers this sample, on the same timing origin as the shaped
             forms. */
          double c = ((double)i - lead) / (cfg->sps * cfg->rate) - cfg->tau;
          if (c >= 0.0 && c < (double)cfg->nsym)
            a = sy[(size_t)c];
        }
      else
        {
          /* Only the symbols whose span covers sample i contribute. Solving
             |(i - ctr)/sps - tau| <= span for k bounds the inner loop at
             ~2*span/rate terms instead of nsym; the fabs() guard below still
             decides membership, so the bound is a speed-up and not a second
             definition of the window. */
          double kc = ((double)i / (cfg->sps * cfg->rate)) - (double)cfg->span;
          double kw = ((double)cfg->span + fabs (cfg->tau)) / cfg->rate + 1.0;
          long   k0 = (long)floor (kc - kw);
          long   k1 = (long)ceil (kc + kw);
          if (k0 < 0)
            k0 = 0;
          if (k1 > (long)cfg->nsym)
            k1 = (long)cfg->nsym;
          for (long k = k0; k < k1; k++)
            {
              /* Symbol k's centre, in symbol periods away from sample i. */
              double ctr
                  = ((double)k + (double)cfg->span) * cfg->sps * cfg->rate;
              double t = ((double)i - ctr) / cfg->sps - cfg->tau;
              if (fabs (t) > (double)cfg->span)
                continue;
              a += sy[k]
                   * (cfg->pulse == DP_TX_RC ? wfm_rc_h (t, cfg->beta)
                                             : wfm_rrc_h (t, cfg->beta));
            }
        }

      if (cfg->fc != 0.0)
        {
          double ph = 2.0 * M_PI * cfg->fc * (double)i;
          x[i] = (float _Complex) (cfg->amp * a * (cos (ph) + I * sin (ph)));
        }
      else
        {
          x[i] = (float)(cfg->amp * a);
        }
    }

  if (syms)
    for (size_t k = 0; k < cfg->nsym; k++)
      syms[k] = sy[k];
  free (sy);
  *n_out = n;
  return x;
}

#endif /* DP_TX_TEST_H */
