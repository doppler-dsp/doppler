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
             || src->interleave_depth || src->randomise || src->convolutional);
}

const char *
wfm_source_frame_error (const wfm_source_t *src)
{
  if (!wfm_source_has_frame (src))
    return NULL;
  if (src->type == WFM_SYNTH_DSSS)
    {
      /* A CONTINUOUS dsss stream has no frame at all; the CLI and the
         composer refuse the burst-frame flags alongside --symbol-rate before
         reaching here, so there is nothing left to check. */
      if (src->symbol_rate > 0.0)
        return NULL;
      /* A burst SPREADS its frame, so frame bits without a code are not a
         geometry this can build. It used to leave a zero-length capture and
         exit 0 -- a refusal nobody was told about. */
      if ((src->n_sync || src->n_bits) && src->n_data_code == 0)
        return "a DSSS burst spreads its frame: --data-code is required "
               "whenever there are frame bits (--sync/--bits) to spread";
    }
  else if (src->type != WFM_SYNTH_BITS)
    return "--acq-code/--sync frame a waveform, and a frame needs an explicit "
           "payload: use --type bits with --bits (--modulation bpsk|qpsk), or "
           "--type dsss to spread it";
  else if (!src->bits || src->n_bits == 0)
    return "a frame needs a payload: --type bits with --acq-code/--sync also "
           "needs --bits";

  /* The stage rules below reach a DSSS burst too, now that its frame is the
     same description every other source's is (doppler#1017). Before that they
     were unreachable for it in the worst way: the flags parsed, the stage was
     dropped, and the waveform came out looking fine. */

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

  /* The interleaver's geometry, checked at the FLAG for the same reason the
     outer code's is: the kernel would refuse mid-assembly, and a caller who
     asked for a permutation the span cannot hold would get a zero-length
     record and no sentence saying why. Measured on this exact case -- an
     80-bit data group with `--interleave 8 --interleave-unit 8` -- which
     produced an empty file and nothing on stderr.

     The COLUMN count follows from the span, so what has to divide is
     `depth * unit` into the data group. */
  if (src->interleave_depth != 0)
    {
      const size_t unit
          = src->interleave_unit_bits ? src->interleave_unit_bits : 1u;
      /* The data group is payload + CRC + the OUTER CODE'S CHECK SYMBOLS,
         which is what `ccsds_tm_frame_desc_of` gives the interleave stage
         to cover ("payload, its CRC, and the outer code's check symbols")
         and is the only span that makes the transform mean anything: an
         interleaver exists to spread a burst across codewords, so leaving
         the parity contiguous would defeat the point.

         This guard omitted the parity, so it validated a DIFFERENT span
         from the one the stage permutes. With --rs-depth 1 the check ran
         against 1784 bits while the stage covered 2040, which refused the
         canonical CCSDS arrangement -- 223 octets under RS(255,223),
         interleaved 5 deep at unit 8 -- even though 2040 divides by 40
         exactly. `bits_interleave_octets_with_outer_code` had that refusal
         pinned in the flag-matrix golden as though it were correct. */
      const size_t parity
          = src->rs_depth ? (size_t)CCSDS_TM_RS_2E * (size_t)src->rs_depth * 8u
                          : 0u;
      const size_t data
          = src->n_bits + (src->crc ? WFM_FRAME_CRC_BITS : 0u) + parity;
      const size_t cell = (size_t)src->interleave_depth * unit;
      if (cell == 0 || data == 0 || data % cell != 0)
        return "--interleave needs the data group (payload, its CRC if any, "
               "and the outer code's check symbols) to be a whole number of "
               "depth x unit units — the column count follows from the span, "
               "so a remainder has nowhere to go and is refused rather than "
               "padded";
    }
  return NULL;
}

/* The source's frame as a DESCRIPTION — an ADAPTER, not a second layout.
 *
 * Which stage covers what is `ccsds_tm_frame_desc_of`'s, because the covers
 * are that standard's and `wfm/wfm_frame.h` deliberately knows nothing about
 * CCSDS. What is left here is this face's own decision: the DSSS preamble is
 * a field for an unspread source and is NOT one for a spread burst.
 *
 * That is a physical fact rather than an inconsistency: a DSSS preamble is
 * transmitted unmodulated and UNSPREAD, because it is the coherent pull-in
 * target a receiver correlates raw chips against. It is therefore outside
 * anything a stage could cover, and `wfm_dsss_desc_chips` prepends it around
 * the description rather than inside it.
 */
int
wfm_source_describe_frame (const wfm_source_t *src, wfm_frame_desc_t *d)
{
  const int                   spread = (src->type == WFM_SYNTH_DSSS);
  const ccsds_tm_frame_spec_t s      = {
    .attach_asm           = src->attach_asm,
    .preamble             = spread ? NULL : src->acq_code,
    .preamble_len         = spread ? 0u : src->n_acq_code,
    .preamble_reps        = spread ? 0u : src->acq_reps,
    .sync                 = src->sync,
    .sync_len             = src->n_sync,
    .payload              = src->bits,
    .payload_len          = src->n_bits,
    .crc                  = src->crc,
    .rs_depth             = src->rs_depth,
    .randomise            = src->randomise,
    .convolutional        = src->convolutional,
    .interleave_depth     = src->interleave_depth,
    .interleave_unit_bits = src->interleave_unit_bits,
  };
  return ccsds_tm_frame_desc_of (&s, d);
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
  if (wfm_source_describe_frame (src, &d) != 0)
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
  /* A BURST is a frame that is spread. The frame comes from the same
     description every other source is built from -- so `--conv`, `--asm`,
     `--rs-depth` and `--randomise` reach a DSSS burst by existing, instead of
     being read from the scene and then silently dropped (doppler#1017). */
  wfm_frame_desc_t d;
  if (wfm_source_describe_frame (src, &d) != 0)
    return -1;
  const size_t n = wfm_dsss_desc_nchips (&d, src->n_acq_code, src->acq_reps,
                                         src->n_data_code);
  if (n == 0)
    return -1; /* frame bits with no data code, or an empty burst */
  uint8_t *chips = malloc (n);
  if (!chips)
    return -1;
  wfm_frame_ops_t ops;
  ccsds_tm_frame_ops (&ops, NULL);
  const size_t got = wfm_dsss_desc_chips (
      &d, &ops, src->acq_code, src->n_acq_code, src->acq_reps, src->data_code,
      src->n_data_code, chips, n);
  if (got != n)
    {
      /* A stage the description names and nothing can run, or a geometry the
         stage refuses (RS wants exactly 223*depth octets). Refuse the burst;
         a waveform missing a stage its caller asked for decodes against
         itself and syncs to nothing. */
      free (chips);
      return -1;
    }
  const int rc = wfm_synth_set_dsss_chips (syn, chips, n);
  free (chips);
  return rc;
}

size_t
wfm_source_dsss_nchips (const wfm_source_t *src)
{
  wfm_frame_desc_t d;
  if (!src || src->type != WFM_SYNTH_DSSS || src->symbol_rate > 0.0
      || wfm_source_describe_frame (src, &d) != 0)
    return 0;
  return wfm_dsss_desc_nchips (&d, src->n_acq_code, src->acq_reps,
                               src->n_data_code);
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
