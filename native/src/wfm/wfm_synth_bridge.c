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
#include "wfm/wfm_frame.h"   /* the frame descriptor both faces now read */
#include "wfm_synth/wfm_synth_core.h"

/* Pulse enum index 1 == "rrc" (see the wfm_pulse [[enum]] SSOT). */
#define WFM_PULSE_RRC 1

int
wfm_source_has_frame (const wfm_source_t *src)
{
  /* Preamble or sync word — never `crc`; see the header on why. */
  return src
         && ((src->acq_code && src->n_acq_code && src->acq_reps)
             || (src->sync && src->n_sync));
}

const char *
wfm_source_frame_error (const wfm_source_t *src)
{
  if (!wfm_source_has_frame (src))
    return NULL;
  if (src->type == WFM_SYNTH_DSSS)
    return NULL; /* the spread path, unchanged */
  if (src->type != WFM_SYNTH_BITS)
    return "--acq-code/--sync frame a waveform, and a frame needs an explicit "
           "payload: use --type bits with --bits (--modulation bpsk|qpsk), or "
           "--type dsss to spread it";
  if (!src->bits || src->n_bits == 0)
    return "a frame needs a payload: --type bits with --acq-code/--sync also "
           "needs --bits";
  return NULL;
}

int
wfm_source_attach_frame (wfm_synth_state_t *syn, const wfm_source_t *src)
{
  if (src->type != WFM_SYNTH_BITS || !src->bits || !src->n_bits)
    return 0; /* nothing to attach; mirrors wfm_synth_set_bits */
  if (!wfm_source_has_frame (src))
    return wfm_synth_set_bits (syn, src->bits, src->n_bits, src->modulation);

  /* Framed: the pattern is the whole frame, assembled by the one descriptor.
     Every field is LITERAL here because that is all a source can carry today —
     the generated PN/Gold kinds `wfm_seq_t` supports have no spelling on any
     face yet (gh-755). */
  wfm_frame_t f   = { 0 };
  f.preamble.kind = WFM_SEQ_LITERAL;
  f.preamble.bits = src->acq_code;
  f.preamble.len  = src->n_acq_code;
  f.preamble_reps = src->acq_reps;
  f.sync.kind     = WFM_SEQ_LITERAL;
  f.sync.bits     = src->sync;
  f.sync.len      = src->n_sync;
  f.payload.kind  = WFM_SEQ_LITERAL;
  f.payload.bits  = src->bits;
  f.payload.len   = src->n_bits;
  f.crc           = src->crc;

  size_t n = wfm_frame_nbits (&f);
  if (n == 0)
    return -1;
  uint8_t *bits = (uint8_t *)malloc (n);
  if (!bits)
    return -1;
  if (wfm_frame_bits (&f, bits, n) != n)
    {
      free (bits);
      return -1;
    }
  /* set_bits copies, so the frame buffer is ours to release. */
  int rc = wfm_synth_set_bits (syn, bits, n, src->modulation);
  free (bits);
  return rc;
}

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
      int mode = src->dsss_code_only          ? WFM_DSSS_DATA_NONE
                 : (src->bits && src->n_bits) ? WFM_DSSS_DATA_BITS
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
  /* A frame this waveform type cannot carry. Refusing is the whole point:
     these fields used to be accepted and dropped, so the caller got an
     unframed waveform and no way to find out. */
  if (wfm_source_frame_error (src) != NULL)
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

  if (wfm_source_attach_frame (eng, src) != 0)
    {
      wfm_synth_destroy (eng);
      return NULL;
    }

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
