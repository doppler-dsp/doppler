/**
 * @file ccsds_415.h
 * @brief The CCSDS 415.1-B-1 specification point the async DSSS receiver
 *        is specified for, stated once: its numbers, the derived rates a
 *        scene at it needs, and (through the receiver's and the pool's
 *        `Ccsds415` views) the receiver built there.
 *
 * The point's numbers are rows of docs/design/async-dsss-receiver.md §1's
 * table, which names each source, and the frame's are §1.4's; none of them
 * is chosen. The receiver's other arguments, in dp_ccsds_415_rx(), are
 * the receiver binding's defaults. The receiver's runs at
 * this point are docs/design/async-dsss-receiver-measurements.md; its
 * certification record holds at another point, 3.069 Mcps, and not at this
 * one (async-dsss-receiver.md §1, The specification, whose paragraph on
 * what the certification covers says so). A test, a
 * validator, a bench or an example that runs at the point reads these
 * names rather than restating them, so two programs cannot measure two
 * points under one name.
 *
 *   code         Gold, 1,023 chips: the mission's, so it is an argument
 *   chip rate    F_fwd x 31 / (221 x 96)                  table 4-1
 *   carrier      F_fwd 2.2 to 2.8 GHz                     owner
 *   symbols      3,000 sym/s, asynchronous to the code    owner
 *   capture      11 MSa/s, complex                        owner
 *   Doppler      within +/-1,500 Hz                       owner
 *   its rate     R_env, 11 Hz/s                           §1.8
 *   floor        Es/N0 4 dB                               owner
 *   frame        1,000 symbols, the first 275 code alone  owner, §1.4
 *
 * The functions (native/src/ccsds_415/, a hand-owned c_dep as ccsds_tm
 * is, so a receiver links the point and not the TM coding chain) are
 * computed with fast math off (the precedent is
 * dp_wfm_dsss_cont_edge(), wfm_dsp.c): under -ffast-math a division is
 * rewritten differently by GCC and by Clang, and a rate one ulp apart
 * moves a bit error between two programs that should agree. The chip
 * rate and the scene are IEEE arithmetic alone, so they are the same bits
 * on every compiler: test_ccsds_415.c pins them to literals, and the
 * Python chip rate is checked bit for bit. C/N0 goes through the
 * platform's log10, so it is held to 1e-12, not bit for bit.
 *
 * A C caller uses the names declared here, `dp_ccsds_415_*`. The
 * `ccsds` module's dp_ccsds415_chip_rate() and dp_ccsds415_cn0()
 * (ccsds_core.h) are the same two functions under the Python module's
 * names, thin aliases that call these: one function each, not two.
 *
 * @code
 * #include <doppler/ccsds/ccsds_415.h>
 * #include <stdio.h>
 *
 * dp_ccsds_415_scene_t s = dp_ccsds_415_scene (DP_CCSDS_415_FC_LO, 2.0, -13.75);
 * printf ("%.6f chips/s, ratio %.6f\n", s.chip_rate, s.ratio);
 * // 3214555.052790 chips/s, ratio 1.710968
 * @endcode
 */
#ifndef DP_CCSDS_415_H
#define DP_CCSDS_415_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** Chips a code period. */
#define DP_CCSDS_415_SF 1023u
/** The capture rate, complex samples a second. */
#define DP_CCSDS_415_FS 11e6
/** Data symbols a second. */
#define DP_CCSDS_415_SYM 3000.0
/** The band's low end, F_fwd, Hz. */
#define DP_CCSDS_415_FC_LO 2.2e9
/** The band's high end, F_fwd, Hz. */
#define DP_CCSDS_415_FC_HI 2.8e9
/** The Doppler uncertainty about best lock, one side, Hz. */
#define DP_CCSDS_415_SPAN_HZ 1500.0
/** R_env, the residual Doppler's rate the receiver meets, Hz/s
 *  (async-dsss-receiver.md §1.8: the pass model's timing envelope times
 *  Earth's rotation factor, rounded up). The pass model is the test
 *  fixture's (dp_s415_r_env(), native/tests/dp_s415_test.h), and a test
 *  holds the two equal, so this is derived, never fitted. */
#define DP_CCSDS_415_R_ENV_HZ_S 11.0
/** The sensitivity floor, Es/N0 dB. */
#define DP_CCSDS_415_FLOOR_ESN0_DB 4.0
/** Symbols a frame. */
#define DP_CCSDS_415_FRAME_SYM 1000u
/** Of a frame's symbols, the code-only ones that open it. */
#define DP_CCSDS_415_CODE_ONLY_SYM 275u
/** The lock allocation from arrival, s: 415.1-B-1 §7.4.1's acquisition
 *  time, and the owner's "typically allocated 20 seconds to lock". */
#define DP_CCSDS_415_LOCK_S 20.0

/**
 * @brief Chips a second at carrier @p fc: F_fwd x 31 / (221 x 96)
 *        (415.1-B-1 table 4-1). Fast math off: the same bits everywhere.
 *        Python: doppler.ccsds.ccsds415_chip_rate().
 *
 * @param fc  The carrier, F_fwd, Hz.
 * @return The chip rate, chips/s.
 *
 * @code
 * printf ("%.6f\n", dp_ccsds_415_chip_rate (2.8e9));   // 4091251.885370
 * @endcode
 */
double dp_ccsds_415_chip_rate (double fc);

/**
 * @brief C/N0, dB-Hz, of an Es/N0 at the point's symbol rate.
 *        Python: doppler.ccsds.ccsds415_cn0().
 *
 * @param esn0_db  Es/N0, dB.
 * @return `esn0_db + 10 log10(DP_CCSDS_415_SYM)`, dB-Hz.
 *
 * @code
 * printf ("%.4f\n", dp_ccsds_415_cn0 (DP_CCSDS_415_FLOOR_ESN0_DB)); // 38.7712
 * @endcode
 */
double dp_ccsds_415_cn0 (double esn0_db);

/**
 * @brief The numbers a scene at the point is built from, at one carrier:
 *        what dp_ccsds_415_scene() returns.
 */
typedef struct
{
  /** Chips a second, dp_ccsds_415_chip_rate(). */
  double chip_rate;
  /** The rate converter's ratio from an emitter at `spc` samples a chip to
   *  the capture: DP_CCSDS_415_FS / (spc x chip_rate). */
  double ratio;
  /** The Doppler at the first sample in ppm of the carrier, the unit
   *  dp_doppler_channel_create() takes: start_hz / fc x 1e6. */
  double start_ppm;
  /** R_env in ppm of the carrier a second: DP_CCSDS_415_R_ENV_HZ_S / fc x
   *  1e6. */
  double rate_ppm_s;
} dp_ccsds_415_scene_t;

/**
 * @brief The scene's derived numbers at carrier @p fc, computed once so a
 *        C program and its Python twin read the same bits.
 *
 * @p spc and @p start_hz are the scene's choices, not the point's: the
 * emitter's samples a chip and the Doppler it starts at. Fast math is off,
 * so the result does not depend on the compiler. Its Python face is
 * `doppler.ccsds.ccsds415_scene()`, which returns a `Ccsds415Scene` record
 * (dp_ccsds415_scene() in `doppler/ccsds/ccsds_core.h`).
 *
 * @param fc        The carrier, Hz.
 * @param spc       The emitter's samples a chip, > 0.
 * @param start_hz  The Doppler at the scene's first sample, Hz.
 * @return The scene's numbers.
 *
 * @code
 * dp_ccsds_415_scene_t s = dp_ccsds_415_scene (2.2e9, 2.0, -13.75);
 * printf ("%.17g %.17g\n", s.start_ppm, s.rate_ppm_s);
 * // -0.0062499999999999995 0.0050000000000000001
 * @endcode
 */
dp_ccsds_415_scene_t dp_ccsds_415_scene (double fc, double spc,
                                       double start_hz);

/** The lowest carrier the point builds a receiver at, Hz: where a symbol
 *  (1 / DP_CCSDS_415_SYM s) is one code period (DP_CCSDS_415_SF chips).
 *  Below it the chip rate is under SF x SYM, a symbol is shorter than a
 *  code period, and the receiver's carrier error, read over a code period
 *  free of symbol transitions, has no such period. About 2.1004 GHz. */
#define DP_CCSDS_415_RX_FC_MIN                                               \
  ((double)DP_CCSDS_415_SF * DP_CCSDS_415_SYM * (221.0 * 96.0) / 31.0)
/** The highest carrier the point builds a receiver at, Hz: where the chip
 *  rate reaches the capture rate, DP_CCSDS_415_FS. Above it a chip is
 *  shorter than a sample. About 7.528 GHz. */
#define DP_CCSDS_415_RX_FC_MAX (DP_CCSDS_415_FS * (221.0 * 96.0) / 31.0)

/**
 * @brief The async DSSS receiver's create arguments at the point, every
 *        one but the code: what dp_ccsds_415_rx() fills.
 *
 * One field per parameter of dp_async_dsss_receiver_create(), in its order
 * and under its name; that function's documentation says what each does.
 * The receiver view (dp_async_dsss_receiver_create_ccsds415()), the pool
 * view (dp_async_dsss_pool_create_ccsds415()) and the receiver's test all
 * read this one record, so the point's arguments are written once.
 */
typedef struct
{
  double chip_rate;   /**< dp_ccsds_415_chip_rate() of the carrier. */
  double symbol_rate; /**< DP_CCSDS_415_SYM. */
  double sample_rate; /**< DP_CCSDS_415_FS. */
  int    m;           /**< 2: BPSK. */
  double cn0_dbhz;    /**< dp_ccsds_415_cn0() of the Es/N0 floor. */
  double pfa;         /**< 1e-3. */
  double pd;          /**< 0.9. */
  double doppler_uncertainty;       /**< DP_CCSDS_415_SPAN_HZ. */
  size_t segments;                  /**< 4. */
  size_t sps;                       /**< 8. */
  int    differential;              /**< 0: coherent demap. */
  double refine_max_error_db;       /**< 0.5. */
  size_t refine_samples_per_symbol; /**< 4. */
  double refine_design_margin_db;   /**< 14.0. */
  size_t refine_n_fft;              /**< 64. */
  size_t refine_zero_pad;           /**< 8. */
  bool   refine_sequential;         /**< false. */
  size_t refine_max_n_blocks;       /**< 100000. */
  double carrier_freq_hz;           /**< The caller's carrier. */
  double lost_confirm_s;            /**< The caller's. */
  bool   reacquire;                 /**< false. */
  double code_only_s; /**< DP_CCSDS_415_CODE_ONLY_SYM / DP_CCSDS_415_SYM. */
  double doppler_rate; /**< DP_CCSDS_415_R_ENV_HZ_S. */
  bool   refine;       /**< The caller's. */
} dp_ccsds_415_rx_t;

/**
 * @brief Fills @p a with the receiver's create arguments at the point, or
 *        refuses with the reason.
 *
 * The caller's choices are the code's length, the carrier, the lost
 * confirm time and the refine stage; everything else is the point's
 * (dp_ccsds_415_rx_t says which). Refused, each with its own sentence in
 * @p why: a code that is not DP_CCSDS_415_SF chips; a carrier that is not
 * finite, below DP_CCSDS_415_RX_FC_MIN or above DP_CCSDS_415_RX_FC_MAX;
 * a negative or NaN @p lost_confirm_s.
 *
 * @param a                Filled on success; untouched on a refusal.
 * @param code_len         The code's chips; the point's is 1,023.
 * @param carrier_freq_hz  The carrier, F_fwd, Hz.
 * @param lost_confirm_s   Both flags down for longer than this enters
 *                         lost; 0 never does.
 * @param refine           The refine stage on or off.
 * @param why              Set to the reason on a refusal; may be NULL.
 * @return 0, or -1 with @p why set.
 *
 * @code
 * dp_ccsds_415_rx_t a;
 * const char       *why = NULL;
 * if (dp_ccsds_415_rx (&a, 1023, 2.0e9, 0.0, true, &why) != 0)
 *   printf ("%s\n", why);
 * // carrier_freq_hz is below DP_CCSDS_415_RX_FC_MIN (about 2.1004 GHz):
 * // its chip rate is under 1,023 x 3,000, so a symbol is shorter than a
 * // code period
 * @endcode
 */
int dp_ccsds_415_rx (dp_ccsds_415_rx_t *a, size_t code_len,
                     double carrier_freq_hz, double lost_confirm_s,
                     bool refine, const char **why);

#ifdef __cplusplus
}
#endif

#endif /* DP_CCSDS_415_H */
