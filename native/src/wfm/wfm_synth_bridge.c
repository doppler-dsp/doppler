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
#include "doppler/dp_hash64.h"               /* the per-PPDU scrambler draw */
#include "doppler/wfm/wfm_compose.h"         /* wfm_source_t */
#include "doppler/wfm/wfm_dsp.h"   /* wfm_rrc_ntaps / dp_wfm_rrc_taps */
#include "doppler/wfm/wfm_frame.h" /* the frame descriptor both faces now read */
#include "doppler/wfm/wfm_surface.h" /* the exclusions every face refuses */
#include "doppler/wfm_synth/wfm_synth_core.h"
#include "doppler/wlan/wlan_core.h" /* the ofdm type's renderer */

/* The bits a sequence field reads as, through dp_wfm_seq_bits (the one place
   a wfm_seq_t becomes bits). With no buffer it only counts: a field with
   bits or a generated kind has `len` of them; WFM_SEQ_DATA has none of its
   own. */
static size_t
seq_read (const wfm_seq_t *s, uint8_t *out, size_t max)
{
  if (s->len == 0 || s->kind == WFM_SEQ_DATA)
    return 0;
  if (!out)
    return (s->kind == WFM_SEQ_LITERAL && !s->bits) ? 0 : s->len;
  return dp_wfm_seq_bits (s, out, max);
}

#define SEQ_READER(field)                                                     \
  size_t dp_wfm_source_##field##_bits (const wfm_source_t *src, double fs,    \
                                       uint8_t *out, size_t max)              \
  {                                                                           \
    (void)fs;                                                                 \
    return seq_read (&src->field, out, max);                                  \
  }
SEQ_READER (acq_code)
SEQ_READER (data_code)
SEQ_READER (data)
SEQ_READER (fill)
SEQ_READER (mac_header)

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

size_t
dp_wfm_source_sync_refuse_text (const char *text, uint8_t *out, size_t max_out,
                                const char **why)
{
  (void)text;
  (void)out;
  (void)max_out;
  if (why)
    *why = dp_wfm_why_retired_sync;
  return 0;
}

size_t
dp_wfm_source_crc_refuse_text (const char *text, uint8_t *out, size_t max_out,
                               const char **why)
{
  (void)text;
  (void)out;
  (void)max_out;
  if (why)
    *why = dp_wfm_why_retired_crc;
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

/* The two reasons a source cannot be framed from flags, said once. */
static const char WHY_CANNOT_FRAME[]
    = "--acq-code/--sync/--frame frame a waveform, and this type carries no "
      "bit stream to frame: use --type bits/bpsk/qpsk/pn, or --type dsss to "
      "spread it";
static const char WHY_NEEDS_DATA[]
    = "a frame's payload is a data source: give --data FIELD or "
      "--data-from-file PATH";

static int         type_can_frame (const wfm_source_t *src);
static int         has_data (const wfm_source_t *src);
static const char *ofdm_source_error (const wfm_source_t *src);

const char *
dp_wfm_framing_flags_error (const wfm_source_t *src)
{
  if (src->type == WFM_SYNTH_DSSS)
    return NULL; /* a burst of preamble and sync alone is a burst */
  if (src->type == WFM_SYNTH_FRAMES)
    return NULL; /* the PSDU is the payload; ofdm_source_error decides */
  if (!type_can_frame (src))
    return WHY_CANNOT_FRAME;
  if (!has_data (src))
    return WHY_NEEDS_DATA;
  return NULL;
}

int
dp_wfm_source_has_frame (const wfm_source_t *src)
{
  /* A carried description, a preamble or a data source. A coded frame is
     always a carried description (a CADU's [ASM | codeblock] has no
     preamble), so the description is what frames it.

     Tested on LENGTH, never on the pointer -- the same rule
     dp_wfm_frame_fixed states. A GENERATED sequence (PN, Gold) has no array
     at all, so a pointer test would read a PN sync as unframed and quietly
     emit the payload unframed; and for a LITERAL, a length with no array is
     an unbuildable description that must REACH `dp_wfm_frame_assemble` to
     be refused there rather than be silently dropped here. */
  return src
         && (src->frame != NULL || (src->acq_code.len && src->acq_reps)
             || src->data.len || src->data_from_file);
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

const char dp_wfm_why_retired_sync[]
    = "sync is retired: the frame-sync word is a field of the frame "
      "description -- pass frame= (a FrameDesc with the sync word as its "
      "first field); the CLI's --sync builds that description for you";

const char dp_wfm_why_retired_crc[]
    = "crc is retired: a CRC is a stage of the frame description -- pass "
      "frame= (a FrameDesc with a crc16 stage); the CLI's --crc builds that "
      "description for you";

const char dp_wfm_why_retired_bits[]
    = "bits is retired: a payload is drawn from a data source -- pass data= "
      "(payload= and pattern= with it)";

/* The per-frame code-only window: of every frame_symbols symbols the first
   code_only_symbols carry the code alone, and the data pauses through them.
   Asked here, for every face: the CLI, a scene and a Python Synth all reach
   a source through this one question, so each sentence names the FIELD, as
   dp_wfm_why_dsss_cont_no_data_code does -- a flag would be the wrong
   spelling on two faces of three. */
static const char *
window_error (const wfm_source_t *src)
{
  const size_t w = src->code_only_symbols, f = src->frame_symbols;
  if (!w && !f)
    return NULL;
  if (!is_cont_dsss (src))
    return "code_only_symbols and frame_symbols frame a continuous dsss "
           "stream, and this source is not one: give type dsss with a "
           "symbol_rate, or drop them";
  if (!f)
    return "code_only_symbols opens a frame, and no frame_symbols is "
           "given: give frame_symbols, the frame's length in symbols";
  /* A frame exists only to place its window: without one it changes no
     sample, so it is refused rather than silently ignored. */
  if (!w)
    return "frame_symbols frames a code-only window, and no "
           "code_only_symbols is given: give code_only_symbols, or drop "
           "frame_symbols";
  if (w > f)
    return "code_only_symbols exceeds frame_symbols: the window opens the "
           "frame, so it is at most the frame";
  /* The one field whose spelling differs by face: Python's dsss_code_only
     is a scene's code_only, so the sentence names both. */
  if (src->dsss_code_only)
    return "a code-only stream (dsss_code_only, a scene's code_only) "
           "already sends the code alone on every symbol, so a code-only "
           "window changes nothing: drop one";
  if (w == f && has_data (src))
    return "code_only_symbols equal to frame_symbols leaves no data "
           "symbol, so the data source would never be sent: shorten the "
           "window";
  return NULL;
}

const char *
dp_wfm_source_error (const wfm_source_t *src)
{
  /* First: a retired spelling is refused as such, before anything it would
     otherwise trip (doppler#1718). */
  if (src->retired_bits.len)
    return dp_wfm_why_retired_bits;
  if (src->retired_sync.len)
    return dp_wfm_why_retired_sync;
  if (src->retired_crc.len)
    return dp_wfm_why_retired_crc;
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
  const char *window_why = window_error (src);
  if (window_why)
    return window_why;
  const char *ofdm_why = ofdm_source_error (src);
  if (ofdm_why)
    return ofdm_why;
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
     payload it is a category error: a preamble and a spreading code carry
     their own bits. */
  if ((src->acq_code.kind == WFM_SEQ_DATA && src->acq_code.len)
      || (src->data_code.kind == WFM_SEQ_DATA && src->data_code.len))
    return "data:LEN is only a frame's payload, drawn from a data source; "
           "a preamble or a spreading code carries its own bits";
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
  /* An ofdm source's PSDU is its data source's frame, built by the PHY:
     ofdm_source_error (asked by dp_wfm_source_error) has decided it. */
  if (src->type == WFM_SYNTH_FRAMES)
    return NULL;
  /* One frame, said one way (docs/design/frame-description.md R). A carried
     description IS the frame, so an unspread preamble beside it is a second
     spelling of part of it -- and one that would be silently dropped,
     because the description wins. A DSSS preamble is not a frame field (it
     is sent unspread, outside the description), so it may sit beside one.
     (A sync word or a CRC beside it is the retired spelling, refused by
     name in dp_wfm_source_error.) */
  if (src->frame && src->type != WFM_SYNTH_DSSS && src->acq_code.len
      && src->acq_reps)
    return "a carried frame (--frame FILE, or a scene's \"frame\") is the "
           "whole frame: put the preamble in it as a field, or drop it and "
           "use --acq-code/--sync/--crc for the common frame";
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
  /* The common frame's payload is its data:LEN field, filled frame by
     frame from a data source (doppler#1718). A CARRIED description is the
     whole frame and may be fixed bits throughout -- sent once, never
     cycled -- or name a data:LEN field a data source fills, so it needs only
     a type that carries bits. Without one, the flags' rule applies: the CLI
     asks the same function before it builds a description, so the two cannot
     disagree about when a source can be framed. */
  else if (src->frame)
    {
      if (!type_can_frame (src))
        return WHY_CANNOT_FRAME;
    }
  else
    {
      const char *no = dp_wfm_framing_flags_error (src);
      if (no)
        return no;
    }

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
           "that fills it (`derived_by` = the stage's index plus one); a "
           "derived field must be the last field its stage covers; an "
           "emitting stage must be the only one, must cover the whole frame, "
           "and must not shrink it (its output is at least the frame's bits); "
           "and no field, cover, offset or output length may overflow a "
           "size_t";

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
int
dp_wfm_source_common_frame (const wfm_source_t *src, const wfm_seq_t *sync,
                            int crc, wfm_frame_desc_t *d)
{
  const int spread = (src->type == WFM_SYNTH_DSSS);
  /* The common frame's payload is `data:LEN`, its bits drawn per frame
     from the data source; with none (a dsss burst of preamble and sync
     only) it is empty. */
  const wfm_seq_t dq = { .kind = WFM_SEQ_DATA, .len = common_data_len (src) };
  const wfm_seq_t none = { 0 };
  return dp_wfm_frame_fixed (d, spread ? NULL : &src->acq_code,
                             spread ? 0u : src->acq_reps, sync,
                             has_data (src) ? &dq : &none, crc);
}

static int
source_frame (const wfm_source_t *src, wfm_frame_desc_t *d)
{
  if (src->frame)
    {
      *d = *src->frame;
      return 0;
    }
  return dp_wfm_source_common_frame (src, NULL, 0, d);
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
  if (!type_can_frame (src) && src->type != WFM_SYNTH_DSSS
      && src->type != WFM_SYNTH_FRAMES)
    return "a data source fills a frame's payload: use --type "
           "bits/bpsk/qpsk/pn/dsss/ofdm";
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
static wfm_bitmod_t
frame_modulation (const wfm_source_t *src)
{
  switch (src->type)
    {
    case WFM_SYNTH_QPSK:
      return WFM_BITMOD_QPSK;
    case WFM_SYNTH_BPSK:
    case WFM_SYNTH_PN:
      return WFM_BITMOD_BPSK;
    default:
      return (wfm_bitmod_t)src->modulation;
    }
}

/* Bits per symbol of a mapping: what the synth's `bit_mod` counts, and what
 * a frame's symbol count divides by. Spelled out per mapping (#2647): the
 * enum's wire values happen to equal the counts today (bpsk 1, qpsk 2), but
 * an enum is an ordering, not a count, and a new mapping appended at 3 need
 * not carry 3 bits. 0 is `none`, the synth's 0/1 line (one bit per
 * symbol); -1 is a value no mapping has, which the synth refuses and the
 * symbol count treats as unknown. */
static int
bitmod_bits_per_symbol (wfm_bitmod_t m)
{
  switch (m)
    {
    case WFM_BITMOD_NONE:
      return 0;
    case WFM_BITMOD_BPSK:
      return 1;
    case WFM_BITMOD_QPSK:
      return 2;
    }
  return -1;
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
      return dp_wfm_synth_attach_data (
          syn, &d, &ops, ds, WFM_DATA_UNPACED,
          bitmod_bits_per_symbol (frame_modulation (src)));
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
    rc = dp_wfm_synth_set_bits (
        syn, bits, lay.out_bits,
        bitmod_bits_per_symbol (frame_modulation (src)));
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

/* The pull's state IS its source's: the chunk is scratch, rewritten before
   each frame is assembled from it, and the description, the codes and the
   pacing are config. So the synth nests the source's own blob, and a
   source that refuses (a pipe) refuses the synth's. */
static size_t
data_pull_state_bytes (const void *u)
{
  return dp_wfm_data_state_bytes (((const data_pull_t *)u)->src);
}

static void
data_pull_get_state (const void *u, void *blob)
{
  dp_wfm_data_get_state (((const data_pull_t *)u)->src, blob);
}

static int
data_pull_set_state (void *u, const void *blob)
{
  return dp_wfm_data_set_state (((data_pull_t *)u)->src, blob);
}

static const char *
data_pull_refusal (const void *u)
{
  return dp_wfm_data_state_refusal (((const data_pull_t *)u)->src);
}

static const wfm_synth_refill_state_t data_pull_state
    = { data_pull_state_bytes, data_pull_get_state, data_pull_set_state,
        data_pull_refusal };

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
  (void)dp_wfm_synth_set_refill_state (syn, &data_pull_state);
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
      /* The per-frame code-only window rides on either data path below; its
         order against dp_wfm_synth_set_dsss_cont does not matter. A window
         breaking its rule is refused by dp_wfm_source_error first. */
      if (dp_wfm_synth_set_dsss_window (syn, src->code_only_symbols,
                                        src->frame_symbols)
          != 0)
        return -1;
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

  if (dp_wfm_source_attach_ofdm (eng, src, fs, src->seed) != 0)
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
      float *taps  = dp_mallocn ((size_t)ntaps, sizeof *taps);
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

/* ── the ofdm type: 802.11 PPDUs, one per frame of the data source ────────

   The bridge installs a frame callback (dp_wfm_synth_attach_frames) that
   builds a PPDU through the `wlan` object from the frame's bits, and nests
   the callback's configuration as its state triplet's reject keys. Design:
   docs/design/wlan.md, section 5. The 802.11 numbers arrive from
   wlan_core.h's tables; nothing of the standard is written here. */

/* One octet is eight bits of the frame, the first bit its MOST significant:
   the order every byte-to-bit unpacking here uses (a data file's bytes, a
   hex Field), so a file of PSDU octets, or `--data 0x08...`, is those octets.
   The PHY's own least-significant-first order on the air is wlan's. */

/* The names tables (wfm_names.h) are indexed by these values: the order is
   the contract, held here for the two C enums and in test_wfm_ofdm_bridge.c
   for each name against its value. */
_Static_assert (WLAN_CODE_1_2 == 0 && WLAN_CODE_2_3 == 1 && WLAN_CODE_3_4 == 2
                    && WLAN_CODE_5_6 == 3,
                "OFDM_CODE_RATE_NAMES is indexed by dp_wlan_code_t");
_Static_assert (WLAN_MAC_NONE == 0 && WLAN_MAC_FCS == 1 && WLAN_MAC_DATA == 2,
                "OFDM_MAC_NAMES is indexed by dp_wlan_mac_t");

/* A data frame's longest header: 24 octets, or 30 with the fourth address. */
#define WFOFDM_HDR_MAX 30u

/* The octets of `n_oct * 8` bits, the first bit of each eight the most
   significant. */
static void
wfofdm_pack (const uint8_t *bits, size_t n_oct, uint8_t *out)
{
  for (size_t k = 0; k < n_oct; k++)
    out[k] = (uint8_t)dp_bin_to_int (bits + 8u * k, 8u, DP_BITORDER_BIG);
}

typedef struct
{
  int      constellation; /* wlan's index: 0 BPSK ... 3 64-QAM */
  int      code_rate;     /* wlan's index: 0 1/2 ... 3 5/6 */
  unsigned oversample;    /* M = fs / bandwidth */
  size_t   n_bits;        /* data_len: the frame's bits */
  size_t   psdu_len;      /* octets, the FCS included */
  size_t   window;        /* samples at fs: dp_wfm_source_window's count */
  size_t   n_sym, ppdu_len, n_out;
} wfofdm_geom_t;

/* The rate pair of two indexes, which dp_wlan_n_bpsc has already vouched
   for. */
static dp_wlan_rate_t
wfofdm_rate (int c, int r)
{
  return (dp_wlan_rate_t){ dp_wlan_n_bpsc (c), (dp_wlan_code_t)r };
}

/* The (constellation, code rate) a source names: wlan N expands through the
   standard's Table 17-6; otherwise the two fields are the pair, and an index
   outside the four a face offers (a C or Python source can give one) is
   refused in the field's own words. */
static const char *
wfofdm_pair (const wfm_source_t *src, int *c, int *r)
{
  *c = src->constellation;
  *r = src->code_rate;
  if (!src->wlan)
    {
      if (!dp_wlan_n_bpsc (*c))
        return "constellation is one of bpsk, qpsk, qam16 or qam64";
      if (*r < 0 || *r > 3)
        return "code_rate is one of 1/2, 2/3, 3/4 or 5/6";
      return NULL;
    }
  dp_wlan_rate_t pair;
  if (src->wlan < 0
      || !dp_wlan_rate_from_mbps (dp_wlan_tables_standard (),
                                  (unsigned)src->wlan, &pair))
    return "wlan is the 20 MHz data rate in Mb/s and takes one of 6, 9, 12, "
           "18, 24, 36, 48 or 54";
  for (int k = 0; k < 4; k++)
    if (dp_wlan_n_bpsc (k) == pair.n_bpsc)
      *c = k;
  *r = (int)pair.code;
  return NULL;
}

/* The rules of an ofdm source that need no sample rate. */
static const char *
ofdm_source_error (const wfm_source_t *src)
{
  const int is_ofdm = src->type == WFM_SYNTH_FRAMES;
  if (!is_ofdm)
    {
      if (src->wlan)
        return "wlan is the 802.11 data rate of an ofdm source: give type "
               "ofdm, or drop wlan";
      if (src->constellation || src->code_rate || src->scrambler_init
          || (src->window && src->window != WFM_WINDOW_UNSET) || src->mac
          || src->gap || src->mac_header.len
          || (src->bandwidth != 0.0 && src->bandwidth != 20e6))
        return "constellation, code_rate, scrambler_init, window, mac, "
               "mac_header, gap and bandwidth are fields of an ofdm source: "
               "give type ofdm, or drop them";
      return NULL;
    }
  if (src->wlan && (src->constellation || src->code_rate))
    return "wlan is a spelling of constellation and code_rate: give wlan, "
           "or constellation and code_rate, not both";
  if (src->bandwidth < 0.0)
    return "bandwidth is 20e6, 10e6 or 5e6 Hz for an 802.11a/g/p/j ofdm "
           "source, not negative";
  if (src->wlan && dp_wfm_source_bandwidth (src) != 20e6)
    return "wlan is the data rate at 20 MHz, and a bandwidth other than "
           "20 MHz carries a different rate for the same pair: give "
           "constellation and code_rate with that bandwidth, or drop wlan";
  if (dp_wfm_source_bandwidth (src) != 20e6
      && dp_wfm_source_bandwidth (src) != 10e6
      && dp_wfm_source_bandwidth (src) != 5e6)
    return "bandwidth is 20e6, 10e6 or 5e6 Hz for an 802.11a/g/p/j ofdm "
           "source";
  int         c, r;
  const char *why = wfofdm_pair (src, &c, &r);
  if (why)
    return why;
  const dp_wlan_rate_t pair = wfofdm_rate (c, r);
  unsigned             mbps;
  if (!dp_wlan_mbps (dp_wlan_tables_standard (), &pair, &mbps))
    return "this constellation and code_rate have no SIGNAL RATE bits "
           "(Table 17-6 lists eight of the sixteen pairs)";
  if (src->scrambler_init < 0 || src->scrambler_init > 127)
    return "scrambler_init is the 7-bit initial state: 1 to 127, or 0 to "
           "draw one per PPDU from seed";
  if (src->mac != WLAN_MAC_NONE && src->mac != WLAN_MAC_FCS
      && src->mac != WLAN_MAC_DATA)
    return "mac is one of none, fcs or data";
  if (src->mac_header.kind == WFM_SEQ_DATA && src->mac_header.len)
    return "data:LEN has no bits of its own: it is the frame's field that "
           "--data fills, not a mac_header";
  if (src->mac != WLAN_MAC_DATA && src->mac_header.len)
    return "mac_header is the MAC header of mac data: give mac data, or "
           "drop the mac_header";
  if (src->mac == WLAN_MAC_DATA && !src->mac_header.len)
    return "mac data needs a mac_header: its octets, 24 or 30, as a Field "
           "(a hex Field 0x0802... is the octets it spells)";
  if (src->mac_header.len % 8u)
    return "mac_header is whole octets: its bits must be a multiple of 8";
  if (src->mac == WLAN_MAC_DATA && src->mac_header.len != 8u * 24u
      && src->mac_header.len != 8u * 30u)
    return "mac_header is a data frame's header: 24 octets, or 30 with the "
           "fourth address";
  uint8_t hbits[8u * WFOFDM_HDR_MAX];
  if (src->mac_header.len
      && dp_wfm_seq_bits (&src->mac_header, hbits, src->mac_header.len)
             != src->mac_header.len)
    return "mac_header is a Field that does not build: check its generator";
  if (src->snr_mode == 2 || src->snr_mode == 3)
    return "snr_mode ebno and esno are refused for ofdm: its noise is "
           "referred to the signal power in the full band (auto or fs)";
  if (src->sps > 1 || src->pulse != 0 || src->symbol_rate > 0.0 || src->symbols
      || src->data_code.len || src->acq_code.len || src->frame)
    return "ofdm renders finished samples: sps, pulse, symbols, "
           "symbol_rate, data_code, acq_code and frame describe a "
           "symbol-rate stream or a frame the PHY builds itself";
  if (!has_data (src))
    return "an ofdm source sends a data source's octets as PSDUs: give "
           "--data FIELD or --data-from-file PATH";
  const size_t bits = common_data_len (src);
  if (bits == 0 || bits % 8u)
    return "an ofdm frame is whole octets: data_len must be a multiple of 8";
  if (!dp_wlan_mpdu_len ((dp_wlan_mac_t)src->mac, src->mac_header.len / 8u,
                         bits / 8u))
    return "an ofdm frame's PSDU is 1 to 4095 octets, the FCS included: "
           "data_len is out of range";
  return NULL;
}

/* The rules that need fs, and the geometry they decide. Every size_t field is
   range-checked BEFORE it is narrowed or summed: a wrapped sum allocates a
   frame smaller than the renderer writes. */
static const char *
wfofdm_geom (const wfm_source_t *src, double fs, wfofdm_geom_t *g)
{
  int c, r;
  (void)wfofdm_pair (src, &c, &r); /* ofdm_source_error has accepted it */
  const char    *no = NULL;
  const unsigned m  = dp_wfm_source_oversample (src, fs, &no);
  if (no)
    return no;
  g->constellation = c;
  g->code_rate     = r;
  g->oversample    = m;
  g->window        = dp_wfm_source_window (src, fs); /* M is checked above */
  if (g->window > (size_t)WLAN_SYM_LEN * g->oversample)
    return "window is 0 to 80 times fs over bandwidth samples: the "
           "shortest OFDM symbol";
  g->n_bits   = common_data_len (src);
  g->psdu_len = dp_wlan_mpdu_len ((dp_wlan_mac_t)src->mac,
                                  src->mac_header.len / 8u, g->n_bits / 8u);
  const dp_wlan_rate_t pair = wfofdm_rate (c, r);
  g->n_sym                  = dp_wlan_rate_n_sym (&pair, g->psdu_len);
  /* A PSDU of 1 to 4095 octets is 1 to WLAN_N_SYM_MAX Data symbols at the
     slowest rate (test_wlan_core.c pins both ends), so a PPDU always fits. */
  g->ppdu_len
      = dp_wlan_ppdu_len (g->n_sym, (unsigned)g->window, g->oversample);
  if (g->ppdu_len > WFM_SYNTH_FRAME_MAX
      || src->gap > WFM_SYNTH_FRAME_MAX - g->ppdu_len)
    return "an ofdm frame, its gap included, is at most 2^24 samples";
  g->n_out = g->ppdu_len + src->gap;
  return NULL;
}

const char *
dp_wfm_ofdm_fs_error (const wfm_source_t *src, double fs)
{
  if (src->type != WFM_SYNTH_FRAMES || ofdm_source_error (src))
    return NULL; /* not ofdm, or already refused at its own rules */
  wfofdm_geom_t g;
  return wfofdm_geom (src, fs, &g);
}

/* The renderer: a pure function of this and (frame, bits). */
typedef struct
{
  dp_wlan_state_t *w;
  int              mac;
  uint32_t         scrambler_init; /* 0: drawn per PPDU from seed and frame */
  uint64_t         seed, gap;
  uint32_t         constellation, code_rate, window, oversample;
  uint32_t         hdr_len;             /* octets, 0 unless mac data */
  uint8_t          hdr[WFOFDM_HDR_MAX]; /* the MAC header's octets */
  size_t           n_bits, n_out, ppdu_len;
} wfofdm_render_t;

static int
wfofdm_frame (void *u, uint64_t frame, const uint8_t *bits, size_t n_bits,
              float _Complex *out, size_t n_out)
{
  wfofdm_render_t *r = u;
  uint8_t          psdu[WLAN_PSDU_MAX];
  uint8_t          body[WLAN_PSDU_MAX];
  /* The synth calls with the n_bits and n_out fixed at attach. */
  const size_t n_oct = n_bits / 8u;
  wfofdm_pack (bits, n_oct, body);
  const size_t len = dp_wlan_mpdu ((dp_wlan_mac_t)r->mac, r->hdr, r->hdr_len,
                                   body, n_oct, psdu, sizeof psdu);
  uint32_t     scr = r->scrambler_init;
  if (!scr)
    {
      uint64_t h = dp_hash64 (DP_HASH64_INIT, &r->seed, sizeof r->seed);
      h          = dp_hash64 (h, &frame, sizeof frame);
      scr        = 1u + (uint32_t)(h % 127u);
    }
  const char   *why = NULL;
  const int64_t got
      = dp_wlan_ppdu_why (r->w, psdu, len, scr, out, r->ppdu_len, &why);
  memset (out + r->ppdu_len, 0, (n_out - r->ppdu_len) * sizeof *out);
  /* The geometry was validated at attach, so a PPDU is rendered; a refusal
     is the synth's to report. */
  return got != (int64_t)r->ppdu_len;
}

static void
wfofdm_free (void *u)
{
  wfofdm_render_t *r = u;
  dp_wlan_destroy (r->w);
  free (r);
}

/* The renderer's configuration, as bytes: the reject keys. Only what no other
   key already decides: the frame's bit count and length are the synth's own
   keys, and the gap is n_out less the PPDU's length, which the keys below
   (rate, window, M, mac) and the bit count fix. Each key here is proved
   alone by test_wfm_ofdm_bridge.c, at an equal n_out. */
#define WFOF_STATE_MAGIC DP_FOURCC ('W', 'F', 'O', 'F')
#define WFOF_STATE_VERSION 1u
#define WFOF_STATE_BYTES                                                      \
  (sizeof (dp_state_hdr_t) + 7u * 4u + 8u + WFOFDM_HDR_MAX)

static size_t
wfofdm_state_bytes (const void *u)
{
  (void)u;
  return WFOF_STATE_BYTES;
}

static void
wfofdm_get_state (const void *u, void *blob)
{
  const wfofdm_render_t *r = u;
  DP_GET_OPEN (WFOF_STATE_MAGIC, WFOF_STATE_VERSION, WFOF_STATE_BYTES);
  dp_w_u32 (&_w, r->constellation);
  dp_w_u32 (&_w, r->code_rate);
  dp_w_u32 (&_w, r->window);
  dp_w_u32 (&_w, r->oversample);
  dp_w_u32 (&_w, (uint32_t)r->mac);
  dp_w_u32 (&_w, r->scrambler_init);
  dp_w_u32 (&_w, r->hdr_len);
  dp_w_u64 (&_w, r->seed);
  dp_w_bytes (&_w, r->hdr, WFOFDM_HDR_MAX);
}

/* The synth compares get_state's bytes and never calls a renderer's
   set_state, so the triplet has none. */
static const wfm_synth_refill_state_t wfofdm_state_ops
    = { wfofdm_state_bytes, wfofdm_get_state, NULL, NULL };

int
dp_wfm_source_attach_ofdm (dp_wfm_synth_state_t *syn, const wfm_source_t *src,
                           double fs, uint32_t seed)
{
  if (src->type != WFM_SYNTH_FRAMES)
    return 0;
  /* No data source: nothing to render, as set_bits is a no-op without a
     pattern. The Plan's noise-only copy of a noisy source is such a source
     (its data is the composer's, dropped), and it only draws the noise. A
     source with data must pass the validator, which refuses none. */
  if (!has_data (src))
    return 0;
  wfofdm_geom_t g;
  if (ofdm_source_error (src) || wfofdm_geom (src, fs, &g))
    return -1;
  wfofdm_render_t *r = dp_xcalloc (1, sizeof *r);
  /* The arguments were validated by ofdm_source_error and wfofdm_geom, so
     the constructor cannot refuse them. */
  r->w   = dp_wlan_create (g.constellation, g.code_rate, (uint32_t)g.window,
                           g.oversample);
  r->mac = src->mac;
  r->hdr_len = (uint32_t)(src->mac_header.len / 8u);
  if (r->hdr_len)
    {
      /* a Field of any kind (literal, PN, Gold, dotted) becomes its bits;
         ofdm_source_error has built it once, so it builds */
      uint8_t hbits[8u * WFOFDM_HDR_MAX];
      (void)dp_wfm_seq_bits (&src->mac_header, hbits, src->mac_header.len);
      wfofdm_pack (hbits, r->hdr_len, r->hdr);
    }
  r->scrambler_init = (uint32_t)src->scrambler_init;
  r->seed           = seed;
  r->gap            = src->gap;
  r->constellation  = (uint32_t)g.constellation;
  r->code_rate      = (uint32_t)g.code_rate;
  r->window         = (uint32_t)g.window;
  r->oversample     = g.oversample;
  r->n_bits         = g.n_bits;
  r->n_out          = g.n_out;
  r->ppdu_len       = g.ppdu_len;
  if (dp_wfm_synth_attach_frames (syn, wfofdm_frame, r, wfofdm_free, g.n_bits,
                                  g.n_out)
      != 0)
    {
      wfofdm_free (r);
      return -1;
    }
  /* From here the synth owns the renderer. The pull is the data source's
     frames, one per PPDU, copied as drawn (no description to assemble). */
  wfm_data_src_t *ds = dp_wfm_data_create_seq (
      src->data_from_file ? NULL : &src->data, src->data_from_file, g.n_bits,
      &src->fill, NULL);
  if (!ds)
    return -1;
  data_pull_t *p = dp_xcalloc (1, sizeof *p);
  p->src         = ds;
  p->pacing      = WFM_DATA_UNPACED;
  p->reps        = 1u;
  p->chunk_bits  = g.n_bits;
  p->chunk       = dp_xcalloc (g.n_bits, 1);
  p->raw         = 1;
  /* A frames synth with its renderer installed takes a refill. */
  (void)dp_wfm_synth_set_refill (syn, data_pull_refill, p, data_pull_free);
  (void)dp_wfm_synth_set_refill_state (syn, &data_pull_state);
  return dp_wfm_synth_attach_frames_state (syn, &wfofdm_state_ops);
}

int
dp_wfm_source_data_is_stream (const wfm_source_t *src)
{
  return src && src->data_from_file && strcmp (src->data_from_file, "-") == 0;
}

/* The symbol after the frames-th data symbol of a continuous dsss stream.
   Without a window every symbol carries data. With one, the data pauses
   through each frame's code_only_symbols, so data symbol k (0-based) is
   symbol (k / d) * frame_symbols + code_only_symbols + k % d, with
   d = frame_symbols - code_only_symbols data symbols a frame. A trailing
   window after the last data symbol is not data, so the run stops before
   it, as it stops before the next data symbol without a window. */
static uint64_t
data_symbol_end (const wfm_source_t *src, uint64_t frames)
{
  const uint64_t f = src->frame_symbols, w = src->code_only_symbols;
  if (!frames || !f || w >= f) /* w >= f with data is refused upstream */
    return frames;
  const uint64_t d = f - w, k = frames - 1u;
  return (k / d) * f + w + k % d + 1u;
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
      return dp_wfm_dsss_cont_edge (data_symbol_end (src, frames), cps) * sps;
    }
  if (src->type == WFM_SYNTH_FRAMES)
    {
      /* A frame is one PPDU and its gap, n_out samples at the output rate. */
      wfofdm_geom_t g;
      return ofdm_source_error (src) || wfofdm_geom (src, fs, &g)
                 ? 0
                 : frames * g.n_out;
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
  const int b = bitmod_bits_per_symbol (frame_modulation (src));
  if (b < 0)
    return 0; /* a mapping with no bit count: the length is unknown */
  const uint64_t bps = b > 0 ? (uint64_t)b : 1u;
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
  if (src->type == WFM_SYNTH_FRAMES)
    {
      const uint64_t len = common_data_len (src);
      return len ? (total + len - 1u) / len : 0;
    }
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
