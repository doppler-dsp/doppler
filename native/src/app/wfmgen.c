/*
 * wfmgen.c — the waveform-generator composer CLI (Phase C, hand-written).
 *
 * The rich sibling of the generated `wavegen` single-shot tool: it sequences
 * multi-segment specs (`--from-file`), emits any output file type
 * (raw/csv/BLUE/SigMF, `--file-type`) in any wire type / byte order, streams
 * to a file, stdout, or a NATS PUB subject (`--output nats://…`), and writes a
 * JSON record of exactly what it produced (`--record`). All of it is thin glue
 * over the C cores in the wfmcompose c_dep — wfm_compose / wfm_writer /
 * wfm_sink — which is why this lives by hand rather than via `jm app` (a
 * composer is not a single-object generator).
 *
 * Single-segment mode (the default) builds a one-segment spec from the same
 * flags as `wavegen`, so `wfmgen --type qpsk --count 4096 …` and
 * `wavegen --type qpsk --count 4096 …` agree sample-for-sample.
 */
#include <complex.h>
#include <math.h>
#include <signal.h>
#include <stddef.h> /* offsetof — the option table names fields by offset */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> /* isatty */

#include "doppler/version.h" /* DOPPLER_VERSION (configure-time stamp) */
#include "dp_interrupt.h"
#include "timing/timing_core.h"
#include "wfm/wfm_compose.h"
#include "wfm/wfm_sink.h"
#include "wfm/wfmgen.h"
#include "wfm_writer/wfm_writer_core.h"

#define BLK 4096

static const char *const TYPES[]
    = { "tone",  "noise", "pn",      "bpsk", "qpsk",
        "chirp", "bits",  "symbols", "dsss" };
static const char *const MODES[]   = { "auto", "fs", "ebno", "esno" };
static const char *const CRCS[]    = { "none", "crc16" };
static const char *const BITMODS[] = { "none", "bpsk", "qpsk" };
static const char *const STYPES[]  = { "cf32", "cf64", "ci32", "ci16", "ci8" };
static const char *const FTYPES[]  = { "raw", "csv", "blue", "sigmf" };
static const char *const ENDIANS[] = { "le", "be" };
static const char *const LFSRS[]   = { "galois", "fibonacci" };
static const char *const PULSES[]  = { "rect", "rrc" };
/* Ordered to match wfm_seed_advance_t (none=0, noise=1, all=2). */
static const char *const SEEDADV[] = { "none", "noise", "all" };
/* Ordered to match wfm_segment_t.gap_noise (auto=0, off=1). */
static const char *const GAPNOISE[] = { "auto", "off" };
/* --data's two sources, ordered to match wfm_source_t.dsss_code_only rather
   than to match the usage text: "prbs" is the seeded PN (code_only 0) and
   "none" is code-only (code_only 1), so the chosen index IS the field. */
static const char *const DATA_SRC[] = { "prbs", "none" };
/* --randomise: WHICH section-10 generator, because 131.0-B-6 specifies two
   and they produce waveforms only the matching receiver derandomises.
   Index 0 is "off" so an absent flag and an explicit off are one value, and
   index 1 is B-6 10.4.1's -- the `shall` -- which is what OPT_CHOICE_OPT
   selects when the flag is given with no value. "legacy" is 10.4.2's 255-bit
   sequence, kept for backward compatibility and carrying spectral lines at
   1/255 of the symbol rate. */
static const char *const RANDS[] = { "off", "ccsds", "legacy" };

/* Look name up in a NULL-free table of n entries; -1 if absent. */
static int
lookup (const char *s, const char *const *tbl, int n)
{
  for (int i = 0; i < n; i++)
    if (!strcmp (s, tbl[i]))
      return i;
  return -1;
}

/* Parse a numeric flag value as a scalar (`12000`) or a uniform range
 * (`9000:14000`). Returns the low value; on a range it also sets *hi and
 * *ranged so the composer redraws the field each repeat. A bare scalar leaves
 * *ranged 0; strtod stops at the ':', so it yields lo directly. */
static double
parse_range (const char *v, double *hi, int *ranged)
{
  const char *colon = strchr (v, ':');
  if (colon && colon[1])
    {
      *hi     = strtod (colon + 1, NULL);
      *ranged = 1;
    }
  else
    *ranged = 0;
  return strtod (v, NULL);
}

/* Parse a binary string ("10110101") into a malloc'd 0/1 array; *n gets the
 * length. Whitespace is skipped; any other char fails (returns NULL). */
static uint8_t *
parse_bit_string (const char *s, size_t *n)
{
  size_t   cap = strlen (s), len = 0;
  uint8_t *b = malloc (cap ? cap : 1);
  if (!b)
    return NULL;
  for (; *s; s++)
    {
      if (*s == '0' || *s == '1')
        /* len advances only on a kept char, so len < strlen(s) == cap. */
        /* NOLINTNEXTLINE(clang-analyzer-security.ArrayBound) */
        b[len++] = (uint8_t)(*s - '0');
      else if (*s != ' ' && *s != '\t' && *s != '\n' && *s != '\r')
        {
          free (b);
          return NULL;
        }
    }
  *n = len;
  return b;
}

/* Parse a hex string ("AA55") into a malloc'd 0/1 array (MSB first), 4 bits
 * per hex digit; *n gets the bit count. Returns NULL on a non-hex char. */
static uint8_t *
parse_hex_string (const char *s, size_t *n)
{
  size_t   ndig = strlen (s);
  uint8_t *b    = malloc (ndig ? ndig * 4 : 1);
  if (!b)
    return NULL;
  size_t len = 0;
  for (; *s; s++)
    {
      int v;
      if (*s >= '0' && *s <= '9')
        v = *s - '0';
      else if (*s >= 'a' && *s <= 'f')
        v = *s - 'a' + 10;
      else if (*s >= 'A' && *s <= 'F')
        v = *s - 'A' + 10;
      else
        {
          free (b);
          return NULL;
        }
      for (int bit = 3; bit >= 0; bit--)
        /* exactly 4 writes per digit; b holds ndig*4 bytes — never overruns.
         */
        /* NOLINTNEXTLINE(clang-analyzer-security.ArrayBound) */
        b[len++] = (uint8_t)((v >> bit) & 1);
    }
  *n = len;
  return b;
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
      STYPES[stype], dbfs, need);
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
      "WAVEFORM TYPE\n"
      "  --type TYPE     tone | noise | pn | bpsk | qpsk | chirp | bits |\n"
      "                    symbols | dsss\n"
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
      "  (LO:HI on --freq/--f-end/--snr/--level/--count/--off draws that "
      "field\n"
      "   uniformly each repeat — e.g. --freq 9000:14000 — reproducible per"
      " seed)\n"
      "  --fs HZ         Sample rate (default 1.0; freq treated as"
      " normalised)\n"
      "  --freq HZ[:HZ]  Carrier / sweep-start frequency (default 0.0)\n"
      "  --f-end HZ[:HZ] Chirp sweep-end frequency (chirp only)\n"
      "  --fc HZ         Centre frequency stored in SigMF metadata only\n"
      "  --count N[:N]   Samples to generate (default 1024)\n"
      "  --off N[:N]     Trailing gap after the segment (carries the"
      " noise\n"
      "                  floor by default; zeros when clean or --gap-noise"
      " off)\n"
      "  --delay N[:N]   Leading gap before the burst (arrival delay /"
      " jitter)\n"
      "  --gap-noise M   auto | off — gaps carry the segment's noise floor\n"
      "                  (auto, default) or stay hard zeros (off)\n"
      "  --repeats N     Play the segment N times back-to-back (ranged"
      " fields\n"
      "                  re-draw and noise is fresh per instance; signal"
      " fixed)\n"
      "  --seed N        PRNG seed (default 0; deterministic — vary it for"
      " run-to-run change)\n"
      "  --sps N         Samples per symbol for PSK / PN (default 1)\n"
      "\n"
      "NOISE / SNR\n"
      "  --snr DB[:DB]   Add AWGN at this SNR (dB); omit to suppress noise\n"
      "  --snr-mode MODE auto | fs | ebno | esno (default auto)\n"
      "                    auto  - Es/No for PSK; full-band for tone/noise\n"
      "                    fs    - relative to full sample-rate band\n"
      "                    ebno  - Eb/No (energy per bit / noise density)\n"
      "                    esno  - Es/No (energy per symbol / noise"
      " density)\n"
      "\n"
      "PULSE SHAPING\n"
      "  --pulse SHAPE   rect | rrc (root-raised-cosine) (default rect)\n"
      "  --rrc-beta R    RRC roll-off factor 0 < R <= 1 (default 0.35)\n"
      "  --rrc-span N    RRC filter span in symbols (default 8)\n"
      "\n"
      "BITS INPUT  (--type bits)\n"
      "  --bits BITSTR   Literal bit string, e.g. \"10110010\"\n"
      "  --bits-hex HEX  Hex string, e.g. \"b2\" -> 10110010 (MSB-first)\n"
      "  --bits-file F   Binary file; bits consumed MSB-first per byte\n"
      "  --modulation M  none | bpsk | qpsk (default bpsk)\n"
      "\n"
      "SYMBOLS INPUT  (--type symbols)\n"
      "  --symbols-file F  Raw cf32 file (interleaved float32 I,Q); each\n"
      "                    sample is one constellation point. Oversampled by\n"
      "                    --sps, cycled, and RRC-shaped with --pulse rrc.\n"
      "                    Generalises any modulation (pi/4-QPSK, QAM, ...).\n"
      "\n"
      "FRAMING  (--type bits, and --type dsss below)\n"
      "  --acq-code/--sync describe a FRAME:\n"
      "      [preamble x acq-reps | sync | payload | CRC-16]\n"
      "  and setting either one is what frames the waveform (--crc alone\n"
      "  does not -- it defaults to crc16). For --type bits the payload is\n"
      "  --bits/--bits-hex/--bits-file and --modulation maps it to BPSK or\n"
      "  QPSK; the frame then CYCLES to fill --count, so one description\n"
      "  gives a multi-frame record. A frame needs an explicit payload, so\n"
      "  --type bpsk/qpsk/pn (whose symbols come from the PN LFSR) refuse\n"
      "  these flags rather than ignoring them.\n"
      "\n"
      "CHANNEL CODING  (--type bits)\n"
      "  Stages over the frame's fields, each optional, and they do NOT all\n"
      "  cover the same bits -- which is the point. A marker, a preamble and\n"
      "  a sync word are things a receiver FINDS, so they must look the same\n"
      "  in every frame: the outer code and the randomiser reach over the\n"
      "  data group only, and the inner code reaches over everything.\n"
      "  Setting any of them frames the waveform, as --sync does.\n"
      "  --rs-depth I    Reed-Solomon (255,223) E=16, interleaved I deep;\n"
      "                  1, 2, 3, 4, 5 or 8. The payload plus its CRC must\n"
      "                  be exactly 223*I octets -- a short frame is "
      "refused,\n"
      "                  not padded (virtual fill is not implemented).\n"
      "  --randomise [G] A section-10 pseudo-randomiser over the data group.\n"
      "                  Its own inverse, so the receiver runs it too.\n"
      "                    ccsds   131071-bit, h(x)=x^17+x^14+1 -- the\n"
      "                            default, and what 131.0-B-6 10.4.1 needs\n"
      "                    legacy  255-bit, h(x)=x^8+x^7+x^5+x^3+1, kept by\n"
      "                            10.4.2 for legacy systems only; it puts\n"
      "                            spectral lines at 1/255 of the symbol "
      "rate\n"
      "                    off     the same as omitting the flag\n"
      "                  NOT interchangeable on the air: only the matching\n"
      "                  receiver derandomises a given waveform.\n"
      "  --asm           Prepend the 0x1ACFFC1D attached sync marker.\n"
      "  --conv          Convolutional K=7 rate-1/2, over the WHOLE frame\n"
      "                  including the marker; doubles the bit count.\n"
      "  All four, with a 223*I-octet payload and no preamble or sync word,\n"
      "  is a CCSDS CADU. That is a configuration of these flags, not a mode\n"
      "  they switch into.\n"
      "\n"
      "DSSS BURST  (--type dsss)\n"
      "  One burst = an unmodulated repeated preamble (code A) followed by\n"
      "  the frame [sync | payload | CRC-16], every frame bit spread by a\n"
      "  second code B. The payload bits come from --bits/--bits-hex/\n"
      "  --bits-file; --sps is samples per CHIP; --count is derived (one\n"
      "  burst = n_chips * sps samples) and ignored; --snr-mode esno is the\n"
      "  Es/N0 of the outer DATA symbol (code-B chips x sps samples).\n"
      "  --acq-code BITS      Preamble code as a 0/1 string\n"
      "  --acq-code-hex HEX   Preamble code as hex (MSB-first)\n"
      "  --acq-reps N         Preamble repetitions (default 1)\n"
      "  --data-code BITS     Payload spreading code as a 0/1 string\n"
      "  --data-code-hex HEX  Payload spreading code as hex (MSB-first)\n"
      "  --sync BITS          Frame-sync word, e.g. Barker-13 (default none)\n"
      "  --crc C              none | crc16 payload trailer (default crc16)\n"
      "\n"
      "DSSS CONTINUOUS  (--type dsss --symbol-rate HZ)\n"
      "  An endless stream: code B repeats forever and data rides it at\n"
      "  --symbol-rate Hz, independent of the chip clock (non-integer\n"
      "  chips/symbol -- the asynchronicity). No preamble/sync/CRC frame;\n"
      "  --count is honoured verbatim; --snr-mode esno is the Es/N0 of the\n"
      "  data symbol (fs/symbol_rate samples). Data source: default PRBS\n"
      "  (seeded PN a receiver regenerates), --data none for code-only\n"
      "  (the pure code), or --bits* for a payload. Rejects the burst-frame\n"
      "  flags (--acq-code/--sync/--crc) and --data with --bits*.\n"
      "  --symbol-rate HZ     Data symbol rate; > 0 selects continuous mode\n"
      "  --data-code[-hex] C  Spreading code (required)\n"
      "  --data D             none | prbs data source (default prbs)\n"
      "\n"
      "PN SEQUENCE  (--type pn)\n"
      "  --pn-length N   Register length; period = 2^N - 1 (default 15)\n"
      "  --pn-poly N     Generator polynomial; 0 = auto-select (default 0)\n"
      "  --lfsr TYPE     galois | fibonacci (default galois)\n"
      "\n"
      "AMPLITUDE & CLIPPING\n"
      "  --level DB[:DB] Output level in dBFS (default 0)\n"
      "  --headroom DB   Back off composite to prevent clipping (default 0)\n"
      "  --clip-report   Print clipping fraction and peak to stderr\n"
      "  --clip-error    Exit non-zero if output clips after headroom\n"
      "\n"
      "OUTPUT\n"
      "  --output DEST   File path, - for stdout, or nats://HOST:PORT/SUBJECT"
      " (default -)\n"
      "  --sample-type T cf32 | cf64 | ci32 | ci16 | ci8 (default cf32)\n"
      "  --file-type T   raw | csv | blue | sigmf (default raw)\n"
      "  --endian E      le | be (default le)\n"
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
      "  --realtime-resync  Resync clock at each segment boundary\n"
      "  --detached      Run as a detached background process\n"
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
  free (s->bits);
  free (s->symbols);
  free (s->acq_code);
  free (s->data_code);
  free (s->sync);
  /* Nulled individually, not chained: `symbols` is float complex * while the
     rest are uint8_t *, so a chain would be an incompatible assignment. */
  s->bits      = NULL;
  s->symbols   = NULL;
  s->acq_code  = NULL;
  s->data_code = NULL;
  s->sync      = NULL;
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
  int           data_flag_set;   /* --data given (continuous dsss only) */
  int           symbol_rate_set; /* --symbol-rate given (reject <= 0) */
} wfmgen_opts_t;

/* How a flag's value is read. The enum type is used for `opt_t.kind` (rather
   than a plain int) so -Wswitch reports a kind added here with no arm in
   parse_args, instead of it silently falling through as a no-op. */
enum opt_kind
{
  OPT_SET,        /* takes no value; the destination int becomes 1        */
  OPT_STR,        /* the raw token, stored verbatim (NULL is tolerated)   */
  OPT_CHOICE,     /* one name from `tbl`; the destination int gets its index */
  OPT_CHOICE_OPT, /* OPT_CHOICE whose value may be OMITTED, and then means
                     index 1 -- so `--flag` and `--flag <name>` both work.
                     The table must therefore be ordered with the "off" or
                     absent sense at 0 and the default ON sense at 1. */
  OPT_DOUBLE,     /* strtod                                               */
  OPT_INT,        /* strtol                                               */
  OPT_SIZE,       /* strtoull -> size_t                                   */
  OPT_U32,        /* strtoul  -> uint32_t                                 */
  OPT_U64,        /* strtoull -> uint64_t                                 */
  OPT_RANGE_D,    /* LO[:HI] -> double at off, hi at aux, bit in src.ranged */
  OPT_RANGE_N,    /* LO[:HI] -> size_t at off, hi at aux, bit in seg.ranged */
  OPT_BITS,       /* "0101" -> uint8_t * at off, its length at aux        */
  OPT_HEX,        /* "a5"   -> uint8_t * at off, its bit count at aux     */
  OPT_BITS_FILE,  /* a file holding a "0101" string                       */
  OPT_SYMBOLS,    /* a raw cf32 file -> float _Complex * at off           */
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
static const opt_t OPTS[] = {
  { .name = "--from-file", .kind = OPT_STR, .off = OFF (from_file) },
  { .name = "--type",
    .kind = OPT_CHOICE,
    .off  = OFF (src.type),
    CHOICES (TYPES) },
  { .name = "--snr-mode",
    .kind = OPT_CHOICE,
    .off  = OFF (src.snr_mode),
    CHOICES (MODES) },
  { .name = "--sample-type",
    .kind = OPT_CHOICE,
    .off  = OFF (sample_type),
    CHOICES (STYPES) },
  { .name = "--file-type",
    .kind = OPT_CHOICE,
    .off  = OFF (file_type),
    CHOICES (FTYPES) },
  { .name = "--endian",
    .kind = OPT_CHOICE,
    .off  = OFF (endian),
    CHOICES (ENDIANS) },
  { .name = "--lfsr",
    .kind = OPT_CHOICE,
    .off  = OFF (src.lfsr),
    CHOICES (LFSRS) },
  { .name = "--pulse",
    .kind = OPT_CHOICE,
    .off  = OFF (src.pulse),
    CHOICES (PULSES) },
  { .name          = "--rrc-beta",
    .kind          = OPT_DOUBLE,
    .off           = OFF (src.rrc_beta),
    .unit_interval = 1 },
  { .name = "--rrc-span", .kind = OPT_INT, .off = OFF (src.rrc_span) },
  { .name = "--modulation",
    .kind = OPT_CHOICE,
    .off  = OFF (src.modulation),
    CHOICES (BITMODS) },
  { .name = "--bits",
    .kind = OPT_BITS,
    .off  = OFF (src.bits),
    .aux  = AUX (src.n_bits) },
  { .name = "--bits-hex",
    .kind = OPT_HEX,
    .off  = OFF (src.bits),
    .aux  = AUX (src.n_bits) },
  { .name = "--bits-file",
    .kind = OPT_BITS_FILE,
    .off  = OFF (src.bits),
    .aux  = AUX (src.n_bits) },
  { .name = "--acq-code",
    .kind = OPT_BITS,
    .off  = OFF (src.acq_code),
    .aux  = AUX (src.n_acq_code) },
  { .name = "--acq-code-hex",
    .kind = OPT_HEX,
    .off  = OFF (src.acq_code),
    .aux  = AUX (src.n_acq_code) },
  { .name = "--acq-reps", .kind = OPT_SIZE, .off = OFF (src.acq_reps) },
  { .name = "--data-code",
    .kind = OPT_BITS,
    .off  = OFF (src.data_code),
    .aux  = AUX (src.n_data_code) },
  { .name = "--data-code-hex",
    .kind = OPT_HEX,
    .off  = OFF (src.data_code),
    .aux  = AUX (src.n_data_code) },
  { .name = "--sync",
    .kind = OPT_BITS,
    .off  = OFF (src.sync),
    .aux  = AUX (src.n_sync) },
  { .name = "--crc",
    .kind = OPT_CHOICE,
    .off  = OFF (src.crc),
    CHOICES (CRCS) },
  /* Channel coding, as STAGES over the frame's fields. Each is separately
     optional because the standard makes it so, and they do not all cover the
     same bits -- which is the whole reason the frame is a description rather
     than a chain. See wfm/wfm_frame.h. */
  { .name = "--rs-depth", .kind = OPT_U32, .off = OFF (src.rs_depth) },
  { .name  = "--randomise",
    .alias = "--randomize",
    .kind  = OPT_CHOICE_OPT,
    .off   = OFF (src.randomise),
    CHOICES (RANDS) },
  { .name = "--asm", .kind = OPT_SET, .off = OFF (src.attach_asm) },
  { .name = "--conv", .kind = OPT_SET, .off = OFF (src.convolutional) },
  { .name = "--symbol-rate",
    .kind = OPT_DOUBLE,
    .off  = OFF (src.symbol_rate),
    .seen = SEEN (symbol_rate_set) },
  { .name = "--data",
    .kind = OPT_CHOICE,
    .off  = OFF (src.dsss_code_only),
    .seen = SEEN (data_flag_set),
    CHOICES (DATA_SRC) },
  { .name = "--symbols-file",
    .kind = OPT_SYMBOLS,
    .off  = OFF (src.symbols),
    .aux  = AUX (src.n_symbols) },
  { .name = "--fs", .kind = OPT_DOUBLE, .off = OFF (seg.fs) },
  { .name      = "--freq",
    .kind      = OPT_RANGE_D,
    .off       = OFF (src.freq),
    .aux       = AUX (src.freq_hi),
    .range_bit = WFM_RANGE_FREQ },
  { .name      = "--f-end",
    .kind      = OPT_RANGE_D,
    .off       = OFF (src.f_end),
    .aux       = AUX (src.f_end_hi),
    .range_bit = WFM_RANGE_FEND },
  { .name = "--fc", .kind = OPT_DOUBLE, .off = OFF (fc) },
  { .name      = "--snr",
    .kind      = OPT_RANGE_D,
    .off       = OFF (src.snr),
    .aux       = AUX (src.snr_hi),
    .range_bit = WFM_RANGE_SNR },
  { .name = "--seed", .kind = OPT_U32, .off = OFF (src.seed) },
  { .name = "--sps", .kind = OPT_INT, .off = OFF (src.sps) },
  { .name = "--pn-length", .kind = OPT_INT, .off = OFF (src.pn_length) },
  { .name = "--pn-poly", .kind = OPT_U64, .off = OFF (src.pn_poly) },
  { .name      = "--count",
    .kind      = OPT_RANGE_N,
    .off       = OFF (seg.num_samples),
    .aux       = AUX (seg.num_samples_hi),
    .range_bit = WFM_RANGE_NUM_SAMPLES },
  { .name      = "--off",
    .kind      = OPT_RANGE_N,
    .off       = OFF (seg.off_samples),
    .aux       = AUX (seg.off_samples_hi),
    .range_bit = WFM_RANGE_OFF_SAMPLES },
  { .name = "--repeats", .kind = OPT_SIZE, .off = OFF (seg.repeats) },
  { .name      = "--delay",
    .kind      = OPT_RANGE_N,
    .off       = OFF (seg.delay_samples),
    .aux       = AUX (seg.delay_samples_hi),
    .range_bit = WFM_RANGE_DELAY_SAMPLES },
  { .name = "--gap-noise",
    .kind = OPT_CHOICE,
    .off  = OFF (seg.gap_noise),
    CHOICES (GAPNOISE) },
  { .name = "--repeat", .kind = OPT_SET, .off = OFF (repeat) },
  { .name = "--continuous", .kind = OPT_SET, .off = OFF (continuous) },
  { .name = "--seed-advance",
    .kind = OPT_CHOICE,
    .off  = OFF (seed_advance),
    CHOICES (SEEDADV) },
  { .name = "--detached", .kind = OPT_SET, .off = OFF (detached) },
  { .name = "--realtime", .kind = OPT_SET, .off = OFF (realtime) },
  { .name      = "--level",
    .kind      = OPT_RANGE_D,
    .off       = OFF (src.level),
    .aux       = AUX (src.level_hi),
    .range_bit = WFM_RANGE_LEVEL },
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

/* Find the row matching one argv token, by long name or alias; NULL if the
   token is not a flag this CLI accepts. */
static const opt_t *
find_opt (const char *a)
{
  for (size_t k = 0; k < sizeof OPTS / sizeof *OPTS; k++)
    if (!strcmp (a, OPTS[k].name)
        || (OPTS[k].alias && !strcmp (a, OPTS[k].alias)))
      return &OPTS[k];
  return NULL;
}

/* The three bit-array flags, which differ only in where the characters come
 * from and which alphabet they are in: a literal 0/1 string, a hex string, or
 * a file holding a 0/1 string. Returns 0, or the exit code — 1 when the file
 * cannot be read, 2 when the characters are not what the flag accepts. */
static int
parse_bits_into (const opt_t *opt, const char *a, const char *v, uint8_t **dst,
                 size_t *n)
{
  char *text = NULL;
  if (opt->kind == OPT_BITS_FILE)
    {
      text = slurp_file (v);
      if (!text)
        {
          (void)fprintf (stderr, "error: cannot read %s %s\n", a, v);
          return 1;
        }
    }
  free (*dst); /* a repeated flag replaces, it does not leak */
  *dst = opt->kind == OPT_HEX ? parse_hex_string (v, n)
                              : parse_bit_string (text ? text : v, n);
  free (text);
  if (!*dst)
    {
      (void)fprintf (stderr, "error: %s expects a %s\n", a,
                     opt->kind == OPT_HEX ? "hex string" : "0/1 string");
      return 2;
    }
  return 0;
}

/* Resolve argv into `o`. Returns 0, or the exit code of the first failure —
 * 2 for a usage error, 1 for an unreadable input file.
 *
 * It RETURNS rather than exiting because a half-parsed `o` can already own
 * heap (--bits and friends): the caller's `done:` path frees it on every
 * exit, which is the fix for the five unix.Malloc leaks the old inline chain
 * carried past its 28 early returns.
 *
 * --help / -h / --version / -V never reach here; they are handled by the
 * pre-scan in doppler_wfmgen so they work regardless of the other flags.
 */
static int
parse_args (int argc, char *argv[], wfmgen_opts_t *o)
{
  char *base = (char *)o;

  for (int i = 1; i < argc; i++)
    {
      const char  *a   = argv[i];
      const opt_t *opt = find_opt (a);
      if (!opt)
        {
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
      if (opt->kind == OPT_CHOICE_OPT)
        {
          /* PEEK. The value is optional, so a following token is only ours
             if it is not another flag -- otherwise `--randomise --asm` would
             eat `--asm` and report it as a bad value. */
          if (i + 1 < argc && argv[i + 1][0] != '-')
            v = argv[++i];
        }
      else if (opt->kind != OPT_SET)
        v = (i + 1 < argc) ? argv[++i] : NULL;
      if (!v && opt->kind != OPT_SET && opt->kind != OPT_STR
          && opt->kind != OPT_CHOICE_OPT)
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

        case OPT_CHOICE_OPT:
          if (v == NULL)
            {
              *(int *)dst = 1; /* the flag alone means its default ON sense */
              break;
            }
          /* fall through: with a value it is an ordinary choice */
          /* FALLTHROUGH */
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
            double d = strtod (v, NULL);
            if (opt->unit_interval && (d <= 0.0 || d > 1.0))
              {
                (void)fprintf (stderr, "error: %s must be in (0, 1]\n", a);
                return 2;
              }
            *(double *)dst = d;
          }
          break;

        case OPT_INT:
          *(int *)dst = (int)strtol (v, NULL, 10);
          break;

        case OPT_SIZE:
          *(size_t *)dst = (size_t)strtoull (v, NULL, 10);
          break;

        case OPT_U32:
          *(uint32_t *)dst = (uint32_t)strtoul (v, NULL, 10);
          break;

        case OPT_U64:
          *(uint64_t *)dst = (uint64_t)strtoull (v, NULL, 10);
          break;

        case OPT_RANGE_D:
          {
            /* parse_range writes *hi only for a real range, so a later bare
               scalar clears the bit and leaves the stale hi unread. */
            int ranged     = 0;
            *(double *)dst = parse_range (v, (double *)aux, &ranged);
            o->src.ranged  = ranged ? (o->src.ranged | opt->range_bit)
                                    : (o->src.ranged & ~opt->range_bit);
          }
          break;

        case OPT_RANGE_N:
          {
            int    ranged  = 0;
            double hi      = 0.0;
            *(size_t *)dst = (size_t)parse_range (v, &hi, &ranged);
            *(size_t *)aux = (size_t)hi;
            o->seg.ranged  = ranged ? (o->seg.ranged | opt->range_bit)
                                    : (o->seg.ranged & ~opt->range_bit);
          }
          break;

        case OPT_BITS:
        case OPT_HEX:
        case OPT_BITS_FILE:
          {
            int bits_rc
                = parse_bits_into (opt, a, v, (uint8_t **)dst, (size_t *)aux);
            if (bits_rc)
              return bits_rc;
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
  const wfmgen_opts_t *o;
  wfm_compose_state_t *comp;
  const wfm_segment_t *segs; /* resolved, borrowed from the composer */
  size_t               n_segs;
  double               fs;      /* the capture sample rate */
  double               gain;    /* 10^(-headroom/20), the peak backoff */
  int                  endless; /* the composer resolved --continuous */
  dp_sample_clock_t   *clk;     /* NULL unless the run is real-time */
} emit_ctx_t;

/* Open a writer on `fp` and apply the run's gain and clip tracking. */
static wfm_writer_state_t *
open_writer (const emit_ctx_t *e, FILE *fp, int file_type)
{
  wfm_writer_state_t *w = wfm_writer_open (
      fp, file_type, e->o->sample_type, e->o->endian, e->fs, e->o->fc, 0, 0.0);
  if (!w)
    return NULL;
  wfm_writer_set_gain (w, e->gain);
  if (e->o->clip_report)
    wfm_writer_track_clipping (w, 1);
  return w;
}

/* Drive the composer into `w` until it runs dry; returns the sample count
 * (the BLUE header needs it, the other callers discard it).
 *
 * `paced` is a parameter rather than just `e->clk != NULL` because the
 * detached path does NOT pace and never has — see emit_detached_blue.
 */
static size_t
drain_to_writer (const emit_ctx_t *e, wfm_writer_state_t *w, int paced)
{
  float complex buf[BLK];
  size_t        n, total = 0;
  while ((n = wfm_compose_execute (e->comp, buf, BLK)) > 0)
    {
      wfm_writer_write (w, buf, n);
      total += n;
      if (paced && e->clk)
        dp_sample_clock_pace (e->clk, n);
      if (n < BLK)
        break;
      /* An interrupted capture must still be a VALID capture. The BLUE
         header carries the final sample count and is written by
         wfm_writer_close, so leaving the loop is what lets the file be
         closed properly -- killing the process here would leave a capture
         with no header at all, which is worse than a short one. */
      if (dp_interrupted ())
        break;
    }
  return total;
}

/* Close a writer, reporting (and with --clip-error, failing on) clipping. */
static int
close_writer (const emit_ctx_t *e, wfm_writer_state_t *w)
{
  int rc = report_clip (wfm_writer_peak (w), wfm_writer_clip_fraction (w),
                        e->o->sample_type, e->o->headroom, e->o->clip_report,
                        e->o->clip_error);
  wfm_writer_close (w);
  return rc ? 1 : 0;
}

/* Stream to a NATS PUB subject.
 *
 * The stream sink lives in the optional libdoppler_stream component (it pulls
 * in the vendored nats.c client). The pure-C core links only weak no-op
 * stubs, so wfm_stream_sink_available() reports 0 unless the real component
 * is linked — which is a clearer failure than silently publishing nothing. */
static int
emit_to_stream (const emit_ctx_t *e)
{
  const wfmgen_opts_t *o = e->o;
  if (!wfm_stream_sink_available ())
    {
      (void)fprintf (stderr,
                     "error: nats output (%s) requires the stream component; "
                     "this build was not linked against libdoppler_stream\n",
                     o->out_path);
      return 1;
    }
  wfm_stream_sink_t *sink = wfm_stream_sink_open (o->out_path, o->sample_type);
  if (!sink)
    {
      (void)fprintf (stderr, "error: cannot open stream sink %s\n",
                     o->out_path);
      return 1;
    }
  wfm_stream_sink_set_gain (sink, e->gain);
  if (o->clip_report)
    wfm_stream_sink_track_clipping (sink, 1);

  float complex buf[BLK];
  size_t        n;
  while ((n = wfm_compose_execute (e->comp, buf, BLK)) > 0)
    {
      wfm_stream_sink_send (sink, buf, n, e->fs, o->fc);
      if (e->clk)
        dp_sample_clock_pace (e->clk, n);
      if (n < BLK)
        break;
      if (dp_interrupted ())
        break;
    }
  int rc = report_clip (wfm_stream_sink_peak (sink),
                        wfm_stream_sink_clip_fraction (sink), o->sample_type,
                        o->headroom, o->clip_report, o->clip_error);

  /* Say the stream has ended BEFORE draining. The order matters: a drain
     cannot be reversed and refuses sends once it reaches its
     publish-flushing phase, so an EOS issued after one may simply not go.
     Without this a subscriber has only silence to go on, and silence is
     exactly what it cannot interpret. */
  (void)wfm_stream_sink_send_eos (sink);

  /* Drain BEFORE close, on every exit -- interrupted or finished. A send
     returns once the client has the block, not once the server does, so
     closing without this leaves the tail to the client's own best-effort
     flush: 500 ms, no failure report, silently dropped beyond that. The
     budget is reported rather than swallowed, because "wfmgen exited 0" has
     to mean the samples arrived. */
  int drc = wfm_stream_sink_drain (sink, 0);
  if (drc != DP_OK)
    {
      (void)fprintf (stderr,
                     "wfmgen: stream did not drain (error %d) -- the tail "
                     "may not have reached the server\n",
                     drc);
      rc = 1;
    }
  wfm_stream_sink_close (sink);
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
  if (e->endless)
    {
      (void)fprintf (stderr, "error: --detached requires finite output "
                             "(not --continuous)\n");
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

  int                 rc    = 0;
  size_t              total = 0;
  wfm_writer_state_t *w     = open_writer (e, df, WFM_FT_RAW);
  if (w)
    {
      /* Unpaced, unlike the stream and file paths: --realtime has never
         applied to a detached run. Preserved verbatim here rather than
         quietly unified while extracting — see gh-725. */
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
  wfm_blue_write_hcb (hf, o->sample_type, o->endian, e->fs, o->fc, 0.0, total,
                      1, 0.0);
  (void)fclose (hf);
  return rc;
}

/* The .sigmf-meta sidecar, from the resolved spans. Best-effort: a capture
 * whose data was written is not failed by an unwritable sidecar. */
static void
write_sigmf_meta (const emit_ctx_t *e)
{
  char *meta = wfm_sigmf_meta_json (e->o->sample_type, e->o->endian, e->fs,
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

  int                 rc = 0;
  wfm_writer_state_t *w
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
  char *json
      = wfm_spec_to_json (e->segs, e->n_segs, repeating, e->endless,
                          wfm_compose_seed_advance (e->comp), e->o->headroom);
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

/* Continuous-DSSS flag consistency, for a run built from the flags rather
 * than from a spec file. Every one of these rejects rather than silently
 * ignoring: the flags below are meaningless outside continuous DSSS, and a
 * knob that does nothing in the mode you are in is the worse failure (the
 * --detached precedent). Returns 0, or the usage exit code.
 */
static int
check_continuous_dsss (const wfmgen_opts_t *o)
{
  if (o->symbol_rate_set && o->src.symbol_rate <= 0.0)
    {
      (void)fprintf (stderr, "error: --symbol-rate must be positive (it is "
                             "the continuous-dsss data symbol rate in Hz)\n");
      return 2;
    }
  if (o->src.symbol_rate <= 0.0)
    {
      if (o->data_flag_set)
        {
          (void)fprintf (
              stderr, "error: --data selects the continuous-dsss data source; "
                      "it needs --symbol-rate\n");
          return 2;
        }
      return 0;
    }
  if (o->src.type != WFM_SYNTH_DSSS)
    {
      (void)fprintf (stderr, "error: --symbol-rate is only for --type dsss\n");
      return 2;
    }
  if (!o->src.data_code)
    {
      (void)fprintf (stderr, "error: continuous dsss (--symbol-rate) needs "
                             "--data-code\n");
      return 2;
    }
  if (o->src.acq_code || o->src.sync)
    {
      (void)fprintf (stderr, "error: --acq-code/--sync are burst-frame flags, "
                             "meaningless with --symbol-rate\n");
      return 2;
    }
  if (o->data_flag_set && o->src.bits)
    {
      (void)fprintf (stderr,
                     "error: --data and --bits both set the data; use one\n");
      return 2;
    }
  return 0;
}

/**
 * @brief Refuse a frame this waveform type cannot carry — with the reason.
 *
 * The rule itself is `wfm_source_frame_error()`, shared with the standalone
 * Synth and the composer so all three faces answer identically. What this adds
 * is the CLI's half of the contract: a named exit code and the reason on
 * stderr, rather than the generic build failure a NULL from
 * `wfm_compose_create()` produces. Both refuse; only one of them tells you
 * what to do instead.
 */
static int
check_frame (const wfmgen_opts_t *o)
{
  const char *why = wfm_source_frame_error (&o->src);
  if (!why)
    return 0;
  (void)fprintf (stderr, "error: %s\n", why);
  return 2;
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
  char *json = wfm_spec_template_json ();
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
int
doppler_wfmgen (int argc, char *argv[])
{
  wfm_compose_state_t *comp = NULL; /* wfm_compose_destroy tolerates NULL */
  int                  rc   = 0;

  /* FIRST, before parsing or opening anything. A signal arriving before this
   * is not ignored, it terminates the process -- and the window is real:
   * measured at ~5 ms for a dynamically linked binary, which is long enough
   * for a supervisor's stop signal to land inside it. Installing here rather
   * than beside the emit loop is the difference between "Ctrl+C is handled"
   * and "Ctrl+C is handled once we get that far".
   *
   * Both signals, because a container runtime sends SIGTERM and a terminal
   * sends SIGINT, and losing the tail should not depend on which. */
  (void)dp_interrupt_on_signal (SIGINT);
  (void)dp_interrupt_on_signal (SIGTERM);

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
     frequencies are normalised (cycles/sample) out of the box. These mirror
     the Python Synth/Composer defaults (just-makeit.toml) so `wfmgen` and
     `Synth()` agree sample-for-sample. Every other field is zero, which is
     the default the option table's rows are written against. */
  wfmgen_opts_t o
      = { .src = { .sps        = 1,
                   .snr        = 100.0,
                   .pn_length  = 15,
                   .modulation = 1, /* bits: default bpsk */
                   .rrc_beta   = 0.35,
                   .rrc_span   = 8,
                   .acq_reps   = 1,   /* dsss: one preamble */
                   .crc        = 1 }, /* dsss: crc16 trailer */
          .seg = { .n_sources = 1, .fs = 1.0, .num_samples = 1024 } };
  /* Set after the initialiser, not inside it: `&o.src` is the address of a
     sibling member of the very object being initialised. */
  o.seg.sources = &o.src;

  rc = parse_args (argc, argv, &o);
  if (rc)
    goto done;

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
      comp = wfm_compose_from_json (spec);
      if (!o.headroom_set)
        o.headroom = wfm_spec_headroom (spec);
      free (spec);
    }
  else
    {
      rc = check_continuous_dsss (&o);
      if (rc)
        goto done;
      rc = check_frame (&o);
      if (rc)
        goto done;
      comp = wfm_compose_create (&o.seg, 1, o.repeat, o.continuous);
      wfm_compose_set_seed_advance (comp, o.seed_advance);
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
  const wfm_segment_t *segs = wfm_compose_segments (comp, &n_segs, &r, &c);
  double               fs   = n_segs ? segs[0].fs : o.seg.fs;

  /* Real-time pacing: throttle the emit loop to fs, mimicking a sample clock
     driving the output. Anchored once here so the schedule is drift-free; a
     NULL clk in the context below is what "not real-time" means downstream. */
  dp_sample_clock_t clk = { 0 }; /* the underrun report reads it either way */
  if (o.realtime)
    dp_sample_clock_init (&clk, fs, o.realtime_resync);

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
     ours to release, and wfm_compose_destroy tolerates the NULL `comp` an
     exit taken before the composer was built leaves behind. */
done:
  wfm_compose_destroy (comp);
  source_free (&o.src);
  return rc;
}
