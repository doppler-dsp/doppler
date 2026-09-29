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
 * inside `dp_wfm_synth_set_rrc`, so standalone generation stays byte-identical
 * to the composed path.
 */
#include <stdlib.h>

#include "doppler/ccsds_tm/ccsds_tm_frame.h" /* the kernels its coded stages run */
#include "doppler/wfm/wfm_compose.h"         /* wfm_source_t */
#include "doppler/wfm/wfm_dsp.h"   /* wfm_rrc_ntaps / dp_wfm_rrc_taps */
#include "doppler/wfm/wfm_frame.h" /* the frame descriptor both faces now read */
#include "doppler/wfm_synth/wfm_synth_core.h"

/* Pulse enum index 1 == "rrc" (see the wfm_pulse [[enum]] SSOT). */
#define WFM_PULSE_RRC 1

int
dp_wfm_source_has_frame (const wfm_source_t *src)
{
  /* A carried description, a preamble or a sync word -- never `crc`; see
     the header on why. A coded frame is always a carried description (a
     CADU's [ASM | codeblock] has neither a preamble nor a sync word), so
     the description is what frames it.

     Tested on LENGTH, never on the pointer -- the same rule
     dp_wfm_frame_fixed states. A GENERATED sequence (PN, Gold) has no array
     at all, so a pointer test would read a PN sync as unframed and quietly
     emit the payload unframed; and for a LITERAL, a length with no array is
     an unbuildable description that must REACH `dp_wfm_frame_assemble` to
     be refused there rather than be silently dropped here. */
  return src
         && (src->frame != NULL || (src->acq_code.len && src->acq_reps)
             || src->sync.len);
}

static int type_can_frame (const wfm_source_t *src);
static int source_frame (const wfm_source_t *src, wfm_frame_desc_t *d);

const char dp_wfm_why_pn_poly[]
    = "pn_poly has a bit at or above bit pn_length, outside the register "
      "it configures, and the generator would mask it away: give a pn_poly "
      "inside the pn_length-bit register, or 0 for its maximal-length "
      "polynomial";

const char *
dp_wfm_source_error (const wfm_source_t *src)
{
  /* Only where a PN register is built from it: a framed bpsk is built as a
     BITS synth (dp_wfm_source_synth_type), and a tone never reads it. */
  const int t = dp_wfm_source_synth_type (src);
  if (t >= WFM_SYNTH_PN && t <= WFM_SYNTH_QPSK && src->pn_poly
      && !pn_fits_register (src->pn_poly, (uint32_t)src->pn_length))
    return dp_wfm_why_pn_poly;
  return dp_wfm_source_frame_error (src);
}

const char *
dp_wfm_source_frame_error (const wfm_source_t *src)
{
  if (!dp_wfm_source_has_frame (src))
    return NULL;
  /* One frame, said one way (docs/design/frame-description.md R). A carried
     description IS the frame, so a sync word or an unspread preamble beside
     it is a second spelling of part of it -- and one that would be silently
     dropped, because the description wins. A DSSS preamble is not a frame
     field (it is sent unspread, outside the description), so it may sit
     beside one. */
  if (src->frame
      && (src->sync.len
          || (src->type != WFM_SYNTH_DSSS && src->acq_code.len
              && src->acq_reps)))
    return "a carried frame (--frame FILE, or a scene's \"frame\") is the "
           "whole frame: put the sync word and the preamble in it as fields, "
           "or drop it and use --acq-code/--sync/--crc for the common frame";
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
      if ((src->frame || src->sync.len || src->payload.len)
          && src->data_code.len == 0)
        return "a DSSS burst spreads its frame: --data-code is required "
               "whenever there are frame bits (--sync/--bits) to spread";
    }
  else if (!type_can_frame (src))
    return "--acq-code/--sync/--frame frame a waveform, and this type "
           "carries no bit stream to frame: use --type bits/bpsk/qpsk/pn, or "
           "--type dsss to spread it";
  /* LENGTH, not the pointer: a generated payload has no array. #755 refused
     the PN-sourced types outright here because their data is endless and
     nothing bounded it; a generated payload Field is that bound, so the
     question is the one BITS always answered -- is there a payload at all
     (gh-762). */
  else if (!src->frame && src->payload.len == 0)
    return "a frame needs a payload: --bits <FIELD> (literal bits, or "
           "generated, e.g. --bits pn:1024:<pn-length> over the waveform's "
           "own register) or --bits-file";

  /* Last, and deliberately last: does the description this source resolves to
     actually lay out? Every check above is about ONE flag's value, so it can
     name the flag. This one asks the geometry itself, which is the only way a
     CARRIED description gets checked at all -- a coding stage's own rules
     (an outer code's 223*I octets, an interleaver's whole number of units)
     are this layout's and its kernels', not a flag's.
     doppler#1155: a derived field naming no producing stage laid out at zero
     length and generated an empty capture, exit 0 and nothing on stderr,
     which is the refusal-nobody-was-told-about shape --data-code already had
     to be dragged out of. */
  wfm_frame_desc_t        desc;
  wfm_frame_desc_layout_t lay;
  if (source_frame (src, &desc) != 0
      || dp_wfm_frame_desc_layout (&desc, &lay) != 0)
    return "this frame description does not lay out: a field that declares a "
           "length but supplies no bits is DERIVED and must name the stage "
           "that fills it (`derived_by` = the stage's index plus one), a "
           "derived field must be the last field its stage covers, and an "
           "emitting stage must be the only one and must cover the whole "
           "frame";

  /* And does it ASSEMBLE? A layout is geometry; each stage's kernel has
     rules of its own that only running it can ask -- an outer code takes
     exactly 223*I octets (virtual fill is not implemented, gh-813), an
     interleaver a whole number of depth x unit units, a field's generator a
     register it can build. A kernel that refuses mid-assembly used to leave
     a zero-length capture, exit 0 and nothing on stderr (measured: an
     80-bit group under an 8 x 8 interleaver). The flags that once guarded
     each rule by name are gone; this one question guards every kernel,
     including a caller's own through the same table. A frame with no bits
     -- a DSSS burst that is preamble only -- has nothing to assemble. */
  if (lay.out_bits > 0)
    {
      wfm_frame_ops_t ops;
      dp_ccsds_tm_frame_ops (&ops, NULL);
      uint8_t     *scratch = dp_xmalloc (lay.out_bits);
      const size_t got
          = dp_wfm_frame_assemble (&desc, &ops, scratch, lay.out_bits);
      free (scratch);
      if (got != lay.out_bits)
        return "this frame does not assemble: a stage's kernel refused its "
               "span -- an rs stage needs exactly 223*depth octets to cover "
               "(virtual fill is not implemented), an interleave stage a "
               "whole number of depth x unit_bits units, a randomise stage "
               "a generator (depth 0 or 1 = 10.4.1, 2 = 10.4.2 legacy), and "
               "a generated field a register it can build";
    }
  return NULL;
}

/* The source's frame as a DESCRIPTION: the carried one, else the common
 * frame `dp_wfm_frame_fixed` builds from the four fields.
 *
 * One decision here is this face's own: the preamble is a field for an
 * unspread source and is NOT one for a spread burst. That is a physical fact
 * rather than an inconsistency: a DSSS preamble is transmitted unmodulated
 * and UNSPREAD, because it is the coherent pull-in target a receiver
 * correlates raw chips against. It is therefore outside anything a stage
 * could cover, and `dp_wfm_dsss_desc_chips` prepends it around the
 * description rather than inside it.
 *
 * A carried description is copied rather than aliased so the caller may
 * reuse or free their own; the SEQUENCES it points at stay borrowed, on the
 * same terms as everywhere else here. */
static int
source_frame (const wfm_source_t *src, wfm_frame_desc_t *d)
{
  if (src->frame)
    {
      *d = *src->frame;
      return 0;
    }
  const int spread = (src->type == WFM_SYNTH_DSSS);
  return dp_wfm_frame_fixed (d, spread ? NULL : &src->acq_code,
                             spread ? 0u : src->acq_reps, &src->sync,
                             &src->payload, src->crc);
}

/* Expand a sequence into a caller-owned array, whatever produced it.
 *
 * The DSSS chip builder takes RAW ARRAYS, not a description: the spreading
 * code and a spread preamble are chips, and chips are outside anything the
 * frame descriptor covers (`dp_wfm_dsss_desc_chips` prepends the preamble
 * AROUND the description). A generated sequence has `bits == NULL`, so
 * handing one to that path reads through a null pointer -- which it did,
 * for a `data_code_gen` arriving from a record, until this existed.
 *
 * Returns a malloc'd array the caller frees, or NULL. `*n` is its length. A
 * LITERAL is returned as a copy rather than borrowed, so one free() covers
 * both cases and the caller needs no branch. Both consumers COPY what they
 * are given (`dp_wfm_synth_set_dsss_cont`'s `code` is documented "copied";
 * `dp_wfm_dsss_desc_chips` reads it during the call), so a temporary is
 * enough.
 */
static uint8_t *
seq_to_chips (const wfm_seq_t *q, size_t *n)
{
  *n = 0;
  if (!q || q->len == 0)
    return NULL;
  /* dp_xmalloc, not malloc: an OOM unwind here is a path no test can reach,
     which is exactly what the allocation-helper rule exists to remove. NULL
     from this function therefore means one thing only -- the sequence is
     unbuildable -- and that IS reachable and tested. */
  uint8_t *buf = dp_xmalloc (q->len);
  if (dp_wfm_seq_bits (q, buf, q->len) != q->len)
    {
      free (buf);
      return NULL;
    }
  *n = q->len;
  return buf;
}

/* The symbol mapping a framed source's bits take.
 *
 * A `bits` source says so with --modulation. The PN-sourced types SAY it in
 * their own name -- a framed --type qpsk is Gray-coded QPSK over the frame,
 * not over the PN stream it would otherwise have emitted -- so asking for a
 * --modulation there would be a second way to spell the type. */
static int
frame_modulation (const wfm_source_t *src)
{
  switch (src->type)
    {
    case WFM_SYNTH_QPSK:
      return 2; /* qpsk */
    case WFM_SYNTH_BPSK:
    case WFM_SYNTH_PN:
      return 1; /* bpsk */
    default:
      return src->modulation;
    }
}

/* Can this waveform type carry a frame at all?
 *
 * BITS always could. The PN-sourced types could not, for one reason: their
 * data comes from the synth's own LFSR, which is endless, so there was no
 * length to bound a payload with and #755 refused them outright. A payload
 * LENGTH is exactly what removes that -- once the payload field has a size,
 * a framed --type bpsk is the same descriptor every other source builds
 * (gh-762). CHIRP, TONE, NOISE and SYMBOLS carry no bit stream at all and
 * still cannot. */
static int
type_can_frame (const wfm_source_t *src)
{
  return src->type == WFM_SYNTH_BITS || src->type == WFM_SYNTH_BPSK
         || src->type == WFM_SYNTH_QPSK || src->type == WFM_SYNTH_PN;
}

int
dp_wfm_source_synth_type (const wfm_source_t *src)
{
  /* Exactly the sources dp_wfm_source_attach_frame hands a FRAME to, less
     BITS (already one). A framed bpsk/qpsk/pn transmits the frame, and the
     only synth that plays a bit pattern is a BITS one -- set_bits is a no-op
     on any other -- so a synth created with the source's own type played
     its LFSR and dropped the frame (doppler#1616). The mapping the type
     names still reaches the wire: attach_frame passes frame_modulation(). */
  if (src->type != WFM_SYNTH_BITS && type_can_frame (src)
      && dp_wfm_source_has_frame (src) && (src->payload.len || src->frame))
    return WFM_SYNTH_BITS;
  return src->type;
}

int
dp_wfm_source_attach_frame (dp_wfm_synth_state_t *syn, const wfm_source_t *src)
{
  /* Tested on LENGTH, not on the pointer: a GENERATED payload has no array,
     and reading it as "no payload" is how the generated kinds stayed
     unreachable everywhere else in this file. */
  if (!type_can_frame (src) || (src->payload.len == 0 && !src->frame))
    return 0; /* nothing to attach; mirrors dp_wfm_synth_set_bits */
  if (!dp_wfm_source_has_frame (src))
    {
      /* Unframed. Only a BITS source transmits its payload directly -- for
         the PN-sourced types an unframed payload is not a waveform they
         have, so they keep emitting their own stream. */
      if (src->type != WFM_SYNTH_BITS)
        return 0;
      size_t   n   = 0;
      uint8_t *bts = seq_to_chips (&src->payload, &n);
      if (!bts)
        return -1;
      const int rc = dp_wfm_synth_set_bits (syn, bts, n, src->modulation);
      free (bts);
      return rc;
    }

  /* Framed: the pattern is the whole frame, assembled by the one
     descriptor, whatever waveform type carries it. */
  wfm_frame_desc_t d;
  if (source_frame (src, &d) != 0)
    return -1;

  wfm_frame_desc_layout_t lay;
  if (dp_wfm_frame_desc_layout (&d, &lay) != 0 || lay.out_bits == 0)
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
  dp_ccsds_tm_frame_ops (&ops, NULL);
  if (dp_wfm_frame_assemble (&d, &ops, bits, n) != n)
    {
      free (bits);
      return -1;
    }
  /* set_bits copies, so the frame buffer is ours to release. */
  int rc = dp_wfm_synth_set_bits (syn, bits, n, frame_modulation (src));
  free (bits);
  return rc;
}

int
dp_wfm_source_attach_dsss (dp_wfm_synth_state_t *syn, const wfm_source_t *src,
                           double fs)
{
  if (src->type != WFM_SYNTH_DSSS)
    return 0; /* no-op, mirrors dp_wfm_synth_set_dsss_chips */
  if (src->symbol_rate > 0.0)
    {
      /* Continuous async: the data clock is independent of the code. sps is
         samples-per-CHIP for dsss, so chip_rate = fs/sps and chips/symbol =
         chip_rate/symbol_rate (non-integer — the asynchronicity). Data comes
         from the payload when supplied, else the seeded PN a receiver can
         regenerate. (Code-only, --data none, arrives with the CLI flag.) */
      double cps
          = (src->sps > 0) ? (fs / (double)src->sps) / src->symbol_rate : 0.0;
      /* LENGTH, not the pointer: a generated payload has no array, and
         reading it as "none" sent the PRBS default in its place, silently
         (doppler#1592). It is materialised the way every other sequence
         here is. */
      int      mode = src->dsss_code_only ? WFM_DSSS_DATA_NONE
                      : src->payload.len  ? WFM_DSSS_DATA_BITS
                                          : WFM_DSSS_DATA_PRBS;
      size_t   dn   = 0;
      uint8_t *dchp = seq_to_chips (&src->data_code, &dn);
      if (!dchp)
        return -1;
      size_t   pn  = 0;
      uint8_t *pay = NULL;
      if (mode == WFM_DSSS_DATA_BITS)
        {
          pay = seq_to_chips (&src->payload, &pn);
          if (!pay)
            {
              free (dchp);
              return -1;
            }
        }
      const int rc
          = dp_wfm_synth_set_dsss_cont (syn, dchp, dn, cps, mode, pay, pn);
      free (pay);
      free (dchp);
      return rc;
    }
  /* A BURST is a frame that is spread. The frame comes from the same
     description every other source is built from -- so a coding stage in a
     carried description reaches a DSSS burst by existing, instead of being
     read from the scene and then silently dropped (doppler#1017). */
  wfm_frame_desc_t d;
  if (source_frame (src, &d) != 0)
    return -1;
  const size_t n = dp_wfm_dsss_desc_nchips (&d, src->acq_code.len,
                                            src->acq_reps, src->data_code.len);
  if (n == 0)
    return -1; /* frame bits with no data code, or an empty burst */
  uint8_t *chips = malloc (n);
  if (!chips)
    return -1;
  wfm_frame_ops_t ops;
  dp_ccsds_tm_frame_ops (&ops, NULL);
  /* Both codes are chips, so both are expanded here rather than in the
     description -- see seq_to_chips above for why they cannot come from it. */
  size_t   an = 0, dn = 0;
  uint8_t *achp = seq_to_chips (&src->acq_code, &an);
  uint8_t *dchp = seq_to_chips (&src->data_code, &dn);
  if ((src->acq_code.len && !achp) || (src->data_code.len && !dchp))
    {
      free (achp);
      free (dchp);
      free (chips);
      return -1;
    }
  const size_t got = dp_wfm_dsss_desc_chips (&d, &ops, achp, an, src->acq_reps,
                                             dchp, dn, chips, n);
  free (achp);
  free (dchp);
  if (got != n)
    {
      /* A stage the description names and nothing can run, or a geometry the
         stage refuses (RS wants exactly 223*depth octets). Refuse the burst;
         a waveform missing a stage its caller asked for decodes against
         itself and syncs to nothing. */
      free (chips);
      return -1;
    }
  const int rc = dp_wfm_synth_set_dsss_chips (syn, chips, n);
  free (chips);
  return rc;
}

size_t
dp_wfm_source_dsss_nchips (const wfm_source_t *src)
{
  wfm_frame_desc_t d;
  if (!src || src->type != WFM_SYNTH_DSSS || src->symbol_rate > 0.0
      || source_frame (src, &d) != 0)
    return 0;
  return dp_wfm_dsss_desc_nchips (&d, src->acq_code.len, src->acq_reps,
                                  src->data_code.len);
}

dp_wfm_synth_state_t *
dp_wfm_source_to_synth (const wfm_source_t *src, double fs)
{
  /* A "bits" waveform with no pattern has nothing to transmit. Reject it here
     so the generated Synth_ensure_gen turns this NULL into an error at first
     generation (the old Synth.__init__ raised eagerly; standalone generation
     is lazy, so the guard moves to first steps()/step()). */
  if (src->type == WFM_SYNTH_BITS && src->payload.len == 0)
    return NULL;
  /* Likewise a "symbols" waveform needs a constellation stream. */
  if (src->type == WFM_SYNTH_SYMBOLS && (!src->symbols || !src->n_symbols))
    return NULL;
  /* A "dsss" BURST needs valid frame geometry (a preamble and/or a data-coded
     frame; frame bits require a data code). A CONTINUOUS stream (symbol_rate >
     0) has no frame — it needs only a spreading code. */
  /* Checked through the ONE compiler the render uses, so a carried frame and
     its stages are what is validated -- not a four-field sum the render never
     reads (doppler#1593). */
  if (src->type == WFM_SYNTH_DSSS && src->symbol_rate <= 0.0
      && dp_wfm_source_dsss_nchips (src) == 0)
    return NULL;
  /* LEN, not `bits`. A generated spreading code carries `bits == NULL` by
     construction -- the parameters ARE the code -- so testing the pointer
     refused every continuous `data_code_gen` before it could reach
     seq_to_chips, on the one face a record restores through. A code that is
     declared but cannot be built is caught below, where it can say so. */
  if (src->type == WFM_SYNTH_DSSS && src->symbol_rate > 0.0
      && src->data_code.len == 0)
    return NULL;
  /* A frame this waveform type cannot carry. Refusing is the whole point:
     these fields used to be accepted and dropped, so the caller got an
     unframed waveform and no way to find out. */
  if (dp_wfm_source_error (src) != NULL)
    return NULL;
  /* A sweeping chirp needs its span, and standalone there is no segment to
     lend one. It used to lock to the length of the first read, so step(),
     steps(N) and steps(64) x 16 were three different waveforms from one
     configuration (#1115). A flat chirp (f_end == freq) is a tone and has no
     slope to lose. */
  if (src->type == WFM_SYNTH_CHIRP && src->span == 0
      && src->f_end != src->freq)
    return NULL;

  /* Refer a dsss data-symbol Es/N0 to fs before create (the SSOT helper the
     composer also uses, so both faces agree to the bit). */
  int    snr_mode = 0;
  double snr_c    = dp_wfm_source_create_snr (src, fs, src->snr, &snr_mode);
  dp_wfm_synth_state_t *eng = dp_wfm_synth_create (
      dp_wfm_source_synth_type (src), fs, src->freq, snr_c, snr_mode,
      src->seed, src->sps, src->pn_length, src->pn_poly, src->lfsr,
      src->f_end);
  if (!eng)
    return NULL;
  dp_wfm_synth_set_chirp_span (eng, src->span); /* no-op for non-chirp */

  if (dp_wfm_source_attach_frame (eng, src) != 0)
    {
      dp_wfm_synth_destroy (eng);
      return NULL;
    }

  if (src->type == WFM_SYNTH_SYMBOLS && src->symbols && src->n_symbols)
    dp_wfm_synth_set_symbols (eng, src->symbols, src->n_symbols);

  if (dp_wfm_source_attach_dsss (eng, src, fs) != 0)
    {
      dp_wfm_synth_destroy (eng);
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
          dp_wfm_rrc_taps (src->rrc_beta, src->sps, src->rrc_span, taps);
          dp_wfm_synth_set_rrc (eng, taps, ntaps);
          free (taps); /* set_rrc copies the taps */
        }
    }
  return eng;
}
