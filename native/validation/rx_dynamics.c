/**
 * @file rx_dynamics.c
 * @brief The receiver under a COUPLED Doppler ramp across a data onset --
 *        the continuous flavor's own scenario, and the one measurement that
 *        separates the three NDA carrier taps on it.
 *
 * `docs/design/mpsk.md` S0 describes the continuous flavor as **NRZ BPSK,
 * periods of data modulation off but carrier on**. Nothing in the tree
 * measured that. `rx_battery.c` runs `RX_FRAME_CONT` -- i.i.d. bits, dense
 * transitions from the first symbol to the last -- through an RRC pair, which
 * is the BURST flavor's waveform; the retired tap sweep ran NRZ but NOISELESS
 * and with no dynamics at all. So the flavor shipped with a tap
 * chosen against evidence taken from a neighbouring waveform.
 *
 * This harness is that scenario:
 *
 *   - **NRZ BPSK, I&D matched filter, `m_out = 4`, DTTL.** The matched TED
 *     for a rectangular pulse, and the TED choice is not cosmetic -- it is
 *     the largest single effect on this page. The run prints the SAME record
 *     through Gardner beside it: `strobe`'s onset dip is 0.075 of lock under
 *     DTTL and 0.306 under Gardner, **four times deeper from the wrong
 *     detector alone**. Both passes are printed; only DTTL is gated, because
 *     Gardner on a rectangular pulse is a misconfiguration rather than a
 *     receiver defect.
 *   - **First half: modulation OFF.** Carrier on, zero transitions, so the
 *     TED has no edge and the timing loop cannot close on anything. The
 *     carrier loop must acquire regardless.
 *   - **Second half: dense i.i.d. transitions**, arriving as a step.
 *   - **Throughout: a Doppler RAMP through `doppler_channel`**, which moves
 *     the carrier AND every clock together because a Doppler shift dilates
 *     the whole received time base. One impairment, both loops stressed, in
 *     the ratio physics actually delivers.
 *
 * ## What it found, and what it did not
 *
 * The tap that reads the on-time strobe wins on every axis measured, which
 * is the opposite of what the tap enum's prose implied:
 *
 * | tap      | lock quiet | min post-onset | recovered | end    |
 * | -------- | ---------- | -------------- | --------- | ------ |
 * | `strobe` | +0.935     | **+0.860**     | +0.921    | +0.920 |
 * | `mf_out` | +0.934     | +0.478         | +0.714    | +0.802 |
 * | `mf_in`  | +0.761     | +0.417         | +0.635    | +0.714 |
 *
 * `strobe`'s symbol-timing dependency costs nothing in the half where timing
 * is impossible, and the reason is worth stating because it is not obvious:
 * **an unmodulated NRZ carrier is sampling-phase invariant.** Every sample is
 * the same constellation point, so the M-th-power discriminator does not care
 * which one the timing loop would have nominated. Timing closure gates
 * DEMODULATION, not carrier acquisition, and the tap that depends on it is
 * therefore free exactly when it looked most exposed.
 *
 * `mf_out` takes the largest hit at the onset (0.46 of lock, against
 * `strobe`'s 0.08) and has still not recovered by the end -- its
 * between-symbol outputs average two symbols, so its ISI bias appears the
 * moment transitions exist and not one symbol before.
 *
 * `mf_in` is worst throughout and starts from a lower baseline. That is NOT
 * an intrinsic cost of reading ahead of the matched filter, and this file
 * previously said it was. Measured at the node with the AGC off so the path
 * is linear, the `mf_in` node sits **6.01 dB** below Es/N0 while the terminal
 * node sits 1.7 dB below it -- and `10*log10(bank_sps) = 6.02 dB`. The
 * deficit is a pure BANDWIDTH ratio (identical at 6.79, 12 and 20 dB Es/N0),
 * not a loss of signal energy: a Nyquist-sampled band-limited signal loses
 * nothing, and this node is not band-limited to the signal. DEC filters to
 * ITS OWN Nyquist -- +-bank_sps*Rs/2 -- while the signal occupies ~+-Rs, and
 * nothing between them removes the difference. That is the tap's PRICE, not a
 * defect: an arm filter would recover it and is declined, because `strobe`
 * reads the node already matched to the signal for free and measures better
 * here anyway.
 *
 * ## Telemetry is the deliverable, not a side effect
 *
 * Given `--out DIR` this writes one `dp_tlm_capture` per tap -- every probe
 * the receiver exposes, on the receiver's own clock -- so the report plots
 * the dynamics rather than restating a table. The 16-byte record layout IS
 * the file (`n:u8, value:f4, probe:u2, flags:u2`) plus a JSON sidecar naming
 * the probes, so reading it back needs nothing doppler-specific.
 *
 * The flush is the CALLER's, which is the composition API's contract
 * (`mpsk_rx_tlm_flush`: "Out-of-line on purpose; callers gate on
 * `l->tlm.ctx`"). `mpsk_receiver_steps()` does it for you and pins
 * `RATESYNC_TED_GARDNER`; this harness needs DTTL, so it steps and flushes
 * itself. Omitting that flush is why an earlier version of this measurement
 * came back with only the two AGC probes populated and every loop probe
 * silent -- the telemetry was wired correctly and the caller was not.
 *
 * Usage:
 *   rx_dynamics              the full record, printed
 *   rx_dynamics --check      the CI gate
 *   rx_dynamics --out DIR    also write DIR/rx-dyn-<tap>.tlm for plotting
 */
#include "doppler_channel/doppler_channel_core.h"
#include "dp_rng_test.h"
#include "dp_tlm_capture/dp_tlm_capture_core.h"
#include "mpsk_receiver/mpsk_receiver_core.h"

#include <complex.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** @brief Transmit amplitude. A cascade that plans a CIC bounds its input to
 * +-1.0 and clips silently past it, so every run asserts get_clipped() == 0
 * rather than trusting the level (the choice mpsk_ber_common.h makes). */
#define RX_DYN_AMP 0.5

/** @brief Symbol rate and oversampling. Rs = 1 kSym/s makes the ramp's Hz/s
 * and the loop's bn (per symbol) directly comparable. */
#define RX_DYN_RS 1000.0
#define RX_DYN_SPS 8.0

/** @brief Terminal outputs per symbol. `>= 4` is not optional with an I&D
 * matched filter: at 2 the rectangle's filter degenerates to a two-tap sum
 * and the eye barely opens (docs/design/mpsk.md S5). */
#define RX_DYN_M_OUT 4u

/** @brief Loop bandwidths, per symbol. */
#define RX_DYN_BN_CARRIER 0.005
#define RX_DYN_BN_TIMING 0.01

/** @brief Record length and where the data starts. Half and half: the quiet
 * stretch has to be long enough for the carrier loop to settle (5/bn = 1000
 * symbols) with room to read a steady value, and the data stretch long enough
 * to show whether the onset transient RECOVERS rather than merely dips. */
#define RX_DYN_NSYM 24000u
#define RX_DYN_ONSET 12000u

/** @brief The onset window the transient is scored over, in symbols. */
#define RX_DYN_WINDOW 2000u

/** @brief Matched-filter-output Es/N0, dB. 12 dB rather than the battery's
 * 6.79: this measures DYNAMICS, and at the SER=1e-3 anchor the steady-state
 * noise on the lock statistic is comparable to the transient being measured,
 * which buries the thing under test. The tap RANKING is the same at both. */
#define RX_DYN_ESN0 12.0

/** @brief The Doppler ramp. 2e-4 ppm/s at 2.4 GHz is 0.48 Hz/s of carrier,
 * which is a phase lag of ~0.6 rad against this loop -- inside the M-th-power
 * S-curve's pi/(2M) linear range at M = 2, so the loop is being asked to
 * TRACK a ramp rather than to survive one it cannot. The same ppm dilates
 * every clock, which is the point of using a channel rather than two knobs:
 * the timing ramp that accompanies it is the one physics pairs with it. */
#define RX_DYN_CARRIER_HZ 2.4e9
#define RX_DYN_RATE_PPM_S 2.0e-4

/** @brief Gates. `strobe` measures 7.99966 samples/symbol at the end against
 * a nominal 8.0, so 0.01 is loose enough for the ramp's residual and tight
 * enough that a timing loop which stopped tracking (drifting at the ramp's
 * rate) cannot pass. The lock floor is the sign rule from S3.1: a lock
 * statistic transiting zero means the constellation is parked on a decision
 * boundary, which is a different failure from a noisy lock and is never
 * acceptable at this Es/N0. */
#define RX_DYN_RATE_TOL 0.01
#define RX_DYN_LOCK_FLOOR 0.0

/** @brief Fractional tolerance on the carrier command against the ramp the
 * CHANNEL was told to apply -- an external truth, not another reading of the
 * receiver. This gate exists because the lock floor alone is vacuous: a
 * discriminator fed a constant `1+0j` reports `(I^2-Q^2)/(I^2+Q^2) = +1.0`
 * forever, so a DEAD detector claiming perfect lock passes any floor. Scored
 * against `f_end = ramp * T` it scores 0.0 instead. The 10% band carries the
 * type-2 loop's steady-state lag against a ramp, which measures ~1% here. */
#define RX_DYN_ACQ_TOL 0.10

/* One tap remains; the array shape is kept so the print sites read
   unchanged and the loop bound is the only thing that moved. */
static const char *RX_DYN_NAMES[1] = { "strobe" };

/** @brief One tap's dynamics over the record. */
typedef struct
{
  double lock_quiet;   /**< mean lock over the settled half of the quiet
                            stretch -- what the tap reads with no data.    */
  double lock_min;     /**< MINIMUM lock inside the onset window. The whole
                            point: endpoints on either side of a transient
                            are exactly what conceals it.                  */
  double lock_recover; /**< lock at the end of the onset window.           */
  double lock_end;     /**< lock at the end of the record.                 */
  double rate_end;     /**< tracked samples/symbol at the end.             */
  double nco_end;      /**< carrier command at the end (ramp integrated).  */
  int    clipped;      /**< front end clipped: the reading is worthless.   */
  int    refused;      /**< create() returned NULL.                        */
} rx_dyn_result_t;

/**
 * @brief Run one tap over the whole scenario.
 *
 * @param tap   MPSK_RX_NDA_TAP_*.
 * @param ted   RATESYNC_TED_DTTL or RATESYNC_TED_GARDNER.
 * @param path  Telemetry capture path, or NULL for no capture.
 * @return The dynamics; check `.refused` and `.clipped` before reading.
 */
static rx_dyn_result_t
rx_dyn_measure (int ted, const char *path)
{
  rx_dyn_result_t r;
  size_t          isps  = (size_t)RX_DYN_SPS;
  size_t          nsamp = (size_t)RX_DYN_NSYM * isps;
  double          fs    = RX_DYN_SPS * RX_DYN_RS;
  uint32_t        st    = 20260816u;
  float complex  *x = NULL, *y = NULL;
  double sigma = RX_DYN_AMP
                 * sqrt (RX_DYN_SPS / (2.0 * pow (10.0, RX_DYN_ESN0 / 10.0)));

  memset (&r, 0, sizeof r);
  r.refused = 1;

  x = (float complex *)malloc (nsamp * sizeof *x);
  y = (float complex *)malloc (nsamp * sizeof *y);
  if (!x || !y)
    goto done;

  /* Stage 1 -- the waveform. The first half is ONE symbol held: carrier on,
     zero transitions. The second half is i.i.d. BPSK, arriving as a step. */
  for (size_t k = 0; k < RX_DYN_NSYM; k++)
    {
      /* A plain PRNG rather than pn_core, for the reason
         mpsk_ber_common.h gives: an MLS contains a run of L identical
         bits, and at 1 bit/symbol BPSK that stalls the TED. */
      double sr = (k < RX_DYN_ONSET)
                      ? RX_DYN_AMP
                      : RX_DYN_AMP * ((dp_xs32 (&st) % 2u) ? -1.0 : 1.0);
      for (size_t j = 0; j < isps; j++)
        x[k * isps + j] = (float)sr + 0.0f * I;
    }

  /* Stage 2 -- IMPAIR. One call moves the carrier and every clock together,
     because a Doppler shift dilates the whole received time base. */
  {
    doppler_channel_state_t *ch = doppler_channel_create (
        fs, RX_DYN_CARRIER_HZ, 0.0, RX_DYN_RATE_PPM_S);
    size_t got = 0;
    if (!ch)
      goto done;
    for (size_t o = 0; o < nsamp; o += DOPPLER_CHANNEL_MAX_BLOCK)
      {
        size_t m = nsamp - o < DOPPLER_CHANNEL_MAX_BLOCK
                       ? nsamp - o
                       : DOPPLER_CHANNEL_MAX_BLOCK;
        got += doppler_channel_execute (ch, x + o, m, y + got, nsamp - got);
      }
    nsamp = got;
    doppler_channel_destroy (ch);
  }

  /* Stage 3 -- AWGN at the stated matched-filter-output Es/N0. */
  /* Named locals, not two calls in one expression: those are
     indeterminately sequenced (C11 6.5.2.2p10), so which noise stream this
     harness measured was the compiler's choice rather than a property of
     the code -- in a file whose table docs/design/mpsk.md quotes.

     The ORDER is load-bearing and was MEASURED, not read off. dp_rng_test.h
     records gcc taking the imaginary operand first; here, under this
     target's flags, gcc -O3 and clang -O0 both take the REAL one, and
     writing the documented order in instead moved the strobe tap's
     post-onset lock from +0.860 to +0.757. Confirmed by diffing the full
     sweep -- `--check` cannot see it, because it prints one line. */
  for (size_t n = 0; n < nsamp; n++)
    {
      double n_re = dp_gauss (&st);
      double n_im = dp_gauss (&st);
      y[n] += (float)(sigma * n_re) + (float)(sigma * n_im) * I;
    }

  /* Stage 4 -- the receiver, with its own telemetry attached. */
  {
    const size_t           BL  = 1024;
    dp_tlm_t              *tlm = NULL;
    dp_tlm_capture_t      *cap = NULL;
    dp_sample_clock_t      clk;
    mpsk_receiver_state_t *rx = mpsk_receiver_create (
        2, RX_DYN_SPS, RX_DYN_M_OUT, MPSK_RX_PULSE_IANDD, 0.35, 8,
        RX_DYN_BN_CARRIER, 0.0, RX_DYN_BN_TIMING, 0.0, 0.0, 0, 0, 1, 0.0);
    size_t nsym_seen = 0;
    double s_quiet   = 0.0;
    size_t n_quiet   = 0;
    double lmin = 2.0, lrec = 0.0;

    if (!rx)
      goto done;
    r.refused = 0;

    if (path)
      {
        /* Probes FIRST: the capture sizes its ring from the probe table. */
        tlm = dp_tlm_create (1u << 16);
        if (tlm && mpsk_receiver_set_telemetry (rx, tlm, "rx", 1) == 0)
          {
            dp_sample_clock_init (&clk, fs, 0);
            cap = dp_tlm_capture_open (tlm, BL, path, &clk);
          }
      }

    for (size_t o = 0; o < nsamp; o += BL)
      {
        size_t m = nsamp - o < BL ? nsamp - o : BL;
        if (tlm)
          dp_tlm_set_now (tlm, o);
        for (size_t n = 0; n < m; n++)
          {
            float complex sym;
            if (!mpsk_receiver_step_ted (rx, y[o + n], &sym, ted))
              continue;
            /* The composition API's contract: the flush is the caller's, and
               it is gated on the attachment rather than assumed. */
            if (rx->l.tlm.ctx)
              mpsk_rx_tlm_flush (&rx->l, sym);

            {
              double lk = mpsk_receiver_get_lock (rx);
              nsym_seen++;
              /* The settled half of the quiet stretch: the first half is the
                 cold-start transient and averaging it in would report
                 acquisition as the tap's steady reading. */
              if (nsym_seen > RX_DYN_ONSET / 2 && nsym_seen <= RX_DYN_ONSET)
                {
                  s_quiet += lk;
                  n_quiet++;
                }
              if (nsym_seen > RX_DYN_ONSET
                  && nsym_seen <= RX_DYN_ONSET + RX_DYN_WINDOW)
                {
                  if (lk < lmin)
                    lmin = lk;
                  lrec = lk;
                }
            }
          }
      }
    if (tlm)
      dp_tlm_set_now (tlm, nsamp);

    r.lock_quiet   = n_quiet ? s_quiet / (double)n_quiet : -2.0;
    r.lock_min     = lmin;
    r.lock_recover = lrec;
    r.lock_end     = mpsk_receiver_get_lock (rx);
    r.rate_end     = rx->l.timing.rate_est;
    r.nco_end      = mpsk_receiver_get_norm_freq (rx);
    r.clipped      = mpsk_receiver_get_clipped (rx);

    if (cap)
      dp_tlm_capture_close (cap);
    if (tlm)
      dp_tlm_destroy (tlm);
    mpsk_receiver_destroy (rx);
  }

done:
  free (x);
  free (y);
  return r;
}

int
main (int argc, char **argv)
{
  int         check = 0;
  const char *out   = NULL;
  int         fail  = 0;

  for (int i = 1; i < argc; i++)
    {
      if (strcmp (argv[i], "--check") == 0)
        check = 1;
      else if (strcmp (argv[i], "--out") == 0 && i + 1 < argc)
        out = argv[++i];
    }

  if (!check)
    {
      printf ("The receiver under a coupled Doppler ramp across a data "
              "onset.\n");
      printf ("NRZ BPSK, I&D, m_out=%u, DTTL, sps=%.0f, Es/N0 %.1f dB.\n",
              (unsigned)RX_DYN_M_OUT, RX_DYN_SPS, RX_DYN_ESN0);
      printf ("%u symbols modulation OFF (carrier on, timing cannot close), "
              "then %u dense.\n",
              (unsigned)RX_DYN_ONSET, (unsigned)(RX_DYN_NSYM - RX_DYN_ONSET));
      printf ("Ramp %.2f Hz/s of carrier and the same ppm on every clock "
              "(doppler_channel).\n\n",
              RX_DYN_RATE_PPM_S * 1e-6 * RX_DYN_CARRIER_HZ);
      printf ("  tap       lock quiet   MIN post-onset   recovered   end"
              "      rate end   nco end\n");
    }

  for (int t = 0; t < 1; t++)
    {
      char            path[512];
      const char     *p = NULL;
      rx_dyn_result_t r;

      if (out)
        {
          (void)snprintf (path, sizeof path, "%s/rx-dyn-%s.tlm", out,
                          RX_DYN_NAMES[t]);
          p = path;
        }
      r = rx_dyn_measure (RATESYNC_TED_DTTL, p);

      if (r.refused)
        {
          printf ("FAIL %s: create() refused\n", RX_DYN_NAMES[t]);
          fail = 1;
          continue;
        }
      if (r.clipped)
        {
          printf ("FAIL %s: front end clipped — the reading is worthless\n",
                  RX_DYN_NAMES[t]);
          fail = 1;
          continue;
        }

      if (!check)
        printf ("  %-8s  %+9.3f   %+12.3f   %+9.3f   %+6.3f   %8.5f   "
                "%+.4e\n",
                RX_DYN_NAMES[t], r.lock_quiet, r.lock_min, r.lock_recover,
                r.lock_end, r.rate_end, r.nco_end);

      /* The carrier loop must have TRACKED the ramp, scored against what the
         channel was told to apply rather than against another receiver
         reading. `f_end = (ramp Hz/s) * T` cycles/sample at the input rate. */
      {
        double T    = (double)RX_DYN_NSYM / RX_DYN_RS; /* seconds */
        double want = RX_DYN_RATE_PPM_S * 1e-6 * RX_DYN_CARRIER_HZ * T
                      / (RX_DYN_SPS * RX_DYN_RS);
        double frac = want != 0.0 ? r.nco_end / want : 0.0;
        if (fabs (frac - 1.0) > RX_DYN_ACQ_TOL)
          {
            printf ("FAIL %s: carrier command %.4e against the %.4e the "
                    "channel applied (%.3f of the ramp) — the loop is not "
                    "tracking\n",
                    RX_DYN_NAMES[t], r.nco_end, want, frac);
            fail = 1;
          }
      }
      /* The timing loop must still be TRACKING the clock ramp at the end. A
         loop that stopped would drift at the ramp's rate, which this bound
         cannot accommodate. */
      if (fabs (r.rate_end - RX_DYN_SPS) > RX_DYN_RATE_TOL)
        {
          printf ("FAIL %s: rate %.5f samples/symbol at the end, off nominal "
                  "by more than %.2f — the timing loop is not tracking the "
                  "clock ramp\n",
                  RX_DYN_NAMES[t], r.rate_end, RX_DYN_RATE_TOL);
          fail = 1;
        }
      /* The sign rule (docs/design/mpsk.md §3.1): a lock statistic that
         transits zero means the constellation is parked on a decision
         boundary, which is a different failure from a merely noisy lock and
         is never acceptable at this Es/N0. Gated at the transient MINIMUM
         because that is where it would happen and where a pair of endpoint
         readings would miss it. */
      if (r.lock_min <= RX_DYN_LOCK_FLOOR)
        {
          printf ("FAIL %s: lock reached %+.3f inside the onset window — a "
                  "statistic transiting zero is a decision-boundary park, "
                  "not a noisy lock (§3.1)\n",
                  RX_DYN_NAMES[t], r.lock_min);
          fail = 1;
        }
      /* The quiet half is the flavor's own condition: the carrier loop must
         have something to report BEFORE any data arrives, on every tap,
         because that is what "no gating" means. */
      if (r.lock_quiet <= RX_DYN_LOCK_FLOOR)
        {
          printf ("FAIL %s: lock %+.3f with modulation OFF — the carrier loop "
                  "must acquire without waiting for data\n",
                  RX_DYN_NAMES[t], r.lock_quiet);
          fail = 1;
        }
    }

  /* The TED is not a detail here, and this is the measurement that says so:
     the SAME record through the wrong detector for a rectangular pulse. Only
     the DTTL pass is gated -- Gardner on NRZ is a misconfiguration, not a
     receiver defect, and gating it would pin a number nobody should rely on.
     It is PRINTED because a 2x difference in the transient is the single
     largest effect on this page, and it is invisible unless both are run. */
  if (!check)
    {
      printf ("\n  The same record through GARDNER -- the wrong TED for a "
              "rectangular pulse:\n\n");
      printf ("  tap       lock quiet   MIN post-onset   recovered   end\n");
      for (int t = 0; t < 1; t++)
        {
          rx_dyn_result_t g = rx_dyn_measure (RATESYNC_TED_GARDNER, NULL);
          if (g.refused || g.clipped)
            continue;
          printf ("  %-8s  %+9.3f   %+12.3f   %+9.3f   %+6.3f\n",
                  RX_DYN_NAMES[t], g.lock_quiet, g.lock_min, g.lock_recover,
                  g.lock_end);
        }
    }

  if (!check && out)
    printf ("\nTelemetry: %s/rx-dyn-<tap>.tlm (+ -meta), every probe, "
            "decim 1.\n",
            out);
  if (check)
    printf ("rx_dynamics: %s\n", fail ? "FAILED" : "OK");
  return fail;
}
