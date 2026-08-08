

# File resamp\_core.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**resamp**](dir_289a9297ce406b952fab973539197d1c.md) **>** [**resamp\_core.h**](resamp__core_8h.md)

[Go to the documentation of this file](resamp__core_8h.md)


```C++

#ifndef RESAMP_CORE_H
#define RESAMP_CORE_H

#include "clib_common.h"
#include "dp_state.h"
#include "nco/nco_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct
  {
    double rate;
    size_t num_phases;
    size_t num_taps;
    unsigned log2_phases;
    int upsample; /* 1 = rate >= 1.0, 0 = rate < 1.0 */

    float *bank; /* num_phases × num_taps, row-major  */

    /* execute state */
    uint32_t phase;
    uint32_t phase_inc;

    /* interpolator / execute_ctrl: dual-buffer delay line */
    float _Complex *delay_buf; /* 2 × delay_cap elements         */
    size_t delay_cap;
    size_t delay_mask;
    size_t delay_head;

    /* decimator transposed-form state (execute, rate < 1) */
    float _Complex *decim_iad; /* integrate-and-dump: num_taps   */
    float _Complex *decim_tfd; /* transposed delay line: num_taps-1 */

    /* execute_ctrl state: its own phase accumulator (nco_core.h),
       embedded by value. A uint32 phase word cannot leave [0, 1), so the
       polyphase arm it selects is in range by construction -- see
       resamp_execute_ctrl_push(). */
    nco_state_t ctrl_nco;
  } resamp_state_t;

  /* ------------------------------------------------------------------
   * Lifecycle
   * ------------------------------------------------------------------ */

  resamp_state_t *resamp_create (double rate);

  resamp_state_t *resamp_create_custom (size_t num_phases, size_t num_taps,
                                        const float *bank, double rate);

  void resamp_destroy (resamp_state_t *state);

  void resamp_reset (resamp_state_t *state);

  /* Serializable state (standard bytes interface; see dp_state.h): after the
   * envelope, the polyphase phase, the fractional ctrl accumulator, the
   * delay-line write head, and the three delay buffers (delay_buf, decim_iad,
   * decim_tfd).  Rate, bank, phase increment and sizes are config.
   *
   * VERSION 2: the ctrl accumulator is a uint32 phase word, not a float64
   * in [0, 1) -- a different width and encoding, so a v1 blob is rejected
   * by the envelope rather than misread. */
#define RESAMP_STATE_MAGIC DP_FOURCC ('R', 'S', 'M', 'P')
#define RESAMP_STATE_VERSION 2u

  size_t resamp_state_bytes (const resamp_state_t *state);
  void resamp_get_state (const resamp_state_t *state, void *blob);
  int resamp_set_state (resamp_state_t *state, const void *blob);

  /* ------------------------------------------------------------------
   * Execute
   * ------------------------------------------------------------------ */

  size_t resamp_execute (resamp_state_t *state, const float _Complex *in,
                         size_t num_in, float _Complex *out, size_t max_out);

  size_t resamp_execute_ctrl (resamp_state_t *state, const float _Complex *in,
                              const float _Complex *ctrl, size_t num_in,
                              float _Complex *out, size_t max_out);

  /* ------------------------------------------------------------------
   * Streaming interpolation (fixed integer rate, output-count driven)
   * ------------------------------------------------------------------ */

  size_t resamp_interp_inputs_needed (const resamp_state_t *state,
                                      size_t max_out);

  size_t resamp_interp_fill (resamp_state_t *state, const float _Complex *in,
                             float _Complex *out, size_t max_out);

  /* ------------------------------------------------------------------
   * Streaming control port (closed-loop timing / arbitrary rate)
   * ------------------------------------------------------------------ */

  size_t resamp_execute_ctrl_push (resamp_state_t *state, float _Complex x,
                                   double ctrl, float _Complex *out,
                                   size_t max_out);

  /* ------------------------------------------------------------------
   * Properties
   * ------------------------------------------------------------------ */

  double resamp_get_rate (const resamp_state_t *state);

  void resamp_set_rate (resamp_state_t *state, double rate);

  size_t resamp_get_num_phases (const resamp_state_t *state);
  size_t resamp_get_num_taps (const resamp_state_t *state);

  double resamp_get_ctrl_acc (const resamp_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* RESAMP_CORE_H */
```


