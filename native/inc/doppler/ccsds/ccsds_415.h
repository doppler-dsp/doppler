/**
 * @file ccsds_415.h
 * @brief The CCSDS 415.1-B-1 specification point the async DSSS receiver
 *        is specified for, stated once: its numbers, the derived rates a
 *        scene at it needs, and (through the receiver's and the pool's
 *        `Ccsds415` views) the receiver built there.
 *
 * Every number is a row of docs/design/async-dsss-receiver.md §1's table,
 * which names its source; nothing here is chosen. The receiver's runs at
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
 * moves a bit error between two programs that should agree. So every
 * compiler, and the Python face, reads the same bits.
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
 * so the result does not depend on the compiler. Its Python face waits on
 * just-makeit#2755 (a module function returning one record).
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

#ifdef __cplusplus
}
#endif

#endif /* DP_CCSDS_415_H */
