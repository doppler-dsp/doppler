

# File resamp\_core.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**resamp**](dir_289a9297ce406b952fab973539197d1c.md) **>** [**resamp\_core.h**](resamp__core_8h.md)

[Go to the documentation of this file](resamp__core_8h.md)


```C++

#ifndef RESAMP_CORE_H
#define RESAMP_CORE_H

#include "clib_common.h"
#include "dp_state.h"

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

    /* execute_ctrl state: the INTERPOLATOR's accumulator.  ctrl_phase is
       the position within the current input interval -- a uint32 fraction
       of one interval, which is exactly what a polyphase arm indexes.
       ctrl_debt is the whole intervals still owed before the next output
       may fire; it is non-zero only when the steered rate drops below
       unity, where one output spans more than one input. */
    uint32_t ctrl_phase;
    /* Inputs the accumulator has REQUESTED and not yet been given.  No
       sample enters the delay line without one of these: the push form
       holds the offered sample until a tick asks for it, exactly as the
       block form only ever loads inside its load branch. */
    uint32_t ctrl_debt;
    /* Inputs given WITHOUT a request, because max_out ended the call before
       a tick could ask.  The caller's sample has to go somewhere -- the API
       has no way to decline it -- so it is loaded and remembered here, and
       the next request is satisfied from it instead of from a new sample. */
    uint32_t ctrl_ahead;
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
   * VERSION 2: the ctrl accumulator is a uint32 phase word plus a uint32
   * load debt, not a float64 in [0, 1) -- a different width and encoding,
   * so a v1 blob is rejected by the envelope rather than misread. */
  /* Floor on the composite rate `rate + ctrl`.  Not a policy about what the
   * bank filters well -- it is what keeps the reciprocal in
   * resamp_execute_ctrl_push() defined.  Small enough that no real steer
   * reaches it. */
#define RESAMP_CTRL_RATE_MIN 1e-6

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
                              const double *ctrl, size_t num_in,
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

  double resamp_dc_gain (const resamp_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* RESAMP_CORE_H */
```


