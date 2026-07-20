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

int
wfm_source_attach_dsss (wfm_synth_state_t *syn, const wfm_source_t *src,
                        double fs)
{
  if (src->type != WFM_SYNTH_DSSS)
    return 0; /* no-op, mirrors wfm_synth_set_dsss */
  if (src->symbol_rate > 0.0)
    {
      /* Continuous async: the data clock is independent of the code. sps is
         samples-per-CHIP for dsss, so chip_rate = fs/sps and chips/symbol =
         chip_rate/symbol_rate (non-integer — the asynchronicity). Data comes
         from the payload when supplied, else the seeded PN a receiver can
         regenerate. (Code-only, --data none, arrives with the CLI flag.) */
      double cps
          = (src->sps > 0) ? (fs / (double)src->sps) / src->symbol_rate : 0.0;
      int mode = (src->bits && src->n_bits) ? WFM_DSSS_DATA_BITS
                                            : WFM_DSSS_DATA_PRBS;
      return wfm_synth_set_dsss_cont (syn, src->data_code, src->n_data_code,
                                      cps, mode, src->bits, src->n_bits);
    }
  return wfm_synth_set_dsss (syn, src->acq_code, src->n_acq_code,
                             src->acq_reps, src->data_code, src->n_data_code,
                             src->sync, src->n_sync, src->bits, src->n_bits,
                             src->crc);
}

wfm_synth_state_t *
wfm_source_to_synth (const wfm_source_t *src, double fs)
{
  /* A "bits" waveform with no pattern has nothing to transmit. Reject it here
     so the generated Synth_ensure_gen turns this NULL into an error at first
     generation (the old Synth.__init__ raised eagerly; standalone generation
     is lazy, so the guard moves to first steps()/step()). */
  if (src->type == WFM_SYNTH_BITS && (!src->bits || !src->n_bits))
    return NULL;
  /* Likewise a "symbols" waveform needs a constellation stream. */
  if (src->type == WFM_SYNTH_SYMBOLS && (!src->symbols || !src->n_symbols))
    return NULL;
  /* A "dsss" BURST needs valid frame geometry (a preamble and/or a data-coded
     frame; frame bits require a data code). A CONTINUOUS stream (symbol_rate >
     0) has no frame — it needs only a spreading code. */
  if (src->type == WFM_SYNTH_DSSS && src->symbol_rate <= 0.0
      && wfm_frame_dsss_nchips (src->n_acq_code, src->acq_reps,
                                src->n_data_code, src->n_sync, src->n_bits,
                                src->crc)
             == 0)
    return NULL;
  if (src->type == WFM_SYNTH_DSSS && src->symbol_rate > 0.0
      && (!src->data_code || src->n_data_code == 0))
    return NULL;

  /* Refer a dsss data-symbol Es/N0 to fs before create (the SSOT helper the
     composer also uses, so both faces agree to the bit). */
  int    snr_mode = 0;
  double snr_c    = wfm_source_create_snr (src, fs, src->snr, &snr_mode);
  wfm_synth_state_t *eng = wfm_synth_create (
      src->type, fs, src->freq, snr_c, snr_mode, src->seed, src->sps,
      src->pn_length, src->pn_poly, src->lfsr, src->f_end);
  if (!eng)
    return NULL;

  if (src->type == WFM_SYNTH_BITS && src->bits && src->n_bits)
    wfm_synth_set_bits (eng, src->bits, src->n_bits, src->modulation);

  if (src->type == WFM_SYNTH_SYMBOLS && src->symbols && src->n_symbols)
    wfm_synth_set_symbols (eng, src->symbols, src->n_symbols);

  if (wfm_source_attach_dsss (eng, src, fs) != 0)
    {
      wfm_synth_destroy (eng);
      return NULL;
    }

  if (src->pulse == WFM_PULSE_RRC
      && (src->type == WFM_SYNTH_PN || src->type == WFM_SYNTH_BPSK
          || src->type == WFM_SYNTH_QPSK || src->type == WFM_SYNTH_BITS
          || src->type == WFM_SYNTH_SYMBOLS || src->type == WFM_SYNTH_DSSS))
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
