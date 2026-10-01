/*
 * wfmgen.c — the waveform-generator composer CLI (Phase C, hand-written).
 *
 * It sequences multi-segment specs (`--from-file`), emits any output file type
 * (raw/csv/BLUE/SigMF, `--file-type`) in any wire type / byte order, streams
 * to a file, stdout, or a NATS PUB subject (`--output nats://…`), and writes a
 * JSON record of exactly what it produced (`--record`). All of it is thin glue
 * over the C cores in the wfmcompose c_dep — wfm_compose / wfm_writer /
 * wfm_sink — which is why this lives by hand rather than via `jm app` (a
 * composer is not a single-object generator).
 *
 * Single-segment mode (the default) builds a one-segment spec from the
 * flags, so `wfmgen --type qpsk --count 4096 …` needs no spec file.
 */
#include "doppler/cvt/cvt_core.h"
#include "doppler/dp_complex.h"
#include <limits.h> /* INT_MAX -- an integer flag is bounded by its field */
#include <math.h>
#include <signal.h>
#include <stddef.h> /* offsetof — the option table names fields by offset */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <fcntl.h> /* _O_BINARY */
#include <io.h>    /* isatty, fileno (the UCRT's POSIX names), _setmode */
#else
#include <unistd.h> /* isatty */
#endif

#include "doppler/dp_interrupt.h"
#include "doppler/timing/timing_core.h"
#include "doppler/version.h" /* DOPPLER_VERSION (configure-time stamp) */
#include "doppler/wfm/wfm_compose.h"
#include "doppler/wfm/wfm_defaults.h" /* WFM_SOURCE/SEGMENT_DEFAULTS */
#include "doppler/wfm/wfm_names.h" /* every choice table -- the one C home (#760) */
#include "doppler/wfm/wfm_sink.h"
#include "doppler/wfm/wfm_surface.h" /* the field flags, generated */
#include "doppler/wfm/wfmgen.h"
#include "doppler/wfm_writer/wfm_writer_core.h"

#define BLK 4096

/* Every --flag choice table below is the `*_NAMES` array from
   wfm/wfm_names.h, the one C home for them (doppler#760). Twelve were
   declared here instead until then -- two of them, TYPE_NAMES and
   MODE_NAMES, verbatim copies of tables that header already carried -- and
   list order IS the C enum value, so a copy that drifted mapped a flag to
   the wrong waveform rather than failing. `make lint-wfm-enum-tables` now
   holds the header to the manifest's [[enum]] blocks. */

/* Look name up in a NULL-free table of n entries; -1 if absent. */
static int
lookup (const char *s, const char *const *tbl, int n)
{
  for (int i = 0; i < n; i++)
    if (!strcmp (s, tbl[i]))
      return i;
  return -1;
}

/* A real number that is EXACTLY the text [s, stop): strtod must consume all
 * of it. strtod alone stops at the first character it cannot use and skips
 * leading space, so `0.1abc` read as 0.1 and an empty token as 0, exit 0
 * (doppler#1611). Returns 0, or -1 for text that is not wholly a number. */
static int
read_double (const char *s, const char *stop, double *d)
{
  if (s == stop || *s == ' ' || *s == '\t' || *s == '\n')
    return -1;
  char        *end;
  const double x = strtod (s, &end);
  if (end != stop)
    return -1;
  *d = x;
  return 0;
}

/* An integer flag value, read by the Field grammar's own reader
 * (dp_wfm_parse_u64) so a flag and a Field read one number one way:
 * decimal, or hex after `0x`, consumed whole; a leading 0 is decimal, never
 * octal. `--seed 0x10` used to record 0 and `--pn-poly 0x6000` to select
 * auto (doppler#1611). Refused past `max`, which is the destination's range.
 * Returns 0, or 2 (the usage-error exit) after saying why. */
static int
flag_uint (const char *a, const char *v, uint64_t max, uint64_t *out)
{
  uint64_t x;
  if (dp_wfm_parse_u64 (v, strlen (v), &x) != 0 || x > max)
    {
      (void)fprintf (stderr,
                     "error: %s takes a whole number, decimal or 0x hex, "
                     "from 0 to %llu -- not '%s'\n",
                     a, (unsigned long long)max, v);
      return 2;
    }
  *out = x;
  return 0;
}

/* Non-zero when @p d is a count a size_t can hold: whole, non-negative and
 * in range. On LP64 SIZE_MAX rounds UP to 2^64 as a double, so the strict
 * `< 2^64` is the bound there; `<= SIZE_MAX` is the bound where size_t is
 * 32 bits and exact. NaN fails `>= 0`. */
static int
whole_count (double d)
{
  return d >= 0.0 && d < 18446744073709551616.0 && d == floor (d)
         && d <= (double)SIZE_MAX;
}

/* Parse a numeric flag value as a scalar (`12000`) or a uniform range
 * (`9000:14000`) into *lo; on a range it also sets *hi and *ranged so the
 * composer redraws the field each repeat. A bare scalar leaves *ranged 0.
 * Each side must be wholly a number: a trailing character, an empty side
 * (`12000:`) or a leading space is refused. Returns 0, or 2 after saying
 * why. */
static int
parse_range (const char *a, const char *v, double *lo, double *hi, int *ranged)
{
  const char *colon = strchr (v, ':');
  const char *end   = v + strlen (v);
  double      l, h = 0.0;
  if (read_double (v, colon ? colon : end, &l) != 0
      || (colon && read_double (colon + 1, end, &h) != 0))
    {
      (void)fprintf (stderr,
                     "error: %s takes a number, or a LO:HI range "
                     "-- not '%s'\n",
                     a, v);
      return 2;
    }
  *lo     = l;
  *ranged = colon != NULL;
  if (colon)
    *hi = h;
  return 0;
}

/* Warn (and optionally fail) when an integer wire type clipped. peak > 1 means
 * the composite ran past full-scale; report the overshoot in dB (the headroom
 * it would need) and how to capture it losslessly. Float types never clip.
 * Shared by the writer and sink paths. Returns non-zero when --clip-error
 * should fail the run. */
static int
report_clip (double peak, double frac, int stype, double headroom,
             int clip_report, int clip_error)
{
  double dbfs = peak > 0.0 ? 20.0 * log10 (peak) : -120.0;
  if (stype < 2 || peak <= 1.0)
    {
      if (clip_report)
        (void)fprintf (stderr, "wfmgen: peak %.1f dBFS — no clipping\n", dbfs);
      return 0;
    }
  /* peak is *after* any --headroom; total backoff to fit it = current + over.
   */
  int need = (int)ceil (headroom + dbfs);
  (void)fprintf (
      stderr,
      "wfmgen: warning: %s output clipped — peak is +%.1f dB over full "
      "scale.\n  remedy: --headroom %d, or --sample-type cf32.\n",
      STYPE_NAMES[stype], dbfs, need);
  if (clip_report)
    (void)fprintf (stderr, "  clipped %.2f%% of I/Q components\n",
                   100.0 * frac);
  return clip_error ? 1 : 0;
}

/* Read a whole file into a malloc'd NUL-terminated string (caller frees). */
static char *
slurp_file (const char *path)
{
  FILE *f = fopen (path, "rb");
  if (!f)
    return NULL;
  if (fseek (f, 0, SEEK_END) != 0)
    {
      (void)fclose (f);
      return NULL;
    }
  long len = ftell (f);
  if (len < 0 || fseek (f, 0, SEEK_SET) != 0)
    {
      (void)fclose (f);
      return NULL;
    }
  char *buf = malloc ((size_t)len + 1);
  if (!buf)
    {
      (void)fclose (f);
      return NULL;
    }
  size_t rd = fread (buf, 1, (size_t)len, f);
  (void)fclose (f);
  /* fread returns at most len, and buf is len+1 bytes, so rd is in bounds. */
  /* NOLINTNEXTLINE(clang-analyzer-security.ArrayBound) */
  buf[rd] = '\0';
  return buf;
}

/* Read a raw interleaved-I/Q cf32 file (float32 re, im, …) into a malloc'd
   complex array. Sets *n to the symbol count and returns the buffer, or NULL
   on read error or a size that is not a whole number of cf32 samples. */
static float _Complex *
read_cf32_file (const char *path, size_t *n)
{
  FILE *f = fopen (path, "rb");
  if (!f)
    return NULL;
  if (fseek (f, 0, SEEK_END) != 0)
    {
      (void)fclose (f);
      return NULL;
    }
  long len = ftell (f);
  if (len <= 0 || (size_t)len % sizeof (float _Complex) != 0
      || fseek (f, 0, SEEK_SET) != 0)
    {
      (void)fclose (f);
      return NULL;
    }
  float _Complex *buf = malloc ((size_t)len);
  if (!buf)
    {
      (void)fclose (f);
      return NULL;
    }
  size_t rd = fread (buf, 1, (size_t)len, f);
  (void)fclose (f);
  if (rd != (size_t)len)
    {
      free (buf);
      return NULL;
    }
  *n = (size_t)len / sizeof (float _Complex);
  return buf;
}

/* Build "<base><suffix>" into dst[n]. Returns 0, or -1 if it would truncate
   (the output path is too long) — the caller reports a usage error rather than
   silently writing a truncated, wrong path. */
static int
build_path (char *dst, size_t n, const char *base, const char *suffix)
{
  int len = snprintf (dst, n, "%s%s", base, suffix);
  return (len < 0 || (size_t)len >= n) ? -1 : 0;
}

static const char USAGE[]
    = "wfmgen - doppler waveform generator\n"
      "\n"
      "USAGE\n"
      "  wfmgen [OPTIONS] [--output FILE|-|nats://HOST:PORT/SUBJECT]\n"
      "  wfmgen json-template [FILE]\n"
      "\n"
      "WAVEFORM TYPE\n" WFM_SURFACE_HELP_TYPE
      "                    tone  - pure CW carrier at --freq\n"
      "                    noise - Gaussian white noise\n"
      "                    pn    - pseudo-random MLS sequence\n"
      "                    bpsk  - BPSK-modulated symbols\n"
      "                    qpsk  - QPSK-modulated symbols\n"
      "                    chirp - linear sweep, --freq to --f-end\n"
      "                    bits  - custom bit pattern (see BITS INPUT)\n"
      "                    symbols - custom complex constellation (see "
      "SYMBOLS INPUT)\n"
      "                    dsss  - two-code DSSS burst (see DSSS BURST)\n"
      "\n"
      "SIGNAL PARAMETERS\n"
      "  (LO:HI on --freq/--f-end/--snr/--level/--count/--off/--doppler/\n"
      "   --doppler-rate draws that field\n"
      "   uniformly each repeat — e.g. --freq 9000:14000 — reproducible per"
      " seed)\n" WFM_SURFACE_HELP_SIGNAL
      "  --fc HZ         Centre frequency stored in SigMF metadata only\n"
      "\n"
      "NOISE / SNR\n" WFM_SURFACE_HELP_NOISE
      "                    auto  - Es/No for PSK; full-band for tone/noise\n"
      "                    fs    - relative to full sample-rate band\n"
      "                    ebno  - Eb/No (energy per bit / noise density)\n"
      "                    esno  - Es/No (energy per symbol / noise"
      " density)\n"
      "\n"
      "PULSE SHAPING\n" WFM_SURFACE_HELP_PULSE "\n"
      "DATA  (--type bits | bpsk | qpsk | pn | dsss)\n" WFM_SURFACE_HELP_BITS
      "\n"
      "SYMBOLS INPUT  (--type symbols)\n" WFM_SURFACE_HELP_SYMBOLS
      "                  F is a raw cf32 file (interleaved float32 I,Q),\n"
      "                  one constellation point per sample.\n"
      "\n"
      "FRAMING  (--type bits | bpsk | qpsk | pn, and --type dsss below)\n"
      "  A payload is a DATA SOURCE, sent as a sequence of frames:\n"
      "      [preamble x REPS | sync | data:LEN | CRC-16]\n"
      "  each carrying the next --data-len bits of --data or\n"
      "  --data-from-file until the source ends, which ends the run.\n"
      "  For --type bits --modulation maps the frames to BPSK or QPSK; the\n"
      "  PN-sourced types map them as their names say. Types with no bit\n"
      "  stream (tone, noise, chirp, symbols) cannot be framed.\n"
      "\n"
      "CODED OR CUSTOM FRAMES  (--type bits | bpsk | qpsk | pn | "
      "dsss)\n" WFM_SURFACE_HELP_CODED
      "  The only way to add a coding stage; a CCSDS CADU is one such file.\n"
      "  It is the whole frame, so --sync, --crc and an unspread --acq-code\n"
      "  are refused beside it too. FILE holds a scene's \"frame\" object,\n"
      "  and --record stores it there. On --type dsss it is the SPREAD\n"
      "  frame: --acq-code stays the unspread preamble.\n"
      "\n"
      "DSSS BURST  (--type dsss)\n"
      "  One burst per frame of the data source: an unmodulated repeated\n"
      "  preamble (code A), then the frame [sync | data:LEN | CRC-16],\n"
      "  every frame bit spread by a second code B. --sps is samples per\n"
      "  CHIP; the run is derived (n_chips * sps samples a burst, one burst\n"
      "  with no data source) and --count ignored; --snr-mode esno is the\n"
      "  Es/N0 of the outer DATA symbol (code-B chips x sps "
      "samples).\n" WFM_SURFACE_HELP_DSSS_BURST "\n"
      "FIELDS  (--acq-code / --sync / --data-code / --data / --fill)\n"
      "  Each takes ONE Field -- literal bits or a generated sequence:\n"
      "    10110010                         literal bits (0 and 1 only)\n"
      "    0x1ACFFC1D                       literal hex, 4 bits a digit\n"
      "    pn:LEN:REG[:SEED[:POLY]][:fibonacci]   an LFSR sequence\n"
      "    gold:LEN:REG:TA:SA:TB:SB         a Gold code from two registers\n"
      "    dotted:LEN                       alternating 1010...\n"
      "  LEN is the output length, REG the register width (1..64); numbers\n"
      "  take decimal or 0x hex. Add *REPS to repeat a preamble:\n"
      "  --acq-code 'pn:31:5*4' (quote the *). --record stores the Field,\n"
      "  so a 1023-chip code is six numbers, not 1023 characters.\n"
      "\n"
      "DSSS CONTINUOUS  (--type dsss --symbol-rate HZ)\n"
      "  An endless stream: code B repeats forever and data rides it at\n"
      "  --symbol-rate Hz, independent of the chip clock (non-integer\n"
      "  chips/symbol -- the asynchronicity). No preamble/sync/CRC frame;\n"
      "  --count is honoured verbatim; --snr-mode esno is the Es/N0 of the\n"
      "  data symbol (fs/symbol_rate samples). Data source: default PRBS\n"
      "  (seeded PN a receiver regenerates), --code-only for the pure\n"
      "  code, or --data / --data-from-file, one bit per data symbol (no\n"
      "  frame, so --data-len and --fill are refused). Rejects the\n"
      "  burst-frame flags (--acq-code/--sync/--crc/--frame) and --code-only\n"
      "  with a data source. --data-code (above) is "
      "required.\n" WFM_SURFACE_HELP_DSSS_CONT "\n"
      "CLOCK DOPPLER\n"
      "  Rescales the whole received time base, so the symbol and chip rates\n"
      "  move with the carrier -- what a real pass does, and what --freq\n"
      "  cannot express (an offset moves the carrier "
      "alone).\n" WFM_SURFACE_HELP_DOPPLER "\n"
      "PN SEQUENCE  (--type pn)\n" WFM_SURFACE_HELP_PN "\n"
      "AMPLITUDE & CLIPPING\n" WFM_SURFACE_HELP_AMPLITUDE
      "  --headroom DB   Back off composite to prevent clipping (default 0)\n"
      "  --clip-report   Print clipping fraction and peak to stderr\n"
      "  --clip-error    Exit non-zero if output clips after headroom\n"
      "\n"
      "OUTPUT\n"
      "  --output DEST   File path, - for stdout, or nats://HOST:PORT/SUBJECT"
      " (default -)\n"
      "  --sample-type T Complex, two components per sample:\n"
      "                    cf32 | cf64 | ci32 | ci16 | ci8 (default cf32)\n"
      "                  Real, one component -- Q is DROPPED, not summed:\n"
      "                    f32 | f64 | i32 | i16 | i8\n"
      "                  A real type halves the file and writes BLUE format\n"
      "                  mode 'S' / SigMF `rf32_le`. Right for a waveform\n"
      "                  that IS real; on a complex baseband it discards\n"
      "                  half the information and mirrors the spectrum.\n"
      "  --file-type T   raw | csv | blue | sigmf (default raw)\n"
      "  --endian E      le | be (default le)\n"
      "  --detached      BLUE detached header: the HCB to <out>.hdr and the\n"
      "                  data to <out>.det, instead of one file. Needs\n"
      "                  --file-type blue, --output, and a finite run.\n"
      "  --record FILE   Write a JSON record of the resolved run to FILE\n"
      "\n"
      "COMPOSITION\n"
      "  --from-file F   Load a multi-segment JSON scene (overrides signal"
      " flags)\n"
      "  --repeat        Loop the spec indefinitely\n"
      "  --continuous    Stream continuously (no defined end)\n"
      "  --seed-advance A  none | noise | all (default none): how the seed "
      "advances per repeat — none = byte-identical; noise = fresh noise each "
      "loop, signal fixed; all = code+data+noise all change\n"
      "\n"
      "REAL-TIME\n"
      "  --realtime      Pace output to wall-clock sample rate\n"
      "  --realtime-resync  Re-anchor the clock when output falls behind\n"
      "                  (absorb an underrun rather than catch up)\n"
      "\n"
      "SUBCOMMANDS\n"
      "  wfmgen json-template [FILE]\n"
      "    Dump an editable JSON spec skeleton; pass back with --from-file.\n"
      "    Default output: stdout.\n"
      "\n"
      "HELP\n"
      "  -h, --help      Print this help and exit\n"
      "  -V, --version   Print the doppler version and exit\n"
      "\n"
      "EXAMPLES\n"
      "  # 1000-sample CW tone at 0.1 Fs, written as cf32\n"
      "  wfmgen --type tone --freq 0.1 --count 1000 --output tone.cf32\n"
      "\n"
      "  # BPSK burst, 4 sps, RRC pulse shaping, Eb/No 10 dB\n"
      "  wfmgen --type bpsk --sps 4 --pulse rrc --rrc-beta 0.35 \\\n"
      "         --snr 10 --snr-mode ebno --count 16384"
      " --output burst.cf32\n"
      "\n"
      "  # QPSK stream to NATS, real-time paced at 2 MHz\n"
      "  wfmgen --type qpsk --sps 8 --fs 2e6 \\\n"
      "         --output nats://127.0.0.1:4222/iq --continuous --realtime\n"
      "\n"
      "  # Multi-segment scene from a JSON spec\n"
      "  wfmgen json-template scene.json  # generate skeleton\n"
      "  wfmgen --from-file scene.json --output scene.cf32\n";

/* The five heap fields a parsed source can own. The composer deep-copies
 * everything it is handed, so these are always the CLI's own copies and are
 * always ours to free — on the success path and, via `done:`, on every early
 * exit. Nulled after freeing so a double call is harmless.
 *
 * It exists because the frees were previously written out once, at the end of
 * the success path only: 28 early returns walked past them, which is the five
 * clang-analyzer unix.Malloc findings this file carried. */
static void
source_free (wfm_source_t *s)
{
  free (s->symbols);
  free ((void *)s->acq_code.bits);
  free ((void *)s->data_code.bits);
  free ((void *)s->sync.bits);
  free ((void *)s->data.bits);
  free ((void *)s->fill.bits);
  s->data.bits = NULL;
  s->fill.bits = NULL;
  /* Nulled individually, not chained: `symbols` is float _Complex * while the
     rest are uint8_t *, so a chain would be an incompatible assignment. */
  s->symbols        = NULL;
  s->acq_code.bits  = NULL;
  s->data_code.bits = NULL;
  s->sync.bits      = NULL;
  /* A --frame description is the CLI's own, read from its file. */
  dp_wfm_frame_free ((wfm_frame_desc_t *)s->frame);
  s->frame = NULL;
}

/* ── The option table ────────────────────────────────────────────────────
 *
 * Every flag is a ROW OF DATA — its spelling, how its value is parsed, and
 * which field it lands in — so `parse_args` below is one lookup and one
 * switch over the value KINDS, not one `else if` arm per flag.
 *
 * That is the whole point of the shape (gh-723). The arm-per-flag chain it
 * replaces carried 89 branches and grew by one arm, one `strcmp` and one
 * more path through the same five error idioms with every option added; a
 * row adds a line of data and no control flow at all. The old chain's own
 * justification conceded exactly this: a dispatcher's LENGTH is inherent to
 * a wide surface, but its BRANCH COUNT is not.
 *
 * Everything the parser writes lives in one object so that a row can name
 * its destination as a byte offset rather than a pointer to a local — which
 * is what lets the table be static, file-scope, const data.
 */
typedef struct
{
  /* Offset 0, deliberately unused, and deliberately FIRST.
   *
   * A row that names no companion field leaves `aux`/`seen` zero, because
   * that is what a designated initialiser writes into the members a row
   * omits. Reserving offset 0 is what lets a plain, unbiased offset say
   * "absent": `seen == 0` cannot be confused with a real field, and `aux`
   * resolves to a harmless slot rather than to NULL — so the companion
   * store needs no null test, and there is no impossible-error branch
   * standing in for one. */
  union
  {
    size_t n;
    double d;
    int    i;
  } discard;
  wfm_source_t  src;
  wfm_segment_t seg;
  double        headroom; /* dB of peak backoff; gain = 10^(-H/20) */
  double        fc;       /* centre frequency, SigMF metadata only */
  const char   *from_file;
  const char   *out_path;
  const char   *record_path;
  int           repeat, continuous, detached;
  int           seed_advance; /* wfm_seed_advance_t: none/noise/all */
  int           realtime, realtime_resync;
  int           clip_report, clip_error;
  int           headroom_set; /* explicit --headroom overrides a record */
  int           sample_type, file_type, endian;
  /* Which surface rows were given, indexed WFM_SURFACE_<owner>_<name>.
     Presence matters where a value's default is not "absent": --crc
     defaults to crc16, so giving it is what frames a waveform, and a given
     --symbol-rate is refused at <= 0 where the default 0 means burst. */
  int surf_seen[WFM_SURFACE_N];
  /* A BESPOKE row's raw value (--frame FILE), read by this face's own code
     rather than the generic parse switch. */
  const char *surf_text[WFM_SURFACE_N];
} wfmgen_opts_t;

/* --frame FILE: the source's frame row, a bespoke one. */
#define FRAME_PATH(o) ((o)->surf_text[WFM_SURFACE_source_frame])

/* How a flag's value is read. The enum type is used for `opt_t.kind` (rather
   than a plain int) so -Wswitch reports a kind added here with no arm in
   parse_args, instead of it silently falling through as a no-op. */
enum opt_kind
{
  OPT_SET,     /* takes no value; the destination int becomes 1        */
  OPT_STR,     /* the raw token, stored verbatim (NULL is tolerated)   */
  OPT_CHOICE,  /* one name from `tbl`; the destination int gets its index */
  OPT_DOUBLE,  /* strtod                                               */
  OPT_INT,     /* strtol                                               */
  OPT_SIZE,    /* strtoull -> size_t                                   */
  OPT_U32,     /* strtoul  -> uint32_t                                 */
  OPT_U64,     /* strtoull -> uint64_t                                 */
  OPT_RANGE_D, /* LO[:HI] -> double at off, hi at aux, bit in src.ranged */
  OPT_RANGE_N, /* LO[:HI] -> size_t at off, hi at aux, bit in seg.ranged */
  OPT_SYMBOLS, /* a raw cf32 file -> float _Complex * at off           */
  OPT_FIELD,   /* a Field (wfm_frame.h) -> the wfm_seq_t at off; its
                  *REPS to the size_t at aux, or refused when the row
                  has no repetition count                             */
};

/* One flag.
 *   aux  — the row's second data field: a `*_hi` bound, or an array length.
 *          Omitted rows resolve to the `discard` slot (see wfmgen_opts_t).
 *   seen — an int set to 1 alongside, for a flag whose PRESENCE also matters
 *          (--headroom overriding a recorded one, --realtime-resync implying
 *          --realtime, --data needing --symbol-rate to be meaningful).
 */
typedef struct
{
  const char        *name;
  const char        *alias;
  enum opt_kind      kind;
  int                unit_interval; /* OPT_DOUBLE: require 0 < v <= 1 */
  unsigned           range_bit;     /* OPT_RANGE_*: the WFM_RANGE_* bit */
  int                field_reps;    /* OPT_FIELD: aux takes *REPS       */
  size_t             off;
  size_t             aux;
  size_t             seen;
  const char *const *tbl; /* OPT_CHOICE: the accepted names */
  int                ntbl;
} opt_t;

#define OFF(f) offsetof (wfmgen_opts_t, f)
#define AUX(f) offsetof (wfmgen_opts_t, f)
#define SEEN(f) offsetof (wfmgen_opts_t, f)
#define CHOICES(t) .tbl = (t), .ntbl = (int)(sizeof (t) / sizeof (*(t)))

/* Rows are in the order the old else-if chain matched them, so the two can
   be read side by side. Order is not otherwise significant — every lookup
   scans the whole table. */
/* Parse a GENERATED sequence: `KIND:LEN[:...]`, colon-separated like the
 * `LO[:HI]` ranges above.
 *
 *     pn:LEN:REG_BITS[:SEED[:POLY]]
 *     gold:LEN:REG_BITS:TAPS_A:SEED_A:TAPS_B:SEED_B
 *     dotted:LEN
 *
 * ONE flag per sequence with the kind as DATA, rather than one flag per
 * (sequence, kind) pair. Three kinds across three sequences would be nine
 * flags for a surface this campaign exists to shrink -- and the spelling
 * here is then the same one the record uses (`sync_gen: {kind: "pn", ...}`),
 * so the CLI, the JSON and the schema share one vocabulary instead of three.
 *
 * Every number goes through strtoull base 0, so a tap mask may be written
 * `0x409` as it is in the record and in the literature, or in decimal.
 * Returns 0, or -1 having already said what was wrong. */
static const opt_t OPTS[] = {
  { .name = "--from-file", .kind = OPT_STR, .off = OFF (from_file) },
  { .name = "--sample-type",
    .kind = OPT_CHOICE,
    .off  = OFF (sample_type),
    CHOICES (STYPE_NAMES) },
  { .name = "--file-type",
    .kind = OPT_CHOICE,
    .off  = OFF (file_type),
    CHOICES (FTYPE_NAMES) },
  { .name = "--endian",
    .kind = OPT_CHOICE,
    .off  = OFF (endian),
    CHOICES (ENDIAN_NAMES) },
  { .name = "--fc", .kind = OPT_DOUBLE, .off = OFF (fc) },
  { .name = "--repeat", .kind = OPT_SET, .off = OFF (repeat) },
  { .name = "--continuous", .kind = OPT_SET, .off = OFF (continuous) },
  { .name = "--seed-advance",
    .kind = OPT_CHOICE,
    .off  = OFF (seed_advance),
    CHOICES (SEED_ADVANCE_NAMES) },
  { .name = "--detached", .kind = OPT_SET, .off = OFF (detached) },
  { .name = "--realtime", .kind = OPT_SET, .off = OFF (realtime) },
  { .name = "--headroom",
    .kind = OPT_DOUBLE,
    .off  = OFF (headroom),
    .seen = SEEN (headroom_set) },
  { .name = "--clip-report", .kind = OPT_SET, .off = OFF (clip_report) },
  { .name = "--clip-error", .kind = OPT_SET, .off = OFF (clip_error) },
  { .name = "--realtime-resync",
    .kind = OPT_SET,
    .off  = OFF (realtime_resync),
    .seen = SEEN (realtime) },
  { .name  = "--output",
    .alias = "-o",
    .kind  = OPT_STR,
    .off   = OFF (out_path) },
  { .name = "--record", .kind = OPT_STR, .off = OFF (record_path) },
};

/* Find the row matching one argv token, by long name or alias, and copy it
   into `out`; 0 if the token is not a flag this CLI accepts.

   Two tables. OPTS above holds the flags that are wfmgen's own; every flag
   that sets a source or segment FIELD is a row of the generated surface
   table (wfm/wfm_surface.h), and is turned into the same opt_t here, so one
   parse switch reads both. The row's offsets are into its own struct, so
   this adds where that struct sits in wfmgen_opts_t. */
/* Flags that USED to exist, each refused with what replaced it -- never
 * aliased (docs/design/frame-description.md F.3). A Field flag took over
 * every spelling of its field: the -hex and -gen forms, the separate
 * repetition count, and the payload's bound, which is exactly the PN
 * sequence `--data pn:N:REG[:SEED[:POLY]]` names. */
typedef struct
{
  const char *flag, *instead;
  const char *why; /* NULL: the Field took over this spelling */
} retired_t;

static const retired_t RETIRED[] = {
/* The payload is a data source (doppler#1718): --data takes every Field
   the payload's flags took, and --data-from-file a file. */
#define DATA_SOURCE "a payload is drawn from a data source"
  { "--bits", "--data <FIELD> --data-len <BITS>", DATA_SOURCE },
  { "--bits-file", "--data-from-file <PATH> (- for stdin)", DATA_SOURCE },
#undef DATA_SOURCE
  { "--bits-hex", "--data 0x<HEX>" },
  { "--payload-gen", "--data <FIELD>, e.g. --data pn:1024:10" },
  { "--payload-len",
    "--data pn:<N>:<pn-length>[:<seed>[:<poly>]] -- the same bits" },
  { "--acq-code-hex", "--acq-code 0x<HEX>" },
  { "--acq-code-gen", "--acq-code <FIELD>, e.g. --acq-code pn:1023:10" },
  { "--acq-reps", "--acq-code '<FIELD>*<N>', e.g. --acq-code 'pn:31:5*4'" },
  { "--data-code-hex", "--data-code 0x<HEX>" },
  { "--data-code-gen",
    "--data-code <FIELD>, e.g. --data-code gold:64:10:..." },
  { "--sync-gen", "--sync <FIELD>, e.g. --sync pn:63:6" },
/* The coding sugar (docs/design/frame-description.md R): a coded frame is
   a description, and a stage names the span it covers. */
#define CODED "a coded frame is a description"
  { "--rs-depth", "--frame FILE, with an \"rs\" stage over the data group",
    CODED },
  { "--randomise",
    "--frame FILE, with a \"randomise\" stage over the data group", CODED },
  { "--randomize",
    "--frame FILE, with a \"randomise\" stage over the data group", CODED },
  { "--asm", "--frame FILE, with the marker 0x1ACFFC1D as its first field",
    CODED },
  { "--conv", "--frame FILE, with a \"conv\" stage over every field", CODED },
  { "--interleave",
    "--frame FILE, with an \"interleave\" stage over the data group", CODED },
  { "--interleave-unit", "--frame FILE: the interleave stage's \"unit_bits\"",
    CODED },
#undef CODED
};

/* The retired row for a flag, or NULL. */
static const retired_t *
retired (const char *a)
{
  for (size_t k = 0; k < sizeof RETIRED / sizeof *RETIRED; k++)
    if (!strcmp (a, RETIRED[k].flag))
      return &RETIRED[k];
  return NULL;
}

static int
find_opt (const char *a, opt_t *out)
{
  for (size_t k = 0; k < sizeof OPTS / sizeof *OPTS; k++)
    if (!strcmp (a, OPTS[k].name)
        || (OPTS[k].alias && !strcmp (a, OPTS[k].alias)))
      {
        *out = OPTS[k];
        return 1;
      }
  for (size_t k = 0; k < WFM_SURFACE_N; k++)
    {
      const wfm_surface_row_t *r = &WFM_SURFACE[k];
      if (!r->cli || strcmp (a, r->cli)) /* a JSON-only row has no flag */
        continue;
      const size_t base = r->owner == WFM_SURF_SOURCE ? OFF (src) : OFF (seg);
      memset (out, 0, sizeof *out);
      out->name          = r->cli;
      out->off           = base + r->off;
      out->seen          = OFF (surf_seen) + k * sizeof (int);
      out->unit_interval = r->unit_interval;
      out->range_bit     = r->range_bit;
      out->tbl           = r->choices;
      out->ntbl          = r->n_choices;
      if (r->range_bit)
        out->aux = base + r->hi_off;
      switch (r->kind)
        {
        case WFM_SV_DOUBLE:
          /* OPT_RANGE_D ranges a SOURCE field and OPT_RANGE_N a SEGMENT
             one; the generator refuses any other pairing. */
          out->kind = r->range_bit ? OPT_RANGE_D : OPT_DOUBLE;
          break;
        case WFM_SV_SIZE:
          out->kind = r->range_bit ? OPT_RANGE_N : OPT_SIZE;
          break;
        case WFM_SV_INT:
          /* A row the scene writes as true/false is a switch on the
             command line too: --code-only, not --code-only 1. */
          out->kind = r->json_bool ? OPT_SET : OPT_INT;
          break;
        case WFM_SV_U32:
          out->kind = OPT_U32;
          break;
        case WFM_SV_U64:
          out->kind = OPT_U64;
          break;
        case WFM_SV_CHOICE:
          out->kind = OPT_CHOICE;
          break;
        case WFM_SV_SYMBOLS:
          out->kind = OPT_SYMBOLS;
          out->aux  = base + r->len_off;
          break;
        case WFM_SV_FIELD:
          out->kind       = OPT_FIELD;
          out->field_reps = r->reps_off != 0;
          if (out->field_reps)
            out->aux = base + r->reps_off;
          break;
        case WFM_SV_BESPOKE:
          /* Kept as the raw token; the face reads it (load_frame). */
          out->kind = OPT_STR;
          out->off  = OFF (surf_text) + k * sizeof (const char *);
          break;
        }
      return 1;
    }
  return 0;
}

/* A Field flag (--acq-code, --sync, --data-code, --data): the text parsed by
 * the ONE reader of the grammar, dp_wfm_field_parse, into the source's
 * wfm_seq_t. A repeated flag replaces -- its literal array is freed, since
 * the source owns it. *REPS goes to the row's repetition count (`aux`) when
 * it has one (--acq-code: acq_reps); anywhere else it is refused, because a
 * sync word or a payload sent twice is not what those fields mean. Returns 0,
 * or 2 with the parser's own reason, prefixed by the flag. */
static int
parse_field_into (const opt_t *opt, const char *a, const char *v,
                  wfm_seq_t *dst, size_t *reps)
{
  wfm_field_t f;
  uint8_t    *owned = NULL;
  const char *why   = NULL;
  if (dp_wfm_field_parse (v, &f, &owned, &why) != DP_OK)
    {
      (void)fprintf (stderr, "error: %s %s: %s\n", a, v, why);
      return 2;
    }
  if (f.reps > 1 && !opt->field_reps)
    {
      free (owned);
      (void)fprintf (stderr, "error: %s %s: %s\n", a, v,
                     WFM_SURFACE_REPS_WHY_CLI);
      return 2;
    }
  if (dst->kind == WFM_SEQ_LITERAL)
    free ((void *)dst->bits); /* owned by the source; see wfm_seq_t */
  *dst = f.seq;
  if (opt->field_reps)
    *reps = f.reps;
  return 0;
}

/* Resolve argv into `o`. Returns 0, or the exit code of the first failure —
 * 2 for a usage error, 1 for an unreadable input file.
 *
 * It RETURNS rather than exiting because a half-parsed `o` can already own
 * heap (--data and friends): the caller's `done:` path frees it on every
 * exit, which is the fix for the five unix.Malloc leaks the old inline chain
 * carried past its 28 early returns.
 *
 * --help / -h / --version / -V never reach here; they are handled by the
 * pre-scan in dp_doppler_wfmgen so they work regardless of the other flags.
 */
static int
parse_args (int argc, char *argv[], wfmgen_opts_t *o)
{
  char *base = (char *)o;

  for (int i = 1; i < argc; i++)
    {
      const char  *a = argv[i];
      opt_t        row;
      const opt_t *opt = find_opt (a, &row) ? &row : NULL;
      if (!opt)
        {
          const retired_t *r = retired (a);
          if (r)
            (void)fprintf (stderr, "error: %s is retired: %s -- write %s\n", a,
                           r->why ? r->why : "one flag per field now",
                           r->instead);
          else
            (void)fprintf (stderr, "error: unknown option '%s' (try --help)\n",
                           a);
          return 2;
        }

      /* The value token, or NULL when the flag was last on the line. Only
         OPT_SET takes none; OPT_STR stores the NULL verbatim (an --output
         with nothing after it is the same as no --output). Every other kind
         must reject it rather than hand it to strtod, which is undefined and
         segfaults in practice. */
      const char *v = NULL;
      if (opt->kind != OPT_SET)
        v = (i + 1 < argc) ? argv[++i] : NULL;
      if (!v && opt->kind != OPT_SET && opt->kind != OPT_STR)
        {
          (void)fprintf (stderr, "error: %s requires a value\n", a);
          return 2;
        }

      void *dst = base + opt->off;
      void *aux = base + opt->aux; /* the discard slot when the row has none */
      if (opt->seen)
        *(int *)(base + opt->seen) = 1;

      switch (opt->kind)
        {
        case OPT_SET:
          *(int *)dst = 1;
          break;

        case OPT_STR:
          *(const char **)dst = v;
          break;

        case OPT_CHOICE:
          {
            int idx = lookup (v, opt->tbl, opt->ntbl);
            if (idx < 0)
              {
                (void)fprintf (stderr, "error: bad value for %s\n", a);
                return 2;
              }
            *(int *)dst = idx;
          }
          break;

        case OPT_DOUBLE:
          {
            double d;
            if (read_double (v, v + strlen (v), &d) != 0)
              {
                (void)fprintf (stderr,
                               "error: %s takes a number -- not '%s'\n", a, v);
                return 2;
              }
            if (opt->unit_interval && (d <= 0.0 || d > 1.0))
              {
                (void)fprintf (stderr, "error: %s must be in (0, 1]\n", a);
                return 2;
              }
            *(double *)dst = d;
          }
          break;

        /* The four integer kinds differ only in the destination's width,
           which is the bound a value must fit rather than wrap into. */
        case OPT_INT:
        case OPT_SIZE:
        case OPT_U32:
        case OPT_U64:
          {
            const uint64_t max = opt->kind == OPT_INT    ? (uint64_t)INT_MAX
                                 : opt->kind == OPT_SIZE ? (uint64_t)SIZE_MAX
                                 : opt->kind == OPT_U32  ? (uint64_t)UINT32_MAX
                                                         : UINT64_MAX;
            uint64_t       x;
            if (flag_uint (a, v, max, &x) != 0)
              return 2;
            if (opt->kind == OPT_INT)
              *(int *)dst = (int)x;
            else if (opt->kind == OPT_SIZE)
              *(size_t *)dst = (size_t)x;
            else if (opt->kind == OPT_U32)
              *(uint32_t *)dst = (uint32_t)x;
            else
              *(uint64_t *)dst = x;
          }
          break;

        case OPT_FIELD:
          {
            int rc = parse_field_into (opt, a, v, (wfm_seq_t *)dst,
                                       (size_t *)aux);
            if (rc)
              return rc;
          }
          break;

        case OPT_RANGE_D:
          {
            /* parse_range writes *hi only for a real range, so a later bare
               scalar clears the bit and leaves the stale hi unread. */
            int ranged = 0;
            if (parse_range (a, v, (double *)dst, (double *)aux, &ranged))
              return 2;
            o->src.ranged = ranged ? (o->src.ranged | opt->range_bit)
                                   : (o->src.ranged & ~opt->range_bit);
          }
          break;

        case OPT_RANGE_N:
          {
            int    ranged = 0;
            double lo = 0.0, hi = 0.0;
            if (parse_range (a, v, &lo, &hi, &ranged))
              return 2;
            /* A sample count, read as a double so `1e3` stays legal -- but
               only a WHOLE, non-negative one that fits: converting a
               negative double to size_t is undefined, and on x86-64 it made
               `--delay -1` a 2^64-sample run that wrote without bound
               (doppler#1629); `1.5` truncated silently. */
            if (!whole_count (lo) || (ranged && !whole_count (hi)))
              {
                (void)fprintf (stderr,
                               "error: %s takes a whole, non-negative sample "
                               "count, or a LO:HI range of two -- not '%s'\n",
                               a, v);
                return 2;
              }
            *(size_t *)dst = (size_t)lo;
            *(size_t *)aux = (size_t)hi;
            o->seg.ranged  = ranged ? (o->seg.ranged | opt->range_bit)
                                    : (o->seg.ranged & ~opt->range_bit);
          }
          break;

        case OPT_SYMBOLS:
          {
            float _Complex **p = (float _Complex **)dst;
            free (*p);
            *p = read_cf32_file (v, (size_t *)aux);
            if (!*p)
              {
                (void)fprintf (stderr,
                               "error: %s %s unreadable or not whole cf32\n",
                               a, v);
                return 1;
              }
          }
          break;
        }
    }

  return 0;
}

/* ── Emitting ────────────────────────────────────────────────────────────
 *
 * Three destinations — a NATS subject, a detached BLUE pair, and an ordinary
 * file or stdout — over one composer. Everything they share is gathered here
 * once so each destination is only what is actually different about it.
 */
typedef struct
{
  const wfmgen_opts_t    *o;
  dp_wfm_compose_state_t *comp;
  const wfm_segment_t    *segs; /* resolved, borrowed from the composer */
  size_t                  n_segs;
  double                  fs;      /* the capture sample rate */
  double                  gain;    /* 10^(-headroom/20), the peak backoff */
  int                     endless; /* the composer resolved --continuous */
  dp_sample_clock_t      *clk;     /* NULL unless the run is real-time */
} emit_ctx_t;

/* Open a writer on `fp` and apply the run's gain and clip tracking. */
static dp_wfm_writer_state_t *
open_writer (const emit_ctx_t *e, FILE *fp, int file_type)
{
  dp_wfm_writer_state_t *w = dp_wfm_writer_open (
      fp, file_type, e->o->sample_type, e->o->endian, e->fs, e->o->fc, 0, 0.0);
  if (!w)
    return NULL;
  dp_wfm_writer_set_gain (w, e->gain);
  if (e->o->clip_report)
    dp_wfm_writer_track_clipping (w, 1);
  return w;
}

/* Drive the composer into `w` until it runs dry; returns the sample count
 * (the BLUE header needs it, the other callers discard it).
 *
 * `paced` is a parameter rather than just `e->clk != NULL` because the
 * detached path does NOT pace and never has — see emit_detached_blue.
 */
static size_t
drain_to_writer (const emit_ctx_t *e, dp_wfm_writer_state_t *w, int paced)
{
  float _Complex buf[BLK];
  size_t n, total = 0;
  double rate = e->fs;
  /* By rate: a block never spans two, so a scene whose segments differ is
     paced at each one's own (doppler#1733). A short block is a rate
     change, not the end -- 0 is the end. */
  while ((n = dp_wfm_compose_execute_rate (e->comp, buf, BLK, &rate)) > 0)
    {
      dp_wfm_writer_write (w, buf, n);
      total += n;
      if (paced && e->clk)
        {
          dp_sample_clock_set_rate (e->clk, rate);
          dp_sample_clock_pace (e->clk, n);
        }
      /* An interrupted capture must still be a VALID capture. The BLUE
         header carries the final sample count and is written by
         dp_wfm_writer_close, so leaving the loop is what lets the file be
         closed properly -- killing the process here would leave a capture
         with no header at all, which is worse than a short one. */
      if (dp_interrupted ())
        break;
    }
  return total;
}

/* Close a writer, reporting (and with --clip-error, failing on) clipping. */
static int
close_writer (const emit_ctx_t *e, dp_wfm_writer_state_t *w)
{
  int rc = report_clip (dp_wfm_writer_peak (w),
                        dp_wfm_writer_clip_fraction (w), e->o->sample_type,
                        e->o->headroom, e->o->clip_report, e->o->clip_error);
  dp_wfm_writer_close (w);
  return rc ? 1 : 0;
}

/* Stream to a NATS PUB subject.
 *
 * The stream sink lives in the optional libdoppler_stream component (it pulls
 * in the vendored nats.c client). The pure-C core links only weak no-op
 * stubs, so dp_wfm_stream_sink_available() reports 0 unless the real component
 * is linked — which is a clearer failure than silently publishing nothing. */
static int
emit_to_stream (const emit_ctx_t *e)
{
  const wfmgen_opts_t *o = e->o;
  if (!dp_wfm_stream_sink_available ())
    {
      (void)fprintf (stderr,
                     "error: nats output (%s) requires the stream component; "
                     "this build was not linked against libdoppler_stream\n",
                     o->out_path);
      return 1;
    }
  /* A real --sample-type has no wire representation yet (doppler#1035), and
     "cannot open sink" would send the reader looking at their broker. */
  if (o->sample_type >= 5)
    {
      fprintf (stderr,
               "error: --sample-type %s is a real (scalar) format, and the\n"
               "  nats:// stream carries complex samples only.\n"
               "  Write a file, or use a complex sample type.\n",
               STYPE_NAMES[o->sample_type]);
      return 1;
    }
  wfm_stream_sink_t *sink
      = dp_wfm_stream_sink_open (o->out_path, o->sample_type);
  if (!sink)
    {
      (void)fprintf (stderr, "error: cannot open stream sink %s\n",
                     o->out_path);
      return 1;
    }
  dp_wfm_stream_sink_set_gain (sink, e->gain);
  if (o->clip_report)
    dp_wfm_stream_sink_track_clipping (sink, 1);

  float _Complex buf[BLK];
  size_t n;
  double rate = e->fs;
  /* Each frame header carries its own fs, so a scene whose segments
     differ is described honestly: a block never spans two rates, and each
     frame states its own (doppler#1733). 0 is the end. */
  while ((n = dp_wfm_compose_execute_rate (e->comp, buf, BLK, &rate)) > 0)
    {
      dp_wfm_stream_sink_send (sink, buf, n, rate, o->fc);
      if (e->clk)
        {
          dp_sample_clock_set_rate (e->clk, rate);
          dp_sample_clock_pace (e->clk, n);
        }
      if (dp_interrupted ())
        break;
    }
  int rc = report_clip (
      dp_wfm_stream_sink_peak (sink), dp_wfm_stream_sink_clip_fraction (sink),
      o->sample_type, o->headroom, o->clip_report, o->clip_error);

  /* Say the stream has ended BEFORE draining. The order matters: a drain
     cannot be reversed and refuses sends once it reaches its
     publish-flushing phase, so an EOS issued after one may simply not go.
     Without this a subscriber has only silence to go on, and silence is
     exactly what it cannot interpret. */
  (void)dp_wfm_stream_sink_send_eos (sink);

  /* Drain BEFORE close, on every exit -- interrupted or finished. A send
     returns once the client has the block, not once the server does, so
     closing without this leaves the tail to the client's own best-effort
     flush: 500 ms, no failure report, silently dropped beyond that. The
     budget is reported rather than swallowed, because "wfmgen exited 0" has
     to mean the samples arrived. */
  int drc = dp_wfm_stream_sink_drain (sink, 0);
  if (drc != DP_OK)
    {
      (void)fprintf (stderr,
                     "wfmgen: stream did not drain (error %d) -- the tail "
                     "may not have reached the server\n",
                     drc);
      rc = 1;
    }
  dp_wfm_stream_sink_close (sink);
  return rc ? 1 : 0;
}

/* BLUE detached: the raw data to <out>.det, the full HCB to <out>.hdr. The
 * header carries the final sample count, so it is written after the drain. */
static int
emit_detached_blue (const emit_ctx_t *e)
{
  const wfmgen_opts_t *o = e->o;
  if (!o->out_path)
    {
      (void)fprintf (stderr, "error: --detached needs --output\n");
      return 2;
    }
  /* The .hdr is written when the run ends, so a run that never ends has no
     header. --repeat loops the spec forever exactly as --continuous does;
     refusing only one of them let the other write until the disk filled
     (doppler#1591). */
  if (e->endless || o->repeat)
    {
      (void)fprintf (stderr, "error: --detached requires finite output "
                             "(not --continuous or --repeat)\n");
      return 2;
    }
  char det_path[1024];
  if (build_path (det_path, sizeof det_path, o->out_path, ".det") != 0)
    {
      (void)fprintf (stderr, "error: output path too long\n");
      return 2;
    }
  FILE *df = fopen (det_path, "wb");
  if (!df)
    {
      (void)fprintf (stderr, "error: cannot open %s\n", det_path);
      return 1;
    }

  int                    rc    = 0;
  size_t                 total = 0;
  dp_wfm_writer_state_t *w     = open_writer (e, df, WFM_FT_RAW);
  if (w)
    {
      /* Unpaced, unlike the stream and file paths, and now unreachable with
         a clock: check_detached rejects --detached --realtime rather than
         accepting a flag it would drop (gh-725). The 0 is therefore the only
         possible value here, not a preserved asymmetry. */
      total = drain_to_writer (e, w, 0);
      rc    = close_writer (e, w);
    }
  (void)fclose (df);

  char  hdr_path[1024];
  FILE *hf = build_path (hdr_path, sizeof hdr_path, o->out_path, ".hdr")
                 ? NULL
                 : fopen (hdr_path, "wb");
  if (!hf)
    return 1;
  dp_wfm_blue_write_hcb (hf, o->sample_type, o->endian, e->fs, o->fc, 0.0,
                         total, 1, 0.0);
  (void)fclose (hf);
  return rc;
}

/* The .sigmf-meta sidecar, from the resolved spans. Best-effort: a capture
 * whose data was written is not failed by an unwritable sidecar. */
static void
write_sigmf_meta (const emit_ctx_t *e)
{
  char *meta = dp_wfm_sigmf_meta_json (e->o->sample_type, e->o->endian, e->fs,
                                       e->o->fc, 0.0, e->segs, e->n_segs);
  if (!meta)
    return;
  char  meta_path[1024];
  FILE *mf
      = build_path (meta_path, sizeof meta_path, e->o->out_path, ".sigmf-meta")
            ? NULL
            : fopen (meta_path, "w");
  if (mf)
    {
      (void)fputs (meta, mf);
      (void)fclose (mf);
    }
  free (meta);
}

/* A file or stdout. SigMF writes the pair <base>.sigmf-data + .sigmf-meta. */
static int
emit_to_file (const emit_ctx_t *e)
{
  const wfmgen_opts_t *o     = e->o;
  int                  sigmf = o->file_type == 3;
  FILE                *fp;
  char                 data_path[1024];

  if (sigmf)
    {
      if (!o->out_path)
        {
          (void)fprintf (stderr, "error: --file-type sigmf needs --output\n");
          return 2;
        }
      if (e->endless)
        {
          /* The sidecar is written after the emit loop from the resolved
             spans; an unbounded stream never reaches it (the constraint
             --detached has, for the same reason). */
          (void)fprintf (stderr, "error: --file-type sigmf requires finite "
                                 "output (not --continuous)\n");
          return 2;
        }
      if (build_path (data_path, sizeof data_path, o->out_path, ".sigmf-data")
          != 0)
        {
          (void)fprintf (stderr, "error: output path too long\n");
          return 2;
        }
      fp = fopen (data_path, "wb");
    }
  else
    {
      /* Refuse to spew raw binary IQ onto an interactive terminal (the
       * footgun when --output is forgotten — `wfmgen` alone defaults to raw
       * to stdout). An explicit `--output -` is stdout too, so it must trip
       * the same guard. CSV is human-readable text so it is allowed;
       * piping/redirecting stdout (not a tty) is always allowed. */
      int to_stdout = !o->out_path || !strcmp (o->out_path, "-");
      if (to_stdout && o->file_type != WFM_FT_CSV && isatty (fileno (stdout)))
        {
          (void)fprintf (stderr,
                         "error: refusing to write binary IQ to a terminal — "
                         "pass --output FILE (or redirect/pipe stdout)\n\n");
          (void)fputs (USAGE, stderr);
          return 1;
        }
      fp = to_stdout ? stdout : fopen (o->out_path, "wb");
    }
  if (!fp)
    {
      (void)fprintf (stderr, "error: cannot open output\n");
      return 1;
    }

  int                    rc = 0;
  dp_wfm_writer_state_t *w
      = open_writer (e, fp, sigmf ? WFM_FT_RAW : o->file_type);
  if (!w)
    {
      (void)fprintf (stderr, "error: cannot open writer\n");
      rc = 1;
    }
  else
    {
      (void)drain_to_writer (e, w, 1);
      rc = close_writer (e, w);
    }
  if (fp != stdout)
    (void)fclose (fp);

  if (sigmf && rc == 0)
    write_sigmf_meta (e);
  return rc;
}

/* The --record sidecar: the fully-resolved run, as the JSON that --from-file
 * reads back. Best-effort, like the SigMF sidecar. */
static void
write_record (const emit_ctx_t *e, int repeating)
{
  char *json = dp_wfm_spec_to_json (e->segs, e->n_segs, repeating, e->endless,
                                    dp_wfm_compose_seed_advance (e->comp),
                                    e->o->headroom);
  if (!json)
    return;
  FILE *rf = fopen (e->o->record_path, "w");
  if (rf)
    {
      (void)fputs (json, rf);
      (void)fputc ('\n', rf);
      (void)fclose (rf);
    }
  free (json);
}

/* --detached selects a FILE FORMAT -- BLUE's detached header, the HCB in
 * <out>.hdr and the samples in <out>.det -- and not a process model. It is
 * honoured by exactly one destination, so every other combination silently
 * dropped a flag the user typed (gh-725).
 *
 * Two were dropped. `--realtime` never reached the detached drain, and
 * `--detached` itself was ignored whenever the destination was not a BLUE
 * file: the dispatch tests `file_type == 2 && detached`, so a nats:// URL or
 * any other --file-type fell through to the ordinary writer and produced one
 * undetached file.
 *
 * Rejecting rather than pacing settles gh-725's open question, and the
 * argument is the destination's own shape: the .hdr carries the final sample
 * count, so it cannot be written until the drain ends, and --detached refuses
 * an endless run anyway. There is no consumer that a paced detached write
 * would serve -- nothing can read the pair until it is complete. Pacing it
 * would only make a finite file take longer with nobody waiting.
 *
 * The confusion was seeded by this tool's own help, which described the flag
 * as "Run as a detached background process" and filed it under REAL-TIME,
 * where --realtime looks like it must apply. The guide and the CLI test had
 * it right all along; the help and the flag-matrix exclusion did not.
 *
 * Returns 0, or the usage exit code.
 */
static int
check_detached (const wfmgen_opts_t *o)
{
  if (!o->detached)
    return 0;
  if (o->realtime || o->realtime_resync)
    {
      (void)fprintf (stderr,
                     "error: --detached does not pace: it selects BLUE's "
                     "detached-header FILE FORMAT (<out>.hdr + <out>.det), "
                     "and the header is written only once the run ends, so "
                     "nothing can read the pair while it is being paced. "
                     "Drop --realtime, or write a single file instead.\n");
      return 2;
    }
  if (o->out_path && !strncmp (o->out_path, "nats://", 7))
    {
      (void)fprintf (stderr, "error: --detached writes a file pair; it has no "
                             "meaning for a nats:// destination\n");
      return 2;
    }
  if (o->file_type != 2)
    {
      (void)fprintf (stderr, "error: --detached is BLUE only; it needs "
                             "--file-type blue\n");
      return 2;
    }
  return 0;
}

/* Continuous-DSSS flag consistency, for a run built from the flags rather
 * than from a spec file. Every one of these rejects rather than silently
 * ignoring: the flags below are meaningless outside continuous DSSS, and a
 * knob that does nothing in the mode you are in is the worse failure (the
 * --detached precedent, now enforced above rather than merely cited).
 * Returns 0, or the usage exit code.
 */
static int
check_continuous_dsss (const wfmgen_opts_t *o)
{
  if (o->surf_seen[WFM_SURFACE_source_symbol_rate]
      && o->src.symbol_rate <= 0.0)
    {
      (void)fprintf (stderr, "error: --symbol-rate must be positive (it is "
                             "the continuous-dsss data symbol rate in Hz)\n");
      return 2;
    }
  if (o->src.symbol_rate <= 0.0)
    {
      if (o->surf_seen[WFM_SURFACE_source_dsss_code_only])
        {
          (void)fprintf (stderr, "error: --code-only sends a continuous-dsss "
                                 "code alone; it needs --symbol-rate\n");
          return 2;
        }
      return 0;
    }
  if (o->src.type != WFM_SYNTH_DSSS)
    {
      (void)fprintf (stderr, "error: --symbol-rate is only for --type dsss\n");
      return 2;
    }
  /* LENGTH, not the pointer: a generated sequence (--data-code pn:...,
     --sync gold:...) has no array, so a pointer test read a real code as
     absent and a real sync as absent (doppler#1592) -- the has_frame bug
     again. */
  if (o->src.data_code.len == 0)
    {
      (void)fprintf (stderr, "error: continuous dsss (--symbol-rate) needs "
                             "--data-code\n");
      return 2;
    }
  if (o->src.acq_code.len || o->src.sync.len || o->src.frame
      || o->surf_seen[WFM_SURFACE_source_crc])
    {
      (void)fprintf (stderr, "error: --acq-code/--sync/--crc/--frame are "
                             "burst-frame flags, meaningless with "
                             "--symbol-rate\n");
      return 2;
    }
  return 0;
}

/* Two surface rows given together that no face takes together (the
 * manifest's `exclusive`, rendered as WFM_SURFACE_EXCLUSIVE). A row is GIVEN
 * when its flag was, or when another flag filled its member. Returns 0, or
 * 2 having said why. */
static int
check_exclusive (const wfmgen_opts_t *o)
{
  for (size_t k = 0; k < WFM_SURFACE_N_EXCLUSIVE; k++)
    {
      const wfm_surface_exclusive_t *e       = &WFM_SURFACE_EXCLUSIVE[k];
      const int                      ends[2] = { e->a, e->b };
      int                            given   = 0;
      for (int j = 0; j < 2; j++)
        {
          const wfm_surface_row_t *r    = &WFM_SURFACE[ends[j]];
          const void              *base = r->owner == WFM_SURF_SOURCE
                                              ? (const void *)&o->src
                                              : (const void *)&o->seg;
          given += o->surf_seen[ends[j]] || wfm_surface_row_is_set (r, base);
        }
      if (given == 2 && e->cli_why)
        {
          (void)fprintf (stderr, "error: %s\n", e->cli_why);
          return 2;
        }
    }
  return 0;
}

/* `--frame FILE`: read a frame description and carry it on the source.
 *
 * The file holds what a scene's "frame" key holds, through the one reader
 * of that form (dp_wfm_frame_from_json). A carried description IS the frame,
 * so the flags that spell the common frame are refused beside it rather than
 * silently dropped: the sync word and an unspread preamble by the bridge
 * (dp_wfm_source_frame_error); --crc here,
 * because only this face can tell it was GIVEN (crc defaults to crc16).
 * Returns 0, or the exit code. */
static int
load_frame (wfmgen_opts_t *o)
{
  if (!FRAME_PATH (o))
    return 0;
  if (o->surf_seen[WFM_SURFACE_source_crc])
    {
      (void)fprintf (stderr, "error: --frame FILE is the whole frame: its CRC "
                             "is a stage in the file, so --crc cannot sit "
                             "beside it\n");
      return 2;
    }
  char *text = slurp_file (FRAME_PATH (o));
  if (!text)
    {
      (void)fprintf (stderr, "error: could not read %s\n", FRAME_PATH (o));
      return 1;
    }
  const char *why = NULL;
  o->src.frame    = dp_wfm_frame_from_json (text, &why);
  free (text);
  if (!o->src.frame)
    {
      (void)fprintf (stderr, "error: %s: %s\n", FRAME_PATH (o),
                     why ? why : "not a frame description");
      return 2;
    }
  return 0;
}

/**
 * @brief Refuse a source that cannot be built — with the reason.
 *
 * The rule itself is `dp_wfm_source_error()`: a source's own parameters
 * (a `--pn-poly` wider than `--pn-length`'s register) and then its frame
 * (`dp_wfm_source_frame_error()`), shared with the standalone Synth, the
 * scene reader and the composer so every face answers identically. What
 * this adds is the CLI's half of the contract: a named exit code, the reason
 * on stderr, and the flag values when the reason is about them -- rather
 * than the generic build failure a NULL from `dp_wfm_compose_create()`
 * produces. Both refuse; only one of them tells you what to do instead.
 */
static int
check_source (wfmgen_opts_t *o)
{
  /* A data source sets the run's length (payload-data-source.md 4.6): a
     finite one is its frames, so a --count beside it is refused by name
     -- the one face that can tell a count given from its default -- and
     an absent count is 0 (the composer derives it, or runs a stream until
     it ends). A stream may take a --count as an upper bound. */
  const int has_data = o->src.data.len || o->src.data_from_file;
  /* A carried frame of fixed bits is a finite source too: one frame, sent
     once (doppler#1718) -- more of it is --repeats, a gap after it
     --off. */
  const int finite = dp_wfm_source_data_frames (&o->src) > 0;
  if (finite && o->surf_seen[WFM_SURFACE_segment_num_samples])
    {
      (void)fprintf (stderr,
                     has_data ? "error: --count: a finite data source sets "
                                "the run's length (its frames); drop "
                                "--count\n"
                              : "error: --count: a carried frame of fixed "
                                "bits is sent once and sets the run's length "
                                "(one frame); drop --count, and give "
                                "--repeats for more\n");
      return 2;
    }
  if ((has_data || finite) && !o->surf_seen[WFM_SURFACE_segment_num_samples])
    o->seg.num_samples = 0;
  if (has_data)
    {
      /* Paced, a pause in a pipe is an idle frame -- and continuous dsss
         has no frame, so it has nothing to send while it waits: the output
         would fall behind the clock it is paced to. */
      if (o->realtime && o->src.type == WFM_SYNTH_DSSS
          && o->src.symbol_rate > 0.0
          && dp_wfm_source_data_is_stream (&o->src))
        {
          (void)fprintf (stderr,
                         "error: --realtime: a continuous dsss source has "
                         "no frame to send idle while stdin pauses: drop "
                         "--realtime, or give a finite source\n");
          return 2;
        }
    }
  const char *why = dp_wfm_scene_error (&o->seg, 1, o->repeat, o->continuous);
  if (!why)
    return 0;
  if (why == dp_wfm_why_pn_poly)
    (void)fprintf (stderr, "error: --pn-poly 0x%llx, --pn-length %d: %s\n",
                   (unsigned long long)o->src.pn_poly, o->src.pn_length, why);
  else if (why == dp_wfm_why_dsss_frame_no_data_code)
    (void)fprintf (stderr, "error: --data-code: %s\n", why);
  else if (why == dp_wfm_why_dsss_cont_rate)
    (void)fprintf (stderr, "error: --symbol-rate %g, --fs %g, --sps %d: %s\n",
                   o->src.symbol_rate, o->seg.fs, o->src.sps, why);
  else
    (void)fprintf (stderr, "error: %s\n", why);
  return 2;
}

/* An absolute form of @p path, malloc'd, or NULL. */
static char *
absolute_path (const char *path)
{
#ifdef _WIN32
  return _fullpath (NULL, path, 0);
#else
  return realpath (path, NULL);
#endif
}

/* The directory part of @p path, malloc'd: "." when it has none. */
static char *
dir_of (const char *path)
{
  const char  *slash = strrchr (path, '/');
  const size_t n     = slash ? (size_t)(slash - path) : 0u;
  char        *d     = dp_xmalloc (n + 2u);
  if (!slash)
    (void)strcpy (d, ".");
  else if (n == 0)
    (void)strcpy (d, "/");
  else
    {
      memcpy (d, path, n);
      d[n] = '\0';
    }
  return d;
}

/* A --record replays from its own directory: a scene's relative
 * "data_from_file" resolves against the scene (dp_wfm_compose_from_json_at).
 * So when the record is written anywhere but the working directory, a
 * relative --data-from-file is made absolute before the run, and the source
 * and its record name the same file. Beside the working directory -- the
 * common case -- the path is kept as typed, so a record stays portable.
 * Returns a malloc'd absolute path to use instead, or NULL to keep it. */
static char *
data_path_for_record (const wfmgen_opts_t *o)
{
  const char *p = o->src.data_from_file;
  if (!p || !o->record_path || p[0] == '/' || strcmp (p, "-") == 0)
    return NULL;
  char     *rdir = dir_of (o->record_path);
  char     *ra   = rdir ? absolute_path (rdir) : NULL;
  char     *ca   = absolute_path (".");
  const int same = ra && ca && strcmp (ra, ca) == 0;
  free (rdir);
  free (ra);
  free (ca);
  return same ? NULL : absolute_path (p);
}

/* `wfmgen json-template [FILE]` — emit a ready-to-edit example spec in the
 * canonical --from-file schema. Writes to FILE, or to stdout when FILE is
 * absent or "-". JSON is text, so the binary-to-tty guard the emit paths
 * carry does not apply: printing it to a terminal is the point. */
static int
run_json_template (int argc, char *argv[])
{
  const char *tpl_path
      = (argc >= 3 && strcmp (argv[2], "-") != 0) ? argv[2] : NULL;
  char *json = dp_wfm_spec_template_json ();
  if (!json)
    {
      (void)fprintf (stderr, "error: out of memory building the template\n");
      return 1;
    }
  FILE *tf = tpl_path ? fopen (tpl_path, "wb") : stdout;
  if (!tf)
    {
      (void)fprintf (stderr, "error: cannot open %s for writing\n", tpl_path);
      free (json);
      return 1;
    }
  (void)fputs (json, tf);
  (void)fputc ('\n', tf);
  if (tpl_path)
    (void)fclose (tf);
  free (json);
  return 0;
}

/* The CLI's whole body lives here as a plain callable (argv in, exit-code out)
 * so it can be archived into libdoppler and invoked by a downstream linker —
 * the `wfmgen` binary is a one-line `main` shim over it (wfmgen_main.c).
 *
 * It reads as the four phases it is: resolve argv (parse_args, over the
 * option table above), check the flag combinations a single-segment run
 * cannot honour, build the composer, and emit.
 *
 * Every failure exits through `done:`, which frees the parsed source and
 * destroys the composer. `comp` and `rc` are therefore declared here rather
 * than at first use: a `goto` that jumps past a declaration leaves it
 * uninitialised, and the label reads both. */
static int
wfmgen_run (int argc, char *argv[])
{
  dp_wfm_compose_state_t *comp
      = NULL;            /* dp_wfm_compose_destroy tolerates NULL */
  char *data_abs = NULL; /* an absolute --data-from-file, when one was made */
  int   rc       = 0;

  /* --help / --version short-circuit before any spec is built, so they work
   * regardless of the other flags and never leak a partially-parsed source. */
  for (int i = 1; i < argc; i++)
    {
      if (!strcmp (argv[i], "--help") || !strcmp (argv[i], "-h"))
        {
          (void)fputs (USAGE, stdout);
          return 0;
        }
      if (!strcmp (argv[i], "--version") || !strcmp (argv[i], "-V"))
        {
          (void)printf ("wfmgen (doppler) %s\n", DOPPLER_VERSION);
          return 0;
        }
    }

  if (argc >= 2 && !strcmp (argv[1], "json-template"))
    return run_json_template (argc, argv);

  /* Single-segment defaults: one source in one segment. fs = 1.0 means
     frequencies are normalised (cycles/sample) out of the box.

     These no longer MIRROR the Python defaults -- they ARE them, rendered
     from the same `just-makeit.toml` declaration by
     `scripts/gen_wfm_defaults.py` (doppler#1142). They used to be a struct
     literal here whose own comment said it mirrored the manifest, with no
     gate comparing the two, and with `modulation` and `crc` restated as enum
     INDICES so a prepended [[enum]] entry would have changed them silently.
     Every other field is zero, which is the default the option table's rows
     are written against, and which is why the generated macros carry only
     the non-zero ones. */
  wfmgen_opts_t o
      = { .src = WFM_SOURCE_DEFAULTS, .seg = WFM_SEGMENT_DEFAULTS };
  /* n_sources is the single-segment SHAPE rather than a user default: the
     manifest does not declare it and should not, because no flag sets it. */
  o.seg.n_sources = 1;
  /* Set after the initialiser, not inside it: `&o.src` is the address of a
     sibling member of the very object being initialised. */
  o.seg.sources = &o.src;

  rc = parse_args (argc, argv, &o);
  if (rc)
    goto done;

  /* Before the composer, and OUTSIDE the from_file branch below: --detached
     is a destination flag, so it means the same thing whether the run was
     built from a spec file or from the signal flags. check_continuous_dsss
     is flags-only because the knobs it polices have spec-file equivalents
     that the JSON reader validates itself. */
  rc = check_detached (&o);
  if (rc)
    goto done;
  rc = check_exclusive (&o);
  if (rc)
    goto done;

  if (o.surf_text[WFM_SURFACE_source_data_from_file] && o.from_file)
    {
      (void)fprintf (stderr, "error: --data-from-file names the data of a "
                             "run built from flags; a --from-file scene "
                             "carries its own, as a source's "
                             "\"data_from_file\"\n");
      rc = 2;
      goto done;
    }
  if (FRAME_PATH (&o) && o.from_file)
    {
      (void)fprintf (stderr, "error: --frame describes the frame of a run "
                             "built from flags; a --from-file scene carries "
                             "its own, as a source's \"frame\"\n");
      rc = 2;
      goto done;
    }

  /* Build the composer: from a JSON spec, or the single-segment flags. A
     recorded --headroom rides in the spec file and is reapplied here unless
     an explicit --headroom on this run overrides it. `comp` is declared at the
     top of the function so `done:` can destroy it from any exit. */
  if (o.from_file)
    {
      char *spec = slurp_file (o.from_file);
      if (!spec)
        {
          (void)fprintf (stderr, "error: could not read %s\n", o.from_file);
          rc = 1;
          goto done;
        }
      /* A refused FRAME is the one spec failure with a sentence behind it,
         so it exits here rather than falling through to the generic line
         below — two messages for one fault reads as two faults. */
      const char *why  = NULL;
      char       *sdir = dir_of (o.from_file);
      comp             = dp_wfm_compose_from_json_at (spec, sdir, &why);
      free (sdir);
      if (!comp && why)
        {
          (void)fprintf (stderr, "error: %s\n", why);
          free (spec);
          rc = 2;
          goto done;
        }
      if (!o.headroom_set)
        o.headroom = dp_wfm_spec_headroom (spec);
      free (spec);
    }
  else
    {
      rc = load_frame (&o);
      if (rc)
        goto done;
      /* The surface-only --data-from-file row: its text is the path, read
         by the data source when the synth is built (wfm/wfm_data.h). */
      o.src.data_from_file = o.surf_text[WFM_SURFACE_source_data_from_file];
      data_abs             = data_path_for_record (&o);
      if (data_abs)
        o.src.data_from_file = data_abs;
      rc = check_continuous_dsss (&o);
      if (rc)
        goto done;
      rc = check_source (&o);
      if (rc)
        goto done;
      comp = dp_wfm_compose_create (&o.seg, 1, o.repeat, o.continuous);
      dp_wfm_compose_set_seed_advance (comp, o.seed_advance);
      /* Paced, a data stream with nothing yet sends an idle frame of fill
         so the carrier and the frame timing never break (section 4.5). */
      if (o.realtime)
        dp_wfm_compose_set_data_pacing (comp, WFM_DATA_PACED);
    }
  if (!comp)
    {
      (void)fprintf (stderr, "error: could not build the waveform spec\n");
      rc = 1;
      goto done;
    }

  /* Borrow the resolved segments (for --record / SigMF) + the capture fs. */
  size_t               n_segs = 0;
  int                  r = 0, c = 0;
  const wfm_segment_t *segs = dp_wfm_compose_segments (comp, &n_segs, &r, &c);
  /* The one answer for the whole stream: the shared fs, or 0.0 -- "not
     stated" -- when segments differ, which SigMF says by omission and a
     BLUE header cannot say at all (doppler#1733). */
  double fs = n_segs ? dp_wfm_scene_fs (segs, n_segs) : o.seg.fs;
  if (fs == 0.0 && n_segs && o.file_type == WFM_FT_BLUE
      && !(o.out_path && !strncmp (o.out_path, "nats://", 7)))
    {
      size_t k = 1;
      while (segs[k].fs == segs[0].fs)
        k++;
      (void)fprintf (stderr,
                     "error: this scene's segments have different fs (%g, "
                     "%g); a BLUE file states one sample rate -- write "
                     "SigMF (--file-type sigmf), or give every segment one "
                     "fs\n",
                     segs[0].fs, segs[k].fs);
      rc = 2;
      goto done;
    }

  /* Real-time pacing: throttle the emit loop to fs, mimicking a sample clock
     driving the output. Anchored once here so the schedule is drift-free; a
     NULL clk in the context below is what "not real-time" means downstream. */
  dp_sample_clock_t clk = { 0 }; /* the underrun report reads it either way */
  if (o.realtime)
    dp_sample_clock_init (&clk, n_segs ? segs[0].fs : fs, o.realtime_resync);

  emit_ctx_t e = { .o       = &o,
                   .comp    = comp,
                   .segs    = segs,
                   .n_segs  = n_segs,
                   .fs      = fs,
                   .gain    = pow (10.0, -o.headroom / 20.0),
                   .endless = c,
                   .clk     = o.realtime ? &clk : NULL };

  if (o.record_path)
    write_record (&e, r);

  if (o.out_path && !strncmp (o.out_path, "nats://", 7))
    rc = emit_to_stream (&e);
  else if (o.file_type == 2 && o.detached)
    rc = emit_detached_blue (&e);
  else
    rc = emit_to_file (&e);

  if (o.realtime && clk.underruns)
    (void)fprintf (
        stderr, "wfmgen: %llu underrun(s) — worst %.3f ms behind real time\n",
        (unsigned long long)clk.underruns, (double)clk.max_late_ns / 1e6);

  /* The success path falls in here; every failure jumps to it. The composer
     deep-copied whatever it was handed, so the CLI-owned copies are always
     ours to release, and dp_wfm_compose_destroy tolerates the NULL `comp` an
     exit taken before the composer was built leaves behind. */
done:
  dp_wfm_compose_destroy (comp);
  source_free (&o.src);
  free (data_abs);
  return rc;
}

/* The public entry: the handlers go in FIRST and come out LAST, around the
 * whole run, so every one of wfmgen_run's exits -- --help, --version, a usage
 * error, done: -- leaves the caller's handlers as it found them
 * (doppler#1594). Restoring at each return instead is how one of them gets
 * missed.
 *
 * FIRST, before parsing or opening anything. A signal arriving before this
 * is not ignored, it terminates the process -- and the window is real:
 * measured at ~5 ms for a dynamically linked binary, which is long enough for
 * a supervisor's stop signal to land inside it. Both signals, because a
 * container runtime sends SIGTERM and a terminal sends SIGINT, and losing the
 * tail should not depend on which. */
int
dp_doppler_wfmgen (int argc, char *argv[])
{
  (void)dp_interrupt_on_signal (SIGINT);
  (void)dp_interrupt_on_signal (SIGTERM);

#ifdef _WIN32
  /* stdout carries the bytes a file would: every file is opened "wb", and
   * the Windows CRT's text-mode stdout turns each 0x0A into 0x0D 0x0A --
   * inside binary IQ that shifts every sample after it. Left binary on
   * return: stdout's mode is the process's, and a caller of an IQ
   * generator is not reading text from it. */
  (void)_setmode (_fileno (stdout), _O_BINARY);
#endif

  const int rc = wfmgen_run (argc, argv);

  (void)dp_restore_signal (SIGTERM);
  (void)dp_restore_signal (SIGINT);
  return rc;
}
