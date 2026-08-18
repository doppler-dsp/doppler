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

#include "ccsds_tm/ccsds_tm_frame.h" /* the kernels its coded stages run */
#include "wfm/wfm_compose.h"         /* wfm_source_t */
#include "wfm/wfm_dsp.h"             /* wfm_rrc_ntaps / wfm_rrc_taps */
#include "wfm/wfm_frame.h" /* the frame descriptor both faces now read */
#include "wfm_synth/wfm_synth_core.h"

/* Pulse enum index 1 == "rrc" (see the wfm_pulse [[enum]] SSOT). */
#define WFM_PULSE_RRC 1

int
wfm_source_has_frame (const wfm_source_t *src)
{
  /* Preamble or sync word — never `crc`; see the header on why. Any coding
     stage frames it too, and a CADU is why: [ASM | codeblock] carries neither
     a preamble nor a sync word, so a source coded but unframed would take the
     plain set_bits path and emit the payload with no coding at all. `crc`
     stays excluded for the reason it always was — it DEFAULTS to crc16, so it
     alone says nothing about the caller's intent, while every flag below is
     off unless asked for. */
  return src
         && ((src->acq_code && src->n_acq_code && src->acq_reps)
             || (src->sync && src->n_sync) || src->attach_asm || src->rs_depth
             || src->randomise || src->convolutional);
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

  /* 4.3.5.1's depths, checked here so a caller learns it from the flag rather
     than from a kernel refusing mid-assembly. */
  if (src->rs_depth != 0 && src->rs_depth != 1 && src->rs_depth != 2
      && src->rs_depth != 3 && src->rs_depth != 4 && src->rs_depth != 5
      && src->rs_depth != 8)
    return "--rs-depth must be 1, 2, 3, 4, 5 or 8 (CCSDS 131.0-B-3 4.3.5.1)";

  /* 4.4.1: a codeblock is a whole number of codewords, so the data the outer
     code is given — the payload plus any CRC trailer — must be exactly
     223*depth octets. Virtual fill is not implemented (gh-813), so anything
     else is refused rather than padded: padding produces a codeblock that
     encodes and decodes perfectly here and is the wrong length for the
     receiver it was aimed at. */
  if (src->rs_depth != 0)
    {
      const size_t data = src->n_bits + (src->crc ? WFM_FRAME_CRC_BITS : 0u);
      const size_t want = (size_t)CCSDS_TM_RS_K * src->rs_depth * 8u;
      if (data != want)
        return "--rs-depth needs the payload (plus its CRC, if any) to be "
               "exactly 223*depth octets — virtual fill is not implemented, "
               "so a short frame is refused rather than padded";
    }
  return NULL;
}

/* The source's frame as a DESCRIPTION: the fields in wire order, and the
 * span each stage covers.
 *
 * Built here rather than through wfm_frame_describe() because the marker goes
 * in FRONT of everything and a description's field list is positional -- it
 * cannot be appended after the fact. That is the same reason the marker is
 * interesting at all: it enters the frame third and one of the stages after
 * it reaches back over it.
 *
 * Which stage covers what is the whole content of this function:
 *
 *   marker / preamble / sync   found, not decoded -- covered by the inner
 *                              code alone, because all three have to look
 *                              the same in every frame to be findable
 *   payload / crc / parity     the data group -- what the outer code and the
 *                              randomiser reach over
 *   everything                 the inner code
 *
 * The middle row is 10.3.4's rule generalised: CCSDS says the randomiser does
 * not cover the ASM, and the reason it gives -- a marker a receiver
 * correlates against must not vary between frames -- is exactly as true of a
 * preamble and a sync word.
 */
static int
describe_source_frame (const wfm_source_t *src, wfm_frame_desc_t *d)
{
  memset (d, 0, sizeof *d);

  unsigned i_asm = 0, i_data = 0, i_crc = 0, i_parity = 0;
  unsigned n = 0;

  /* The ASM is a literal field like any other; its pattern comes from the one
     function that expands it, never a second transcription. */
  static uint8_t marker[CCSDS_TM_ASM_BITS];
  if (src->attach_asm)
    {
      ccsds_tm_asm_bits (marker);
      i_asm                = n;
      d->field[n].seq.kind = WFM_SEQ_LITERAL;
      d->field[n].seq.bits = marker;
      d->field[n].seq.len  = CCSDS_TM_ASM_BITS;
      n++;
    }
  if (src->n_acq_code && src->acq_reps)
    {
      d->field[n].seq.kind = WFM_SEQ_LITERAL;
      d->field[n].seq.bits = src->acq_code;
      d->field[n].seq.len  = src->n_acq_code;
      d->field[n].reps     = src->acq_reps;
      n++;
    }
  if (src->n_sync)
    {
      d->field[n].seq.kind = WFM_SEQ_LITERAL;
      d->field[n].seq.bits = src->sync;
      d->field[n].seq.len  = src->n_sync;
      n++;
    }

  i_data               = n;
  d->field[n].seq.kind = WFM_SEQ_LITERAL;
  d->field[n].seq.bits = src->bits;
  d->field[n].seq.len  = src->n_bits;
  n++;

  /* Stage indices are needed before the stages exist, because a derived field
     names the stage that writes it. They are assigned in application order:
     the CRC first, then the outer code over the result, then the randomiser,
     then the inner code. */
  unsigned s_crc = 0, s_rs = 0, s_rand = 0, s_conv = 0, ns = 0;
  if (src->crc)
    s_crc = ns++;
  if (src->rs_depth)
    s_rs = ns++;
  if (src->randomise)
    s_rand = ns++;
  if (src->convolutional)
    s_conv = ns++;

  if (src->crc)
    {
      i_crc                  = n;
      d->field[n].bits       = WFM_FRAME_CRC_BITS;
      d->field[n].derived_by = s_crc + 1u;
      n++;
    }
  if (src->rs_depth)
    {
      i_parity               = n;
      d->field[n].bits       = (size_t)CCSDS_TM_RS_2E * src->rs_depth * 8u;
      d->field[n].derived_by = s_rs + 1u;
      n++;
    }
  d->n_fields = n;
  d->n_stages = ns;

  /* The data group: payload, its CRC, and the outer code's check symbols.
     Contiguous by construction, and each derived field is the last of its own
     stage's cover, which is what lets one kernel signature serve them all. */
  const unsigned data_end = n; /* one past the last data field */

  if (src->crc)
    {
      d->stage[s_crc].kind        = WFM_STAGE_CRC16;
      d->stage[s_crc].first_field = i_data;
      d->stage[s_crc].n_fields    = i_crc - i_data + 1u;
    }
  if (src->rs_depth)
    {
      d->stage[s_rs].kind        = WFM_STAGE_RS;
      d->stage[s_rs].depth       = src->rs_depth;
      d->stage[s_rs].first_field = i_data;
      d->stage[s_rs].n_fields    = i_parity - i_data + 1u;
    }
  if (src->randomise)
    {
      d->stage[s_rand].kind        = WFM_STAGE_RANDOMISE;
      d->stage[s_rand].first_field = i_data;
      d->stage[s_rand].n_fields    = data_end - i_data;
      /* WHICH generator, carried on the stage: 131.0-B-6 specifies two and
         they produce waveforms only the matching receiver derandomises, so
         this is not a detail the kernel may pick for itself. `depth` is the
         stage's free parameter and the randomiser has no other use for it. */
      d->stage[s_rand].depth = (unsigned)src->randomise;
    }
  if (src->convolutional)
    {
      d->stage[s_conv].kind        = WFM_STAGE_CONV;
      d->stage[s_conv].first_field = 0u;
      d->stage[s_conv].n_fields    = n;
      d->stage[s_conv].emit_num    = 2u;
      d->stage[s_conv].emit_den    = 1u;
    }
  (void)i_asm;
  return 0;
}

int
wfm_source_attach_frame (wfm_synth_state_t *syn, const wfm_source_t *src)
{
  if (src->type != WFM_SYNTH_BITS || !src->bits || !src->n_bits)
    return 0; /* nothing to attach; mirrors wfm_synth_set_bits */
  if (!wfm_source_has_frame (src))
    return wfm_synth_set_bits (syn, src->bits, src->n_bits, src->modulation);

  /* Framed: the pattern is the whole frame, assembled by the one descriptor.
     Every caller-supplied field is LITERAL here because that is all a source
     can carry today — the generated PN/Gold kinds `wfm_seq_t` supports have
     no spelling on any face yet (gh-755). */
  wfm_frame_desc_t d;
  if (describe_source_frame (src, &d) != 0)
    return -1;

  wfm_frame_desc_layout_t lay;
  if (wfm_frame_desc_layout (&d, &lay) != 0 || lay.out_bits == 0)
    return -1;

  const size_t n    = lay.out_bits;
  uint8_t     *bits = (uint8_t *)malloc (n);
  if (!bits)
    return -1;

  /* The CCSDS kernels, because the stages a source may select are CCSDS's
     picks. wfm_frame.c cannot call them — ccsds_tm depends on it — so they
     arrive as a table, and NULL for the inner encoder's state because a
     source describes one frame that is then CYCLED to fill the record. A
     stream of frames sharing one register is a different waveform and would
     need the cycle to be a coding decision rather than a length one. */
  wfm_frame_ops_t ops;
  ccsds_tm_frame_ops (&ops, NULL);
  if (wfm_frame_assemble (&d, &ops, bits, n) != n)
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
