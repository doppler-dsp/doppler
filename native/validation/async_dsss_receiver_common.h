/**
 * @file async_dsss_receiver_common.h
 * @brief The certification's operating point, receiver and stimulus, shared
 *        by the two harnesses that measure the receiver there.
 *
 * `async_dsss_receiver_lock.c` (arrival, dropout, EVM and false lock) and
 * `async_dsss_receiver_ber.c` (the error rate against the BPSK bound) measure
 * ONE receiver on ONE stimulus, so its geometry, its configuration and the
 * stimulus helpers live here once. A second copy would let the two drift
 * apart, and the error rate would then describe a receiver the rest of the
 * certification never ran.
 *
 * Operating point. A 1023-chip Gold code (CCSDS #365) at 3.069 Mcps, two
 * samples a chip, so one code epoch -- the feed block -- is 2,046 samples,
 * 333.3 us, and 1/T_E is 3 kHz; asynchronous BPSK data at 2,700 sym/s. The
 * receiver is the searching flavor at the binding's defaults (pfa 1e-3, the
 * 0.5 dB look-back, the refine's 64-point FFT), its own search over
 * +/-1/T_E and its design C/N0 the true one.
 *
 * Stimulus. The emitter is the shipped continuous-DSSS synth
 * (dp_dsss_cont_emitter) sending the harness's own bits, one per symbol, and
 * the noise the shipped awgn generator, sized by dp_awgn_amplitude_for_snr()
 * from the C/N0 for a unit-power emitter. Nothing here builds a chip, a bit
 * or a sigma by hand.
 */
#ifndef ASYNC_DSSS_RECEIVER_COMMON_H
#define ASYNC_DSSS_RECEIVER_COMMON_H

#include "doppler/async_dsss_receiver/async_dsss_receiver_core.h"
#include "doppler/awgn/awgn_core.h"
#include "doppler/clib_common.h"
#include "dp_rng_test.h"
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SF 1023u
#define SPC 2u
#define TE (SF * SPC) /* one epoch, the feed block: 2046 samples */
#define CHIP_RATE 3.069e6
#define FS (CHIP_RATE * (double)SPC)
#define SYM_RATE 2700.0
#define CPS (CHIP_RATE / SYM_RATE) /* chips a symbol, 1136.7 */
#define SPAN (FS / (double)TE)     /* 1/T_E: 3,000 Hz                */
#define EPOCH_MS (1e3 * (double)TE / FS)

/* C/N0 for an Es/N0 at this symbol rate. */
static inline double
cn0_of (double esn0_db)
{
  return esn0_db + 10.0 * log10 (SYM_RATE);
}

/* The receiver: the searching flavor at the binding's defaults
   (objects/async_dsss_receiver.toml), its own search over +/-1/T_E. */
static inline dp_async_dsss_receiver_state_t *
make_rx (const uint8_t *code, double cn0_dbhz)
{
  return dp_async_dsss_receiver_create (
      code, SF, CHIP_RATE, SYM_RATE, SPC, 2, cn0_dbhz, 1e-3, 0.9, SPAN, 4, 8,
      0, 0.5, 4, 14.0, 64, 8, false, 100000, 0.0, 0.0, false, 0.0, 0.0);
}

/* The noise: the shipped awgn, its amplitude the library's answer for a
   unit-power emitter at this C/N0 over fs. */
static inline dp_awgn_state_t *
make_noise (uint32_t seed, double cn0_dbhz)
{
  return dp_awgn_create (
      seed * 7919u + 1u,
      dp_awgn_amplitude_for_snr ((float)(cn0_dbhz - 10.0 * log10 (FS)), 1.0f));
}

/* A trial's own stream: adjacent seeds share their first xorshift draws,
   so the golden-ratio mix spreads them (dp_rng_test.h's note). */
static inline uint32_t
trial_rng (uint32_t seed)
{
  uint32_t st = seed * 0x9e3779b9u;
  for (int w = 0; w < 4; w++)
    (void)dp_xs32 (&st);
  return st;
}

/* The data, one 0/1 bit a symbol, from the trial's stream. */
static inline uint8_t *
draw_bits (uint32_t *st, size_t n)
{
  uint8_t *bits = dp_xmalloc (n);
  for (size_t i = 0; i < n; i++)
    bits[i] = dp_bit (st) < 0 ? 1u : 0u;
  return bits;
}

/* The symbol carrying sample `n` of the emitter's stream: its chip over
   the chips a symbol, the synth's own clock to within one symbol. */
static inline size_t
symbol_at (size_t n)
{
  return (size_t)((double)n / (double)SPC / CPS);
}

/* Whole blocks covering `samples`. */
static inline size_t
blocks_for (size_t samples)
{
  return (samples + TE - 1) / TE;
}

#endif /* ASYNC_DSSS_RECEIVER_COMMON_H */
