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
#include <string.h>

#include "doppler/ccsds_tm/ccsds_tm_frame.h" /* the kernels its coded stages run */
#include "doppler/wfm/wfm_compose.h"         /* wfm_source_t */
#include "doppler/wfm/wfm_dsp.h"   /* wfm_rrc_ntaps / dp_wfm_rrc_taps */
#include "doppler/wfm/wfm_frame.h" /* the frame descriptor both faces now read */
#include "doppler/wfm/wfm_surface.h" /* the exclusions every face refuses */
#include "doppler/wfm_synth/wfm_synth_core.h"

/* Pulse enum index 1 == "rrc" (see the wfm_pulse [[enum]] SSOT). */
#define WFM_PULSE_RRC 1

size_t
dp_wfm_source_bits_refuse_text (const char *text, uint8_t *out, size_t max_out,
                                const char **why)
{
  (void)text;
  (void)out;
  (void)max_out;
  if (why)
    *why = "a bit field takes bits (a uint8 array); build them from text "
           "with field_bits()";
  return 0;
}

wfm_frame_desc_t *
dp_wfm_frame_refuse_text (const char *text, const char **why)
{
  (void)text;
  if (why)
    *why = "frame= takes a FrameDesc or a Frame; a description as JSON is a "
           "scene's \"frame\" key (Composer.from_json)";
  return NULL;
}

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
             || src->sync.len || src->data.len || src->data_from_file);
}

/* A data source -- `data` or `data_from_file` -- fills this source's frame
   (docs/design/payload-data-source.md). Length, never the pointer: a
   generated Field has no array. */
static int
has_data (const wfm_source_t *src)
{
  return src->data.len || src->data_from_file;
}

static int
is_cont_dsss (const wfm_source_t *src)
{
  return src->type == WFM_SYNTH_DSSS && src->symbol_rate > 0.0;
}

/* sps is samples per CHIP for dsss, so chip_rate = fs/sps, and the data
   clock is symbol_rate -- non-integer, which is the asynchronicity. */
double
dp_wfm_source_dsss_cps (const wfm_source_t *src, double fs)
{
  if (!src || !is_cont_dsss (src) || src->sps <= 0)
    return 0.0;
  return (fs / (double)src->sps) / src->symbol_rate;
}

static int type_can_frame (const wfm_source_t *src);

/* A carried frame of FIXED bits: no data:LEN field to fill, so no data
   source -- a finite source of one frame, sent once (doppler#1718). */
static int
fixed_frame (const wfm_source_t *src)
{
  return src->frame && !has_data (src) && type_can_frame (src);
}

/* The index of a description's one data:LEN field, or -1. */
static int
data_field (const wfm_frame_desc_t *d)
{
  for (unsigned i = 0; i < d->n_fields; i++)
    if (!d->field[i].derived_by && d->field[i].seq.kind == WFM_SEQ_DATA
        && d->field[i].seq.len)
      return (int)i;
  return -1;
}

/* Bits per frame of a data source on the COMMON frame: `data_len`, or 0 to
   take a finite source whole. 0 back means none could be decided -- stdin
   with no data_len, or a file that cannot be opened. */
static size_t
common_data_len (const wfm_source_t *src)
{
  if (src->data_len)
    return src->data_len;
  if (src->data.len)
    return src->data.len;
  return (size_t)dp_wfm_data_length_bits (NULL, src->data_from_file);
}

static int         source_frame (const wfm_source_t *src, wfm_frame_desc_t *d);
static const char *data_error (const wfm_source_t *src);

const char dp_wfm_why_pn_poly[]
    = "pn_poly has a bit at or above bit pn_length, outside the register "
      "it configures, and the generator would mask it away: give a pn_poly "
      "inside the pn_length-bit register, or 0 for its maximal-length "
      "polynomial";

/* doppler#1696. A dsss source's two codes do different jobs, and each one
   missing is its own sentence rather than a NULL from the builder:
   `acq_code` is the preamble a receiver acquires on, and `data_code` is what
   spreads the frame -- and, for a continuous stream, the only thing there
   is. A burst of just the preamble (no sync, no payload, no data_code) is
   valid: it is what an acquisition stimulus is, and waveforms.md says so. */
const char dp_wfm_why_dsss_frame_no_data_code[]
    = "a dsss burst spreads its frame (sync, payload, crc) with data_code, "
      "and none is given: give data_code, a spreading code such as pn:31:5 "
      "-- or no sync and no payload, for a preamble-only burst";
const char dp_wfm_why_dsss_empty[]
    = "a dsss burst has nothing to send: give acq_code, the preamble a "
      "receiver acquires on, or a payload spread by data_code, or both";
const char dp_wfm_why_dsss_cont_no_data_code[]
    = "a continuous dsss stream (symbol_rate > 0) is its spreading code, "
      "and no data_code is given: give data_code, a code such as pn:31:5";

/* The dsss half of dp_wfm_source_error: NULL, or which code is missing. A
   frame that does not lay out is not decided here -- that is
   dp_wfm_source_frame_error's, and it names its own reason. The frame's
   LAYOUT decides whether there are bits to spread, so a payload with no
   preamble, a sync word, a carried description and a data source are one
   question rather than four flags. */
static const char *
dsss_error (const wfm_source_t *src)
{
  if (src->type != WFM_SYNTH_DSSS)
    return NULL;
  if (src->symbol_rate > 0.0)
    return src->data_code.len ? NULL : dp_wfm_why_dsss_cont_no_data_code;
  wfm_frame_desc_t        d;
  wfm_frame_desc_layout_t l;
  if (source_frame (src, &d) != 0 || dp_wfm_frame_desc_layout (&d, &l) != 0)
    return NULL;
  if (l.out_bits && !src->data_code.len)
    return dp_wfm_why_dsss_frame_no_data_code;
  if (!l.out_bits && !(src->acq_code.len && src->acq_reps))
    return dp_wfm_why_dsss_empty;
  return NULL;
}

const char dp_wfm_why_retired_bits[]
    = "bits is retired: a payload is drawn from a data source -- pass data= "
      "(payload= and pattern= with it)";

const char *
dp_wfm_source_error (const wfm_source_t *src)
{
  /* First: a retired spelling is refused as such, before anything it would
     otherwise trip (doppler#1718). */
  if (src->retired_bits.len)
    return dp_wfm_why_retired_bits;
  /* Two members no face takes together (the manifest's `exclusive`). The
     CLI and a scene refuse the pair by flag and key, naming their own
     spelling; this is the object face's refusal -- a Python source or a C
     caller -- asked here, where every face asks about a source. */
  for (size_t k = 0; k < WFM_SURFACE_N_EXCLUSIVE; k++)
    {
      const wfm_surface_exclusive_t *e = &WFM_SURFACE_EXCLUSIVE[k];
      if (WFM_SURFACE[e->a].owner == WFM_SURF_SOURCE
          && wfm_surface_row_is_set (&WFM_SURFACE[e->a], src)
          && wfm_surface_row_is_set (&WFM_SURFACE[e->b], src))
        return e->obj_why;
    }
  if (src->dsss_code_only && has_data (src))
    return "--code-only sends a continuous-dsss code alone, so a data "
           "source (--data or --data-from-file) would be ignored: drop one";
  /* A bits waveform sends its data source's bits -- or a carried frame of
     fixed bits, once -- and nothing else: there is no pattern of its own to
     cycle (doppler#1718). */
  if (src->type == WFM_SYNTH_BITS && !has_data (src) && !src->frame)
    return "type=bits sends a data source's bits: give --data FIELD or "
           "--data-from-file PATH (and --data-len for its frames)";
  if (has_data (src))
    {
      const char *w = data_error (src);
      if (w)
        return w;
    }
  /* `data:LEN` is filled from a data source, so in any slot but a frame's
     payload it is a category error: a sync word, a preamble and a spreading
     code carry their own bits. */
  if ((src->sync.kind == WFM_SEQ_DATA && src->sync.len)
      || (src->acq_code.kind == WFM_SEQ_DATA && src->acq_code.len)
      || (src->data_code.kind == WFM_SEQ_DATA && src->data_code.len))
    return "data:LEN is only a frame's payload, drawn from a data source; "
           "a sync word, a preamble or a spreading code carries its own "
           "bits";
  /* Only where a PN register is built from it: a framed bpsk is built as a
     BITS synth (dp_wfm_source_synth_type), and a tone never reads it. */
  const int t = dp_wfm_source_synth_type (src);
  if (t >= WFM_SYNTH_PN && t <= WFM_SYNTH_QPSK && src->pn_poly
      && !pn_fits_register (src->pn_poly, (uint32_t)src->pn_length))
    return dp_wfm_why_pn_poly;
  const char *why = dsss_error (src);
  return why ? why : dp_wfm_source_frame_error (src);
}

const char *
dp_wfm_source_to_synth_error (const wfm_source_t *src, double fs)
{
  /* A standalone source is a scene of one segment at the bridge's fs, so
     the rules that need a rate (dp_wfm_scene_error's) are asked too, by the
     one validator rather than a second. */
  wfm_segment_t g
      = { .sources = (wfm_source_t *)src, .n_sources = 1, .fs = fs };
  return dp_wfm_scene_error (&g, 1, 0, 0);
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
         exit 0 -- a refusal nobody was told about. The rule is dsss_error's,
         by the frame's layout, asked here too so this function answers for
         every frame it is shown. */
      const char *why = dsss_error (src);
      if (why)
        return why;
    }
  else if (!type_can_frame (src))
    return "--acq-code/--sync/--frame frame a waveform, and this type "
           "carries no bit stream to frame: use --type bits/bpsk/qpsk/pn, or "
           "--type dsss to spread it";
  /* The common frame's payload is its data:LEN field, filled frame by
     frame from a data source (doppler#1718). A CARRIED description is the
     whole frame and may be fixed bits throughout -- sent once, never
     cycled -- or name a data:LEN field a data source fills. */
  else if (!src->frame && !has_data (src))
    return "a frame's payload is a data source: give --data FIELD or "
           "--data-from-file PATH";

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

  /* A frame with a data:LEN field lays out -- the description knows its
     length -- and its bits are a data source's. Without one it is named
     here, because the assembler below can only report it as a field it
     could not build. With one, the probe assembles over a zero chunk:
     geometry and kernels are what it asks, never the data. */
  const int di = data_field (&desc);
  if (di >= 0 && !has_data (src))
    return "this frame's data:LEN field draws from a data source: give one "
           "with --data or --data-from-file";

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
      uint8_t     *chunk = di >= 0 ? dp_xcalloc (lay.field_bits[di], 1) : NULL;
      const size_t got   = dp_wfm_frame_assemble_data (&desc, &ops, chunk,
                                                       scratch, lay.out_bits);
      free (chunk);
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
  /* The common frame's payload is `data:LEN`, its bits drawn per frame
     from the data source; with none (a dsss burst of preamble and sync
     only) it is empty. */
  const wfm_seq_t dq = { .kind = WFM_SEQ_DATA, .len = common_data_len (src) };
  const wfm_seq_t none = { 0 };
  return dp_wfm_frame_fixed (d, spread ? NULL : &src->acq_code,
                             spread ? 0u : src->acq_reps, &src->sync,
                             has_data (src) ? &dq : &none, src->crc);
}

/* What a data source refuses before the first sample, named with its fix
   (payload-data-source.md section 4.4). The source's own bits -- a remainder,
   an empty file -- are dp_wfm_data_create_seq's to refuse at build; this is
   the part a face can decide from the spec alone, without opening stdin. */
static const char *
data_error (const wfm_source_t *src)
{
  if (is_cont_dsss (src))
    {
      /* No frame: one bit per data symbol, so the frame's rows have nothing
         to mean here, and each says so rather than being ignored. */
      if (src->data_len)
        return "a continuous dsss source has no frame: its data is one bit "
               "per data symbol, so data_len does not apply";
      if (src->fill.len)
        return "a continuous dsss source has no frame to pad: its data is "
               "one bit per data symbol, so fill does not apply";
      if (src->data.kind == WFM_SEQ_DATA)
        return "data:LEN has no bits of its own: it is the frame's field "
               "that --data fills, not a source or a fill";
      if (!dp_wfm_source_data_is_stream (src) && !src->data.len
          && dp_wfm_data_length_bits (NULL, src->data_from_file) == 0)
        return "--data-from-file: a file that cannot be opened, is empty, "
               "or is not a regular file (a stream is -, stdin)";
      return NULL;
    }
  if (!type_can_frame (src) && src->type != WFM_SYNTH_DSSS)
    return "a data source fills a frame's payload: use --type "
           "bits/bpsk/qpsk/pn/dsss";
  if (src->data.kind == WFM_SEQ_DATA || src->fill.kind == WFM_SEQ_DATA)
    return "data:LEN has no bits of its own: it is the frame's field that "
           "--data fills, not a source or a fill";
  const int stdin_src
      = src->data_from_file && strcmp (src->data_from_file, "-") == 0;
  if (stdin_src && src->fill.len == 0)
    return "stdin is a stream: its last frame, and an idle frame when "
           "--realtime finds nothing yet, need --fill";
  size_t len;
  if (src->frame)
    {
      const int i = data_field (src->frame);
      if (i < 0)
        return "a data source fills a data:LEN field, and this carried "
               "--frame has none: add one (the common frame has it)";
      len = src->frame->field[i].seq.len;
      if (src->data_len && src->data_len != len)
        return "--data-len must be 0 or the carried frame's data:LEN";
    }
  else
    {
      len = common_data_len (src);
      if (len == 0)
        return stdin_src ? "stdin has no length to take whole as one "
                           "frame: give --data-len"
                         : "--data-from-file: a file that cannot be opened, "
                           "is empty, or is not a regular file (a stream "
                           "is -, stdin)";
    }
  /* The remainder rule (section 4.4), decided here for every finite source
     whose length is known without reading it. */
  const uint64_t total
      = src->data.len
            ? (uint64_t)src->data.len
            : (stdin_src
                   ? 0u
                   : dp_wfm_data_length_bits (NULL, src->data_from_file));
  if (!stdin_src && total == 0)
    return "--data-from-file: a file that cannot be opened, is empty, or is "
           "not a regular file (a stream is -, stdin)";
  if (total % len && src->fill.len == 0)
    return "the data does not fill its last frame and no --fill is "
           "declared: give one, or a --data-len that divides it";
  return NULL;
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
      && dp_wfm_source_has_frame (src) && (src->frame || has_data (src)))
    return WFM_SYNTH_BITS;
  return src->type;
}

int
dp_wfm_source_attach_frame (dp_wfm_synth_state_t *syn, const wfm_source_t *src)
{
  /* A data source: each frame is pulled from it, the description assembled
     over its next chunk (dp_wfm_synth_attach_data). */
  if (has_data (src) && type_can_frame (src))
    {
      wfm_frame_desc_t d;
      int              i = -1;
      /* A spec dp_wfm_source_error passed always has both; a C caller that
         skipped it gets -1 rather than a frame built over nothing. */
      if (source_frame (src, &d) != 0 || (i = data_field (&d)) < 0)
        return -1;
      wfm_data_src_t *ds = dp_wfm_data_create_seq (
          src->data_from_file ? NULL : &src->data, src->data_from_file,
          d.field[i].seq.len, &src->fill, NULL);
      if (!ds)
        return -1;
      wfm_frame_ops_t ops;
      dp_ccsds_tm_frame_ops (&ops, NULL);
      return dp_wfm_synth_attach_data (syn, &d, &ops, ds, WFM_DATA_UNPACED,
                                       frame_modulation (src));
    }
  if (!type_can_frame (src) || !src->frame)
    return 0; /* nothing to attach; mirrors dp_wfm_synth_set_bits */

  /* A carried frame of FIXED bits: assembled once and sent once -- the
     synth's cursor stops at its end (doppler#1718 deleted the cycle). The
     CCSDS kernels, because the stages a source may select are CCSDS's
     picks; wfm_frame.c cannot call them, so they arrive as a table, and
     NULL for the inner encoder's state because this is one frame. */
  wfm_frame_desc_t        d = *src->frame;
  wfm_frame_desc_layout_t lay;
  if (dp_wfm_frame_desc_layout (&d, &lay) != 0 || lay.out_bits == 0)
    return -1;
  uint8_t        *bits = dp_xmalloc (lay.out_bits);
  wfm_frame_ops_t ops;
  dp_ccsds_tm_frame_ops (&ops, NULL);
  int rc = -1;
  if (dp_wfm_frame_assemble (&d, &ops, bits, lay.out_bits) == lay.out_bits)
    rc = dp_wfm_synth_set_bits (syn, bits, lay.out_bits,
                                frame_modulation (src));
  free (bits); /* set_bits copies */
  return rc;
}

/* ── the pull: a frame per chunk of a data source ─────────────────────── */

typedef struct
{
  wfm_frame_desc_t  d;
  wfm_frame_ops_t   ops;
  int               has_ops;
  wfm_data_src_t   *src;
  wfm_data_pacing_t pacing;
  size_t            reps, chunk_bits;
  uint8_t          *chunk;
  /* A dsss burst: the frame is spread after it is assembled, by these
     codes (dp_wfm_dsss_desc_chips_data). `dcode` NULL is a plain frame. */
  uint8_t *acq, *dcode;
  size_t   an, acq_reps, dn;
  /* No frame at all (continuous dsss): the chunk IS the bits, copied as
     drawn -- one data bit per pull. */
  int raw;
} data_pull_t;

static void
data_pull_free (void *u)
{
  data_pull_t *p = u;
  dp_wfm_data_destroy (p->src);
  free (p->chunk);
  free (p->acq);
  free (p->dcode);
  free (p);
}

/* One frame: the next chunk under the pacing rule, assembled into `bits`.
   Returns 0, or 1 when the data has ended (an error ends it too: a frame
   that cannot be built is not sent). */
static int
data_pull_refill (void *u, uint8_t *bits, size_t n)
{
  data_pull_t            *p  = u;
  const wfm_data_status_t st = dp_wfm_data_frame (p->src, p->pacing, p->reps,
                                                  p->chunk, p->chunk_bits);
  if (st != WFM_DATA_FRAME && st != WFM_DATA_IDLE)
    return 1;
  if (p->raw)
    {
      memcpy (bits, p->chunk, n);
      return 0;
    }
  const wfm_frame_ops_t *ops = p->has_ops ? &p->ops : NULL;
  const size_t           got
      = p->dcode ? dp_wfm_dsss_desc_chips_data (&p->d, ops, p->chunk, p->acq,
                                                p->an, p->acq_reps, p->dcode,
                                                p->dn, bits, n)
                 : dp_wfm_frame_assemble_data (&p->d, ops, p->chunk, bits, n);
  return got == n ? 0 : 1;
}

/* A pull over description `d`, owning `src` from here, success or not:
   NULL (and `src` destroyed) when `d` has no data:LEN field to fill. */
static data_pull_t *
pull_new (const wfm_frame_desc_t *d, const wfm_frame_ops_t *ops,
          wfm_data_src_t *src, wfm_data_pacing_t pacing)
{
  data_pull_t *p = dp_xcalloc (1, sizeof *p);
  p->src         = src;
  wfm_frame_desc_layout_t l;
  int                     at = -1;
  if (d && dp_wfm_frame_desc_layout (d, &l) == 0)
    at = data_field (d);
  if (!src || at < 0 || l.out_bits == 0)
    {
      data_pull_free (p);
      return NULL;
    }
  p->d          = *d;
  p->has_ops    = ops != NULL;
  p->ops        = ops ? *ops : (wfm_frame_ops_t){ 0 };
  p->pacing     = pacing;
  p->reps       = d->field[at].reps ? d->field[at].reps : 1u;
  p->chunk_bits = l.field_bits[at];
  p->chunk      = dp_xcalloc (p->chunk_bits, 1);
  /* The pull is lazy -- no frame is assembled until its first bit is due --
     so a description that cannot assemble (a field that cannot be built, a
     stage with no kernel) is found HERE, over a zero chunk, and refuses the
     build. Found at the first bit, it would end the data in silence: a
     refusal nobody is told about. */
  uint8_t     *dry = dp_xmalloc (l.out_bits);
  const size_t got
      = dp_wfm_frame_assemble_data (d, ops, p->chunk, dry, l.out_bits);
  free (dry);
  if (got != l.out_bits)
    {
      data_pull_free (p);
      return NULL;
    }
  return p;
}

/* Hand `p` to the synth as its refill, the pattern `n` long -- a frame's
   bits, or a burst's chips -- and PARK the cursor at its end.

   NOTHING is drawn here, not even the first frame: the cursor is parked at
   the frame boundary, so frame 1 is pulled at the first bit, like every
   frame after it. That is what lets a composer build a source just to
   validate it, and Plan or a repeat build it again, without reading a byte
   of stdin (payload-data-source.md, D-c). The pattern set here is a
   placeholder of the frame's length, never sent. */
static int
pull_park (dp_wfm_synth_state_t *syn, data_pull_t *p, size_t n, int modulation)
{
  uint8_t *blank = dp_xcalloc (n, 1);
  /* A raw pull's placeholder is the payload dp_wfm_synth_set_dsss_cont()
     already installed; a frame's or a burst's is set here. */
  const int rc = p->raw ? 0
                 : p->dcode
                     ? dp_wfm_synth_set_dsss_chips (syn, blank, n)
                     : dp_wfm_synth_set_bits (syn, blank, n, modulation);
  free (blank);
  /* Only a bits synth is fed FRAMES (set_bits is a no-op on any other, so
     the type is checked here, where every kind of pull parks); a dsss
     pull is checked by set_refill, which takes a dsss synth. */
  if (rc != 0 || (!p->dcode && !p->raw && syn->wtype != WFM_SYNTH_BITS)
      || dp_wfm_synth_set_refill (syn, data_pull_refill, p, data_pull_free)
             != 0)
    {
      data_pull_free (p);
      return -1;
    }
  syn->bit_idx = syn->n_bits;
  return 0;
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
         from a data source when one is given, else the seeded PN a receiver
         can regenerate, or nothing at all with --code-only. */
      const double cps  = dp_wfm_source_dsss_cps (src, fs);
      size_t       dn   = 0;
      uint8_t     *dchp = seq_to_chips (&src->data_code, &dn);
      if (!dchp)
        return -1;
      if (has_data (src))
        {
          /* A data source (doppler#1719): no frame, so one bit per pull,
             one per data symbol -- a pull size of one needs no fill, and a
             finite source ends exactly at its last bit. */
          static const uint8_t one[1] = { 0 };
          const int            rc     = dp_wfm_synth_set_dsss_cont (
              syn, dchp, dn, cps, WFM_DSSS_DATA_BITS, one, 1);
          free (dchp);
          wfm_data_src_t *ds = dp_wfm_data_create_seq (
              src->data_from_file ? NULL : &src->data, src->data_from_file, 1,
              NULL, NULL);
          if (rc != 0 || !ds)
            {
              dp_wfm_data_destroy (ds);
              return -1;
            }
          data_pull_t *p = dp_xcalloc (1, sizeof *p);
          p->src         = ds;
          p->pacing      = WFM_DATA_UNPACED;
          p->reps        = 1;
          p->chunk_bits  = 1;
          p->chunk       = dp_xmalloc (1);
          p->raw         = 1;
          return pull_park (syn, p, 1, 0);
        }
      const int mode
          = src->dsss_code_only ? WFM_DSSS_DATA_NONE : WFM_DSSS_DATA_PRBS;
      const int rc
          = dp_wfm_synth_set_dsss_cont (syn, dchp, dn, cps, mode, NULL, 0);
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
  /* A data source: one burst per chunk, assembled and spread when it is
     due, in place of the one burst assembled below and cycled. The pull
     owns the codes from here. */
  if (has_data (src))
    {
      free (chips);
      const int    i = data_field (&d);
      data_pull_t *p = NULL;
      if (i >= 0)
        p = pull_new (
            &d, &ops,
            dp_wfm_data_create_seq (src->data_from_file ? NULL : &src->data,
                                    src->data_from_file, d.field[i].seq.len,
                                    &src->fill, NULL),
            WFM_DATA_UNPACED);
      if (!p)
        {
          free (achp);
          free (dchp);
          return -1;
        }
      p->acq      = achp;
      p->an       = an;
      p->acq_reps = src->acq_reps;
      p->dcode    = dchp;
      p->dn       = dn;
      return pull_park (syn, p, n, 1);
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
  /* A "symbols" waveform needs a constellation stream. */
  if (src->type == WFM_SYNTH_SYMBOLS && (!src->symbols || !src->n_symbols))
    return NULL;
  /* A frame this waveform type cannot carry, a dsss source missing a code,
     a continuous one below a chip per symbol at this fs. Refusing is the
     whole point: these fields used to be accepted and dropped, so the caller
     got an unframed waveform and no way to find out. Asked through the one
     validator, so Synth.steps() raises the sentence every face gives. */
  if (dp_wfm_source_to_synth_error (src, fs) != NULL)
    return NULL;
  /* A "dsss" BURST needs valid frame geometry, checked through the ONE
     compiler the render uses, so a carried frame and its stages are what is
     validated -- not a four-field sum the render never reads
     (doppler#1593). */
  if (src->type == WFM_SYNTH_DSSS && src->symbol_rate <= 0.0
      && dp_wfm_source_dsss_nchips (src) == 0)
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

int
dp_wfm_synth_attach_data (dp_wfm_synth_state_t *syn, const wfm_frame_desc_t *d,
                          const wfm_frame_ops_t *ops, wfm_data_src_t *src,
                          wfm_data_pacing_t pacing, int modulation)
{
  data_pull_t *p = pull_new (d, ops, src, pacing);
  if (!p)
    return -1;
  wfm_frame_desc_layout_t l;
  (void)dp_wfm_frame_desc_layout (d, &l); /* pull_new has checked it */
  return pull_park (syn, p, l.out_bits, modulation);
}

int
dp_wfm_source_data_is_stream (const wfm_source_t *src)
{
  return src && src->data_from_file && strcmp (src->data_from_file, "-") == 0;
}

uint64_t
dp_wfm_source_data_samples (const wfm_source_t *src, double fs,
                            uint64_t frames)
{
  if (!src || (!has_data (src) && !fixed_frame (src)))
    return 0;
  const uint64_t sps = src->sps > 0 ? (uint64_t)src->sps : 1u;
  if (is_cont_dsss (src))
    {
      /* A frame here is one data bit, one per data symbol, and the run is
         every chip before the symbol after the last: that symbol's edge, on
         the one symbol clock the synth's kernel runs on, so the run ends on
         the chip where the waveform's data does by construction. */
      const double cps = dp_wfm_source_dsss_cps (src, fs);
      if (!(cps >= 1.0))
        return 0;
      return dp_wfm_dsss_cont_edge (frames, cps) * sps;
    }
  wfm_frame_desc_t        d;
  wfm_frame_desc_layout_t l;
  if (source_frame (src, &d) != 0 || dp_wfm_frame_desc_layout (&d, &l) != 0)
    return 0;
  /* A dsss burst is a frame spread: its chips, sps samples each. */
  if (src->type == WFM_SYNTH_DSSS)
    return frames * dp_wfm_source_dsss_nchips (src) * sps;
  /* Symbols per frame at the frame's own mapping (a bpsk/pn frame is one
     bit per symbol, qpsk two, a bits pattern its `modulation`, 0 meaning
     one), then sps samples each -- what the synth emits per frame. A frame
     that does not divide into symbols ends on a symbol the next frame
     completes (wfm_synth_bit_symbol), so it rounds UP. */
  const int      m   = frame_modulation (src);
  const uint64_t bps = m > 0 ? (uint64_t)m : 1u;
  return frames * ((l.out_bits + bps - 1u) / bps * sps);
}

uint64_t
dp_wfm_source_data_frames (const wfm_source_t *src)
{
  if (src && fixed_frame (src))
    return 1; /* the frame itself, sent once */
  if (!src || !has_data (src) || dp_wfm_source_data_is_stream (src))
    return 0;
  const uint64_t total
      = src->data.len ? (uint64_t)src->data.len
                      : dp_wfm_data_length_bits (NULL, src->data_from_file);
  if (is_cont_dsss (src))
    return total; /* no frame: one bit per data symbol */
  wfm_frame_desc_t d;
  int              i = -1;
  if (source_frame (src, &d) != 0 || (i = data_field (&d)) < 0
      || d.field[i].seq.len == 0)
    return 0;
  const uint64_t len = d.field[i].seq.len;
  return (total + len - 1u) / len;
}

void
dp_wfm_synth_set_data_pacing (dp_wfm_synth_state_t *syn,
                              wfm_data_pacing_t     pacing)
{
  if (syn && syn->refill == data_pull_refill)
    ((data_pull_t *)syn->refill_user)->pacing = pacing;
}

const wfm_data_src_t *
dp_wfm_synth_data_source (const dp_wfm_synth_state_t *syn)
{
  return syn && syn->refill == data_pull_refill
             ? ((const data_pull_t *)syn->refill_user)->src
             : NULL;
}
