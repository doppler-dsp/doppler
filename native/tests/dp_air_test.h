/**
 * @file dp_air_test.h
 * @brief A scene of continuous-DSSS emitters on the air, at any input rate,
 *        with the truth a measurement subtracts.
 *
 * The receiver's use case (docs/design/async-dsss-receiver.md, the
 * 415.1-B-1 point) is an arbitrary input rate that is deliberately not a
 * multiple of the chip rate: 11 MSa/s against 3.2147 or 4.0913 Mcps, 3.42
 * or 2.69 samples a chip. No shipped generator renders a fractional number
 * of samples a chip, so the scene is the front end's own model, written
 * once here rather than once per test, validator and bench:
 *
 *   emitter k: wfm_synth (continuous DSSS, its data PRBS, its code-only
 *              window) at `spc` whole samples a chip
 *           -> a fixed-ratio RateConverter to `fs`   (the front end)
 *           -> a DopplerChannel at its carrier       (time base + carrier)
 *           -> times its amplitude, summed
 *   plus     the shipped awgn, sized by dp_awgn_amplitude_for_snr()
 *
 * Every stage is a shipped object; nothing here renders a chip, a phase or a
 * noise sample by hand.
 *
 * ## The front end's band
 *
 * The RateConverter is the band limit, and `spc` decides which band. Its
 * one resampler stage is a 60 dB Kaiser bank whose pass and stop edges are
 * 0.4 and 0.6 of the SLOWER side's rate. At 4 samples a chip it decimates
 * to `fs`: it passes to 0.4 fs and stops from 0.6 fs, a front end's
 * anti-alias filter about the output's own Nyquist -- the model -- and at
 * both 415.1 carriers that is one stage, whose delay is published
 * (dp_RateConverter_get_group_delay()). At 2 it interpolates from the
 * generation rate, and the held chips' images inside its transition band
 * come through: the scene then keeps 0.9993 of the chip power, where 4
 * keeps 0.946 (2.2 GHz) and 0.917 (2.8 GHz) (test_dp_air.c). At 8 the plan
 * gains a halfband stage, whose delay is not published, and the code truth
 * below would be unknown. So DP_AIR_SPC is 4.
 *
 * ## Level
 *
 * The noise is sized for a unit-power emitter at `ref_cn0_dbhz` over `fs`;
 * each emitter is scaled to its own `cn0_dbhz` against it, so a level is
 * always a C/N0, both rails counted (#689). It is the emitter's C/N0 at the
 * antenna, before the front end: the band keeps the chip spectrum's
 * in-band fraction, 0.946 at 2.2 GHz and 0.917 at 2.8 GHz for a brickwall
 * at fs/2 (0.24 and 0.38 dB less), a little more through the resampler
 * (test_dp_air.c).
 *
 * ## Truth
 *
 * dp_air_truth() says, for receive sample `k`, the carrier phase the
 * channel applied there (doppler_channel_phase(), the channel's own closed
 * form) and the code phase the emitter's chips are at: the receive time
 * mapped back through the channel's dilation and delay
 * (doppler_channel_excess(), dp_doppler_channel_get_delay_samples()), the
 * converter's group delay and the emitter's lag, onto the synth's own chip
 * clock. A receiver's ".phase.car" and ".phase.code" probes, stamped at the
 * same sample, are compared against these.
 *
 * Allocates; dp_air_free() releases.
 */
#ifndef DP_AIR_TEST_H
#define DP_AIR_TEST_H

#include "doppler/RateConverter/RateConverter_core.h"
#include "doppler/awgn/awgn_core.h"
#include "doppler/doppler_channel/doppler_channel_core.h"
#include "doppler/dp_complex.h"
#include "doppler/wfm_synth/wfm_synth_core.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/** Emitters a scene holds at most. */
#define DP_AIR_MAX_K 16
/** Whole samples a chip the synth renders at (see "The front end's band"). */
#define DP_AIR_SPC 4u
/** Synth samples rendered per step of the chain. */
#define DP_AIR_CHUNK 4096u

/** One emitter: its waveform, where it sits on the air, and its level. */
typedef struct
{
  const uint8_t *code;          /**< spreading code, 0/1 chips            */
  size_t         code_len;      /**< chips a period                       */
  double         chip_rate;     /**< chips a second, nominal              */
  double         sym_rate;      /**< data symbols a second, nominal       */
  double         carrier_hz;    /**< RF carrier the Doppler is referred to */
  double         f0_hz;         /**< carrier offset at receive time 0, Hz */
  double         rate_hz_s;     /**< its ramp, Hz/s                       */
  double         cn0_dbhz;      /**< its C/N0, dB-Hz                      */
  size_t         code_only_sym; /**< code-only symbols a frame (0: none)  */
  size_t         frame_sym;     /**< symbols a frame (0: no frame)        */
  size_t         frame_off;     /**< symbols into its frame at its first
                                     chip: the drawn frame start          */
  double         lag_s;         /**< silence before its first chip, s     */
  uint32_t       seed;          /**< its data PRBS                        */
  const uint8_t *bits;          /**< its payload, 0/1, or NULL for PRBS:
                                     one bit per DATA symbol (the window's
                                     symbols take none), sent once      */
  size_t   n_bits;              /**< payload bits                         */
  unsigned spc; /**< whole samples a chip it is rendered at; 0 means
                     DP_AIR_SPC. To show what the front end's band does:
                     at 2 the converter interpolates and the held chips'
                     images come through its transition band */
} dp_air_emitter_t;

/** A scene: emitters, the rate they are received at, and the noise. */
typedef struct
{
  int              k;            /**< emitters                 */
  double           fs;           /**< receive rate, Hz         */
  double           ref_cn0_dbhz; /**< noise reference          */
  dp_air_emitter_t em[DP_AIR_MAX_K];
  float            amp[DP_AIR_MAX_K];
  double           fs_gen[DP_AIR_MAX_K];     /**< synth rate, Hz   */
  unsigned         spc[DP_AIR_MAX_K];        /**< samples a chip   */
  double           skip_chips[DP_AIR_MAX_K]; /**< frame start */
  size_t           lag[DP_AIR_MAX_K];        /**< silence left, synth
                                                  samples             */
  dp_wfm_synth_state_t       *syn[DP_AIR_MAX_K];
  dp_RateConverter_state_t   *rc[DP_AIR_MAX_K];
  dp_doppler_channel_state_t *ch[DP_AIR_MAX_K];
  float _Complex             *buf[DP_AIR_MAX_K];
  size_t                      len[DP_AIR_MAX_K], cap;
  dp_awgn_state_t            *noise; /**< NULL: no noise               */
  float _Complex             *gen, *mid;
  size_t                      mid_cap;
  uint64_t                    n_out; /**< receive samples produced       */
} dp_air_t;

/** Release a scene (safe on a partly built one). */
static inline void
dp_air_free (dp_air_t *a)
{
  for (int i = 0; i < DP_AIR_MAX_K; i++)
    {
      dp_wfm_synth_destroy (a->syn[i]);
      dp_RateConverter_destroy (a->rc[i]);
      dp_doppler_channel_destroy (a->ch[i]);
      free (a->buf[i]);
    }
  dp_awgn_destroy (a->noise);
  free (a->gen);
  free (a->mid);
  memset (a, 0, sizeof *a);
}

/**
 * Build a scene of `k` emitters received at `fs`, the noise sized for a
 * unit-power emitter at `ref_cn0_dbhz` (NAN: no noise). `block` is the
 * most dp_air_next() is asked for at once. Returns 1, or 0 with the scene
 * freed on a refusal from any shipped object.
 */
static inline int
dp_air_init (dp_air_t *a, const dp_air_emitter_t *em, int k, double fs,
             double ref_cn0_dbhz, uint32_t noise_seed, size_t block)
{
  memset (a, 0, sizeof *a);
  if (k < 0 || k > DP_AIR_MAX_K)
    return 0;
  a->k            = k;
  a->fs           = fs;
  a->ref_cn0_dbhz = ref_cn0_dbhz;
  a->cap          = block + (size_t)8 * DP_AIR_CHUNK;
  a->gen          = malloc (DP_AIR_CHUNK * sizeof *a->gen);
  a->mid_cap      = (size_t)4 * DP_AIR_CHUNK;
  a->mid          = malloc (a->mid_cap * sizeof *a->mid);
  if (!a->gen || !a->mid)
    goto fail;
  if (!isnan (ref_cn0_dbhz))
    {
      a->noise = dp_awgn_create (
          noise_seed, dp_awgn_amplitude_for_snr (
                          (float)(ref_cn0_dbhz - 10.0 * log10 (fs)), 1.0f));
      if (!a->noise)
        goto fail;
    }
  for (int i = 0; i < k; i++)
    {
      const dp_air_emitter_t *e = &em[i];
      a->em[i]                  = *e;
      a->spc[i]                 = e->spc ? e->spc : DP_AIR_SPC;
      a->fs_gen[i]              = (double)a->spc[i] * e->chip_rate;
      a->amp[i]                 = (float)pow (
          10.0,
          (e->cn0_dbhz - (isnan (ref_cn0_dbhz) ? e->cn0_dbhz : ref_cn0_dbhz))
              / 20.0);
      a->syn[i] = dp_wfm_synth_create (WFM_SYNTH_DSSS, a->fs_gen[i], 0.0,
                                       WFM_SYNTH_SNR_CLEAN, 1, e->seed,
                                       (int)a->spc[i], 15, 0, 0, 0.0);
      a->rc[i]  = dp_RateConverter_create (fs / a->fs_gen[i], 0);
      a->ch[i]  = dp_doppler_channel_create (
          fs, e->carrier_hz, e->f0_hz / e->carrier_hz * 1e6,
          e->rate_hz_s / e->carrier_hz * 1e6);
      a->buf[i] = malloc (a->cap * sizeof *a->buf[i]);
      if (!a->syn[i] || !a->rc[i] || !a->ch[i] || !a->buf[i]
          || dp_wfm_synth_set_dsss_cont (
                 a->syn[i], e->code, e->code_len, e->chip_rate / e->sym_rate,
                 e->bits ? WFM_DSSS_DATA_BITS : WFM_DSSS_DATA_PRBS, e->bits,
                 e->bits ? e->n_bits : 0)
                 != 0)
        goto fail;
      if (e->frame_sym
          && dp_wfm_synth_set_dsss_window (a->syn[i], e->code_only_sym,
                                           e->frame_sym)
                 != 0)
        goto fail;
      /* The drawn frame start: the synth's first `frame_off` symbols are
         rendered and dropped, whole chips, so its chip counter starts there
         -- which the truth adds back. */
      const double cps   = e->chip_rate / e->sym_rate;
      const size_t chips = (size_t)ceil ((double)e->frame_off * cps);
      size_t       drop  = chips * a->spc[i];
      a->skip_chips[i]   = (double)chips;
      while (drop)
        {
          const size_t m = drop < DP_AIR_CHUNK ? drop : DP_AIR_CHUNK;
          dp_wfm_synth_steps (a->syn[i], a->gen, m);
          drop -= m;
        }
      a->lag[i] = (size_t)llround (e->lag_s * a->fs_gen[i]);
    }
  return 1;
fail:
  dp_air_free (a);
  return 0;
}

/** The next `n` receive samples of the scene into `x`; 0 on a refusal. */
static inline int
dp_air_next (dp_air_t *a, float _Complex *x, size_t n)
{
  if (a->noise)
    (void)dp_awgn_generate (a->noise, n, x, n);
  else
    memset (x, 0, n * sizeof *x);
  for (int i = 0; i < a->k; i++)
    {
      while (a->len[i] < n)
        {
          size_t m = DP_AIR_CHUNK / 2;
          if (a->lag[i])
            { /* the emitter's silence ahead of its first chip */
              m = a->lag[i] < m ? a->lag[i] : m;
              memset (a->gen, 0, m * sizeof *a->gen);
              a->lag[i] -= m;
            }
          else
            dp_wfm_synth_steps (a->syn[i], a->gen, m);
          const size_t r    = dp_RateConverter_execute (a->rc[i], a->gen, m,
                                                        a->mid, a->mid_cap);
          const size_t room = a->cap - a->len[i];
          if (room < r + 64)
            return 0;
          a->len[i] += dp_doppler_channel_execute (
              a->ch[i], a->mid, r, a->buf[i] + a->len[i], room);
        }
      for (size_t j = 0; j < n; j++)
        x[j] += a->amp[i] * a->buf[i][j];
      a->len[i] -= n;
      memmove (a->buf[i], a->buf[i] + n, a->len[i] * sizeof *a->buf[i]);
    }
  a->n_out += n;
  return 1;
}

/** What emitter `i` is at receive sample `k`. */
typedef struct
{
  double car_cycles; /**< carrier phase the channel applied, cycles      */
  double car_hz;     /**< the carrier offset there, Hz                    */
  double code_chips; /**< code phase, chips in [0, code_len); NAN before
                          its first chip                                  */
  double tx_chips;   /**< chips the emitter has sent by then (unwrapped) */
  long   symbol;     /**< data symbol index from its frame's start        */
} dp_air_truth_t;

/**
 * The truth for emitter `i` at receive sample `k`: the channel's carrier
 * phase there, and the chip it is sending, mapped back through the
 * channel's dilation and delay, the converter's group delay and the
 * emitter's lag. Exact to the shipped objects' own closed forms. The
 * synth's samples are point values, so chip c -- its samples c spc to
 * (c + 1) spc - 1 -- spans (c spc - 0.5, (c + 1) spc - 0.5) samples once
 * the converter interpolates them: chip phase is (sample + 0.5) / spc.
 * test_dp_air.c pins it by the correlation peak's symmetry about it.
 */
static inline dp_air_truth_t
dp_air_truth (const dp_air_t *a, int i, uint64_t k)
{
  const dp_doppler_channel_state_t *ch = a->ch[i];
  const dp_air_emitter_t           *e  = &a->em[i];
  const double                      t  = (double)k / a->fs;
  dp_air_truth_t                    o;
  o.car_cycles = doppler_channel_phase (ch, t);
  o.car_hz
      = e->carrier_hz * (ch->doppler_ppm + ch->doppler_rate_ppm_s * t) * 1e-6;
  /* Receive time -> the channel's input time -> the converter's input time
     -> the synth's own sample count, less the emitter's silence. */
  const double t_ch = t + doppler_channel_excess (ch, t)
                      - dp_doppler_channel_get_delay_samples (ch) / a->fs;
  const double t_gen
      = t_ch - dp_RateConverter_get_group_delay (a->rc[i]) / a->fs_gen[i];
  const double s
      = t_gen * a->fs_gen[i] - (double)llround (e->lag_s * a->fs_gen[i]);
  /* The synth's samples are point values: chip c is samples c spc to
     (c + 1) spc - 1, so as the converter interpolates them the chip spans
     half a sample either side, (c spc - 0.5, (c + 1) spc - 0.5). */
  o.tx_chips = (s + 0.5) / (double)a->spc[i] + a->skip_chips[i];
  if (s < 0.0)
    {
      o.code_chips = NAN;
      o.symbol     = -1;
      return o;
    }
  o.code_chips = fmod (o.tx_chips, (double)e->code_len);
  /* The synth's symbol edges: symbol n starts at chip n * cps. */
  o.symbol = (long)floor (o.tx_chips / (e->chip_rate / e->sym_rate));
  return o;
}

/**
 * Whether any emitter given a payload has run out of it: the synth sends a
 * payload once and then silence (doppler#1718), so a run measured past the
 * end measures nothing. A caller that gives `bits` asserts this is 0
 * after its run.
 */
static inline int
dp_air_data_ended (const dp_air_t *a)
{
  for (int i = 0; i < a->k; i++)
    if (a->em[i].bits && dp_wfm_synth_data_ended (a->syn[i]))
      return 1;
  return 0;
}

/**
 * The bit emitter `i` sends in symbol `n`, counted from its frame's start
 * (dp_air_truth()'s `symbol`), for a payload given as `bits`: 0 in a
 * code-only symbol (the code alone), the payload bit the synth reads there
 * in a data symbol -- one per data symbol before it, the window's taking
 * none -- and -1 past the payload's end (the synth then sends silence) or
 * for a PRBS emitter, whose bits only the synth knows.
 */
static inline int
dp_air_symbol_bit (const dp_air_t *a, int i, long n)
{
  const dp_air_emitter_t *e = &a->em[i];
  if (!e->bits || n < 0)
    return -1;
  size_t p = (size_t)n; /* data symbols before n */
  if (e->frame_sym)
    {
      const size_t f = e->frame_sym, c = e->code_only_sym;
      const size_t q = (size_t)n % f;
      if (q < c)
        return 0;
      p = ((size_t)n / f) * (f - c) + (q - c);
    }
  return p < e->n_bits ? (int)(e->bits[p] != 0) : -1;
}

#endif /* DP_AIR_TEST_H */
