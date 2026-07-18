/**
 * @file carrier_mpsk_core.h
 * @brief M-PSK carrier-tracking loop (integer-NCO de-rotation + decision PLL).
 *
 * The M-ary generalization of the Costas loop (costas_core.h): per sample it
 * de-rotates the input with the integer-phase `lo` NCO (carrier wipe-off); every
 * `tsamps` samples it dumps the coherent integrate-and-dump prompt, runs a
 * **decision-directed M-PSK** phase discriminator, filters the error through an
 * embedded 2nd-order @ref loop_filter_state_t, and steers the NCO frequency +
 * phase. It tracks a small *residual* carrier (bulk Doppler is removed upstream
 * by acquisition); the steering NCO is `lo`, so the phase is bounded and exactly
 * reproducible.
 *
 * The discriminator slices the prompt to the nearest constellation point
 * `ahat = mpsk_slice(P, m)` and uses `e = Im(P * conj(ahat)) / |P|`
 * (= sin of the phase error near lock). At @p m = 2 this reduces *exactly* to
 * the BPSK Costas discriminator. An optional decision-directed cross-product
 * **FLL assist** (`bn_fll > 0`) widens the frequency pull-in.
 *
 * The loop locks to one of @p m phases — an **M-fold ambiguity** on absolute
 * phase. Resolve it downstream with differential demapping (mpsk_diff_demap) or
 * a sync word; this loop only recovers the carrier and emits the prompts.
 *
 * The block API (carrier_mpsk_steps) is the Python face; the JM_FORCEINLINE
 * carrier_mpsk_wipeoff()/carrier_mpsk_update() are the C composition API a
 * receiver inlines into its own sample loop.
 *
 * @code
 * // QPSK carrier loop, 64 samples/symbol, FLL-assisted
 * carrier_mpsk_state_t *c = carrier_mpsk_create(0.05, 0.707, 0.0, 64, 0.01, 4);
 * float complex sym[16];
 * size_t k = carrier_mpsk_steps(c, rx, rx_len, sym, 16);
 * double f = c->nco.norm_freq;                 // tracked residual carrier
 * carrier_mpsk_destroy(c);
 * @endcode
 */
#ifndef CARRIER_MPSK_CORE_H
#define CARRIER_MPSK_CORE_H

#include "clib_common.h"
#include "dp_state.h"
#include "jm_perf.h"
#include "lo/lo_core.h"
#include "loop_filter/loop_filter_core.h"
#include "mpsk/mpsk_core.h"
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Numerical guard on the prompt magnitude in the discriminator (not tunable). */
#define CARRIER_MPSK_EPS 1e-12f
/* EMA smoothing for the decision-aligned lock metric (status diagnostic). */
#define CARRIER_MPSK_LOCK_ALPHA 0.1

/**
 * @brief M-PSK carrier loop state.
 *
 * Allocate with carrier_mpsk_create(), or embed by value and
 * carrier_mpsk_init(). The carrier NCO (`nco`) and PI loop (`lf`) are public
 * sub-components so the inline composition helpers can drive them; treat the
 * integrate-and-dump and diagnostic fields as internal.
 */
typedef struct {
    lo_state_t nco;          /**< integer carrier NCO (uint32 phase).      */
    loop_filter_state_t lf;  /**< 2nd-order carrier PI loop (PLL).         */
    size_t tsamps;           /**< samples per symbol (integrate-and-dump). */
    double seed_norm_freq;   /**< create-time carrier freq, for reset.     */
    double bn;               /**< PLL loop noise bandwidth (retained).     */
    double zeta;             /**< damping factor (retained).               */
    double bn_fll;           /**< FLL-assist bandwidth (0 = pure PLL).     */
    double k_fll;            /**< derived FLL gain (per-symbol freq pull).  */
    int m;                   /**< constellation order M (2, 4, 8).         */
    float complex acc;       /**< running coherent I&D accumulator.        */
    size_t acc_n;            /**< samples accumulated into `acc`.          */
    float complex prev;      /**< previous *data-wiped* prompt (FLL cross). */
    double prev_abs;         /**< |previous prompt| (FLL normalization).   */
    int have_prev;           /**< prev valid (skip FLL on the 1st symbol). */
    double lock_metric;      /**< EMA of Re(P conj a)/|P| (1 = locked).    */
    double last_error;       /**< last PLL discriminator (loop stress).    */
} carrier_mpsk_state_t;

/**
 * @brief Initialise an M-PSK carrier loop in place (no allocation).
 *
 * Seeds the NCO at @p init_norm_freq and the loop integrator to the matching
 * per-symbol frequency so de-rotation is correct from the first sample.
 *
 * @param s               State to initialise.  Must be non-NULL.
 * @param bn              Loop noise bandwidth, normalised to the symbol rate.
 * @param zeta            Damping factor (0.707 = critically damped).
 * @param init_norm_freq  Seed carrier frequency, cycles/sample.
 * @param tsamps          Samples per symbol (the integrate-and-dump period).
 * @param bn_fll          FLL-assist bandwidth (0 = pure PLL).
 * @param m               Constellation order M (2, 4, 8).
 */
void carrier_mpsk_init(carrier_mpsk_state_t *s, double bn, double zeta,
                       double init_norm_freq, size_t tsamps, double bn_fll,
                       int m);

/**
 * @brief Per-sample carrier wipe-off: de-rotate @p x by the NCO, advance it.
 *
 * `x * conj(lo_step(nco))` — strips the (tracked) carrier ahead of the
 * matched-filter integrate-and-dump.  Inline, zero call overhead.
 *
 * @param s  Carrier loop state.  Must be non-NULL.
 * @param x  One input sample.
 * @return The de-rotated sample to feed the integrator.
 */
JM_FORCEINLINE JM_HOT float complex
carrier_mpsk_wipeoff(carrier_mpsk_state_t *s, float complex x)
{
    return x * conjf(lo_step(&s->nco));
}

/**
 * @brief Per-symbol carrier update: decision discriminator -> loop -> NCO.
 *
 * Slices the prompt @p P to the nearest M-PSK point `ahat`, forms the
 * decision-directed phase error `e = Im(P conj(ahat)) / |P|`, optionally runs a
 * decision-directed cross-product FLL on the data-wiped prompts, filters, and
 * steers the NCO frequency + a proportional phase nudge.  Updates the lock
 * metric (decision-aligned `Re(P conj(ahat))/|P|`) and last_error.  Inline.
 *
 * @param s  Carrier loop state.  Must be non-NULL.
 * @param P  The dumped integrate-and-dump prompt for this symbol.
 */
JM_FORCEINLINE JM_HOT void
carrier_mpsk_update(carrier_mpsk_state_t *s, float complex P)
{
    float complex ahat;
    mpsk_slice(P, s->m, &ahat);          /* nearest unit constellation point */
    float complex d = P * conjf(ahat);   /* data-wiped prompt (carrier only) */
    double aP = (double)cabsf(P) + CARRIER_MPSK_EPS;
    double e = (double)cimagf(d) / aP;   /* sin(phase error) near lock */
    s->last_error = e;
    /* FLL assist: a cross-product frequency discriminator on the data-wiped
     * prompts has a far wider linear range than the phase discriminator, so it
     * pulls the frequency integrator onto a large/moving residual the bare PLL
     * cannot. Wiping by the decision conj(ahat) removes the M-PSK data phase,
     * so a symbol change between symbols does not corrupt the cross product. */
    if (s->k_fll > 0.0 && s->have_prev)
    {
        /* Im(conj(prev) * d): the carrier rotation between the two prompts. */
        float cross = crealf(s->prev) * cimagf(d) - cimagf(s->prev) * crealf(d);
        double freq_err = (double)cross / (aP * s->prev_abs);
        s->lf.integ += s->k_fll * freq_err;
    }
    s->prev = d;
    s->prev_abs = aP;
    s->have_prev = 1;
    loop_filter_step(&s->lf, e);
    /* per-symbol freq estimate (rad/symbol) -> rad/sample -> cycles/sample */
    double car_w = s->lf.integ / (double)s->tsamps;
    lo_set_norm_freq(&s->nco, car_w / (2.0 * M_PI));
    /* proportional phase nudge: kp*e radians -> cycles -> uint32 phase
     * delta, via the one shared primitive (a bare truncating cast here
     * is UB on a negative value -- see nco_norm_to_inc()'s own doc). */
    s->nco.phase += nco_norm_to_inc ((s->lf.kp * e) / (2.0 * M_PI));
    /* lock metric: Re(P conj(ahat))/|P| EMA (1 = phase-locked, ~0 = no carrier) */
    double inst = (double)crealf(d) / aP;
    s->lock_metric += CARRIER_MPSK_LOCK_ALPHA * (inst - s->lock_metric);
}

/**
 * @brief Create an M-PSK carrier loop instance.
 *
 * @param bn              Loop noise bandwidth (default 0.05).
 * @param zeta            Damping factor (default 0.707).
 * @param init_norm_freq  Seed carrier frequency, cycles/sample (default 0.0).
 * @param tsamps          Samples per symbol (default 64).
 * @param bn_fll          FLL-assist bandwidth (default 0.0 = pure PLL).
 * @param m               Constellation order M, 2/4/8 (default 4 = QPSK).
 * @return Heap-allocated state, or NULL on allocation failure.
 * @note Caller must call carrier_mpsk_destroy() when done.
 */
carrier_mpsk_state_t *carrier_mpsk_create(double bn, double zeta, double init_norm_freq, size_t tsamps, double bn_fll, int m);

/**
 * @brief Destroy an M-PSK carrier loop instance and release all memory.
 * @param state  May be NULL.
 */
void carrier_mpsk_destroy(carrier_mpsk_state_t *state);

/**
 * @brief Re-seed the loop to its create-time frequency/phase; keep config.
 * @param state  Must be non-NULL.
 */
void carrier_mpsk_reset(carrier_mpsk_state_t *state);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * Pointer-free POD struct, so a whole-struct snapshot resumes the loop exactly.
 */
#define CARRIER_MPSK_STATE_MAGIC DP_FOURCC('C', 'M', 'P', 'K')
#define CARRIER_MPSK_STATE_VERSION 1u

/** @brief Serialized-state byte size. */
size_t carrier_mpsk_state_bytes(const carrier_mpsk_state_t *state);
/** @brief Serialize the full loop state into @p blob. */
void carrier_mpsk_get_state(const carrier_mpsk_state_t *state, void *blob);
/** @brief Restore state; DP_OK, or DP_ERR_INVALID if the envelope rejects. */
int carrier_mpsk_set_state(carrier_mpsk_state_t *state, const void *blob);

size_t carrier_mpsk_steps_max_out(carrier_mpsk_state_t *state);
size_t carrier_mpsk_steps(carrier_mpsk_state_t *state, const float complex *x, size_t x_len, float complex *out, size_t max_out);
void carrier_mpsk_configure(carrier_mpsk_state_t *state, double bn, double zeta);
double carrier_mpsk_get_bn(const carrier_mpsk_state_t *state);
void carrier_mpsk_set_bn(carrier_mpsk_state_t *state, double val);
double carrier_mpsk_get_norm_freq(const carrier_mpsk_state_t *state);
void carrier_mpsk_set_norm_freq(carrier_mpsk_state_t *state, double val);
double carrier_mpsk_get_lock_metric(const carrier_mpsk_state_t *state);
double carrier_mpsk_get_last_error(const carrier_mpsk_state_t *state);
double carrier_mpsk_get_bn_fll(const carrier_mpsk_state_t *state);
void carrier_mpsk_set_bn_fll(carrier_mpsk_state_t *state, double val);
int carrier_mpsk_get_m(const carrier_mpsk_state_t *state);
#ifdef __cplusplus
}
#endif

#endif /* CARRIER_MPSK_CORE_H */
