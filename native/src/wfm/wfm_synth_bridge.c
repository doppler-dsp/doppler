/*
 * wfm_synth_bridge.c — straight-C bridge for the generated Synth's standalone
 * generation (jm composer `source.generates`, gh-287 round 3).
 *
 * jm generates the CPython `Synth.steps()/.step()/.reset()` plumbing in
 * wfm_compose_ext.c; this file is the *construction algorithm only* — build a
 * `wfm_synth` engine from a `wfm_source_t` config — with NO CPython in it. It
 * mirrors what the old Python `compose.py:Synth._engine()` did: create, then
 * attach the bit pattern (type=bits) and the RRC pulse taps (pn/bpsk/qpsk/bits
 * with pulse="rrc"). The unit-energy taps are scaled to unit transmit power
 * inside `wfm_synth_set_rrc`, so standalone generation stays byte-identical to
 * the composed path.
 */
#include <stdlib.h>

#include "wfm/wfm_compose.h" /* wfm_source_t */
#include "wfm/wfm_dsp.h"     /* wfm_rrc_ntaps / wfm_rrc_taps */
#include "wfm_synth/wfm_synth_core.h"

/* Pulse enum index 1 == "rrc" (see the wfm_pulse [[enum]] SSOT). */
#define WFM_PULSE_RRC 1

wfm_synth_state_t *
wfm_source_to_synth (const wfm_source_t *src, double fs)
{
  /* A "bits" waveform with no pattern has nothing to transmit. Reject it here
     so the generated Synth_ensure_gen turns this NULL into an error at first
     generation (the old Synth.__init__ raised eagerly; standalone generation
     is lazy, so the guard moves to first steps()/step()). */
  if (src->type == WFM_SYNTH_BITS && (!src->bits || !src->n_bits))
    return NULL;

  wfm_synth_state_t *eng = wfm_synth_create (
      src->type, fs, src->freq, src->snr, src->snr_mode, src->seed, src->sps,
      src->pn_length, src->pn_poly, src->lfsr, src->f_end);
  if (!eng)
    return NULL;

  if (src->type == WFM_SYNTH_BITS && src->bits && src->n_bits)
    wfm_synth_set_bits (eng, src->bits, src->n_bits, src->modulation);

  if (src->pulse == WFM_PULSE_RRC
      && (src->type == WFM_SYNTH_PN || src->type == WFM_SYNTH_BPSK
          || src->type == WFM_SYNTH_QPSK || src->type == WFM_SYNTH_BITS))
    {
      int    ntaps = wfm_rrc_ntaps (src->sps, src->rrc_span);
      float *taps  = (float *)malloc ((size_t)ntaps * sizeof *taps);
      if (taps)
        {
          wfm_rrc_taps (src->rrc_beta, src->sps, src->rrc_span, taps);
          wfm_synth_set_rrc (eng, taps, ntaps);
          free (taps); /* set_rrc copies the taps */
        }
    }
  return eng;
}
