/*
 * wfmgen.c — the waveform-generator composer CLI (Phase C, hand-written).
 *
 * The rich sibling of the generated `wavegen` single-shot tool: it sequences
 * multi-segment specs (`--from-file`), emits any output container
 * (raw/csv/BLUE/SigMF, `--file-type`) in any wire type / byte order, streams
 * to a file, stdout, or a ZMQ PUB endpoint (`--output zmq://…`), and writes a
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> /* isatty */

#include "doppler/version.h" /* DOPPLER_VERSION (configure-time stamp) */
#include "timing/timing_core.h"
#include "wfm/wfm_compose.h"
#include "wfm/wfm_sink.h"
#include "wfm/wfm_writer.h"
#include "wfm/wfmgen.h"

#define BLK 4096

static const char *const TYPES[]
    = { "tone", "noise", "pn", "bpsk", "qpsk", "chirp", "bits" };
static const char *const MODES[]   = { "auto", "fs", "ebno", "esno" };
static const char *const BITMODS[] = { "none", "bpsk", "qpsk" };
static const char *const STYPES[]  = { "cf32", "cf64", "ci32", "ci16", "ci8" };
static const char *const FTYPES[]  = { "raw", "csv", "blue", "sigmf" };
static const char *const ENDIANS[] = { "le", "be" };
static const char *const LFSRS[]   = { "galois", "fibonacci" };
static const char *const PULSES[]  = { "rect", "rrc" };

/* Look name up in a NULL-free table of n entries; -1 if absent. */
static int
lookup (const char *s, const char *const *tbl, int n)
{
  for (int i = 0; i < n; i++)
    if (!strcmp (s, tbl[i]))
      return i;
  return -1;
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
      "scale.\n  remedy: --headroom %d, or --sample_type cf32.\n",
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
      "  wfmgen [OPTIONS] [--output FILE|-|zmq://HOST:PORT]\n"
      "  wfmgen json-template [FILE]\n"
      "\n"
      "WAVEFORM TYPE\n"
      "  --type TYPE     tone | noise | pn | bpsk | qpsk | chirp | bits\n"
      "                    tone  - pure CW carrier at --freq\n"
      "                    noise - Gaussian white noise\n"
      "                    pn    - pseudo-random MLS sequence\n"
      "                    bpsk  - BPSK-modulated symbols\n"
      "                    qpsk  - QPSK-modulated symbols\n"
      "                    chirp - linear sweep, --freq to --f_end\n"
      "                    bits  - custom bit pattern (see BITS INPUT)\n"
      "\n"
      "SIGNAL PARAMETERS\n"
      "  --fs HZ         Sample rate (default 1.0; freq treated as"
      " normalised)\n"
      "  --freq HZ       Carrier / sweep-start frequency (default 0.0)\n"
      "  --f_end HZ      Chirp sweep-end frequency (chirp only)\n"
      "  --fc HZ         Centre frequency stored in SigMF metadata only\n"
      "  --count N       Samples to generate (default 1024)\n"
      "  --off N         Skip N samples before writing output\n"
      "  --seed N        PRNG seed (default 0; deterministic — vary it for"
      " run-to-run change)\n"
      "  --sps N         Samples per symbol for PSK / PN (default 1)\n"
      "\n"
      "NOISE / SNR\n"
      "  --snr DB        Add AWGN at this SNR (dB); omit to suppress noise\n"
      "  --snr_mode MODE auto | fs | ebno | esno (default auto)\n"
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
      "PN SEQUENCE  (--type pn)\n"
      "  --pn_length N   Register length; period = 2^N - 1 (default 15)\n"
      "  --pn_poly N     Generator polynomial; 0 = auto-select (default 0)\n"
      "  --lfsr TYPE     galois | fibonacci (default galois)\n"
      "\n"
      "AMPLITUDE & CLIPPING\n"
      "  --level DB      Output level in dBFS (default 0)\n"
      "  --headroom DB   Back off composite to prevent clipping (default 0)\n"
      "  --clip-report   Print clipping fraction and peak to stderr\n"
      "  --clip-error    Exit non-zero if output clips after headroom\n"
      "\n"
      "OUTPUT\n"
      "  --output DEST   File path, - for stdout, or zmq://HOST:PORT"
      " (default -)\n"
      "  --sample_type T cf32 | cf64 | ci32 | ci16 | ci8 (default cf32)\n"
      "  --file_type T   raw | csv | blue | sigmf (default raw)\n"
      "  --endian E      le | be (default le)\n"
      "  --record FILE   Write a JSON record of the resolved run to FILE\n"
      "\n"
      "COMPOSITION\n"
      "  --from-file F   Load a multi-segment JSON scene (overrides signal"
      " flags)\n"
      "  --repeat        Loop the spec indefinitely\n"
      "  --continuous    Stream continuously (no defined end)\n"
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
      "         --snr 10 --snr_mode ebno --count 16384"
      " --output burst.cf32\n"
      "\n"
      "  # QPSK stream to ZMQ, real-time paced at 2 MHz\n"
      "  wfmgen --type qpsk --sps 8 --fs 2e6 \\\n"
      "         --output zmq://127.0.0.1:5555 --continuous --realtime\n"
      "\n"
      "  # Multi-segment scene from a JSON spec\n"
      "  wfmgen json-template scene.json  # generate skeleton\n"
      "  wfmgen --from-file scene.json --output scene.cf32\n";

/* The CLI's whole body lives here as a plain callable (argv in, exit-code out)
 * so it can be archived into libdoppler and invoked by a downstream linker —
 * the `wfmgen` binary is a one-line `main` shim over it (wfmgen_main.c).
 *
 * It is a flat, single-pass argv dispatcher: one else-if arm per flag. The
 * length is inherent to the surface area, and splitting the chain across
 * helpers would scatter the parse with no readability gain — hence the
 * function-size suppression. */
int
doppler_wfmgen (int   argc, /* NOLINT(readability-function-size) */
                char *argv[])
{
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

  /* `wfmgen json-template [FILE]` — emit a ready-to-edit example spec in the
   * canonical --from-file schema, then exit. Writes to FILE, or to stdout when
   * FILE is absent or "-". JSON is text, so the binary-to-tty guard below does
   * not apply (printing it to a terminal is fine). */
  if (argc >= 2 && !strcmp (argv[1], "json-template"))
    {
      const char *tpl_path
          = (argc >= 3 && strcmp (argv[2], "-") != 0) ? argv[2] : NULL;
      char *json = wfm_spec_template_json ();
      if (!json)
        {
          (void)fprintf (stderr,
                         "error: out of memory building the template\n");
          return 1;
        }
      FILE *tf = tpl_path ? fopen (tpl_path, "wb") : stdout;
      if (!tf)
        {
          (void)fprintf (stderr, "error: cannot open %s for writing\n",
                         tpl_path);
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

  /* Single-segment defaults: one source in one segment. fs = 1.0 means
     frequencies are normalised (cycles/sample) out of the box. These mirror
     the Python Synth/Composer defaults (just-makeit.toml) so `wfmgen` and
     `Synth()` agree sample-for-sample. */
  wfm_source_t  src    = { .type       = 0,
                           .freq       = 0.0,
                           .snr        = 100.0,
                           .snr_mode   = 0,
                           .seed       = 0,
                           .sps        = 1,
                           .pn_length  = 15,
                           .pn_poly    = 0,
                           .modulation = 1, /* bits: default bpsk */
                           .rrc_beta   = 0.35,
                           .rrc_span   = 8 };
  wfm_segment_t seg    = { .sources     = &src,
                           .n_sources   = 1,
                           .fs          = 1.0,
                           .num_samples = 1024,
                           .off_samples = 0 };
  int           repeat = 0, continuous = 0, detached = 0;
  int           realtime = 0, realtime_resync = 0;
  int           clip_report = 0, clip_error = 0;
  double        headroom     = 0.0; /* dB of peak backoff; gain = 10^(-H/20) */
  int           headroom_set = 0; /* explicit --headroom overrides a record */
  int           sample_type = 0, file_type = 0, endian = 0;
  double        fc        = 0.0;
  const char   *from_file = NULL, *out_path = NULL, *record_path = NULL;

  /* NEXT() yields the flag's value argument, or NULL when the flag was the
     last token on the line. Value-taking flags must reject that NULL rather
     than hand it to strtod/strtol (which is undefined and segfaults in
     practice): REQVAL binds `v` to the value or bails with a usage error. */
#define NEXT() (i + 1 < argc ? argv[++i] : NULL)
#define REQVAL(v)                                                             \
  const char *v = NEXT ();                                                    \
  if (!(v))                                                                   \
    {                                                                         \
      (void)fprintf (stderr, "error: %s requires a value\n", a);              \
      return 2;                                                               \
    }
#define CHOICE(dst, tbl)                                                      \
  do                                                                          \
    {                                                                         \
      const char *v = NEXT ();                                                \
      int idx = v ? lookup (v, (tbl), (int)(sizeof (tbl) / sizeof (*(tbl))))  \
                  : -1;                                                       \
      if (idx < 0)                                                            \
        {                                                                     \
          (void)fprintf (stderr, "error: bad value for %s\n", a);             \
          return 2;                                                           \
        }                                                                     \
      (dst) = idx;                                                            \
    }                                                                         \
  while (0)

  for (int i = 1; i < argc; i++)
    {
      const char *a = argv[i];
      /* --help / -h / --version / -V are handled by the pre-scan above. */
      if (!strcmp (a, "--from-file"))
        {
          from_file = NEXT ();
        }
      else if (!strcmp (a, "--type"))
        {
          CHOICE (src.type, TYPES);
        }
      else if (!strcmp (a, "--snr_mode"))
        {
          CHOICE (src.snr_mode, MODES);
        }
      else if (!strcmp (a, "--sample_type"))
        {
          CHOICE (sample_type, STYPES);
        }
      else if (!strcmp (a, "--file_type"))
        {
          CHOICE (file_type, FTYPES);
        }
      else if (!strcmp (a, "--endian"))
        {
          CHOICE (endian, ENDIANS);
        }
      else if (!strcmp (a, "--lfsr"))
        {
          CHOICE (src.lfsr, LFSRS);
        }
      else if (!strcmp (a, "--pulse"))
        {
          CHOICE (src.pulse, PULSES);
        }
      else if (!strcmp (a, "--rrc-beta"))
        {
          REQVAL (v);
          src.rrc_beta = strtod (v, NULL);
          if (src.rrc_beta <= 0.0 || src.rrc_beta > 1.0)
            {
              (void)fprintf (stderr, "error: --rrc-beta must be in (0, 1]\n");
              return 2;
            }
        }
      else if (!strcmp (a, "--rrc-span"))
        {
          REQVAL (v);
          src.rrc_span = (int)strtol (v, NULL, 10);
        }
      else if (!strcmp (a, "--modulation"))
        {
          CHOICE (src.modulation, BITMODS);
        }
      else if (!strcmp (a, "--bits"))
        {
          REQVAL (v);
          free (src.bits);
          src.bits = parse_bit_string (v, &src.n_bits);
          if (!src.bits)
            {
              (void)fprintf (stderr, "error: --bits expects a 0/1 string\n");
              return 2;
            }
        }
      else if (!strcmp (a, "--bits-hex"))
        {
          REQVAL (v);
          free (src.bits);
          src.bits = parse_hex_string (v, &src.n_bits);
          if (!src.bits)
            {
              (void)fprintf (stderr,
                             "error: --bits-hex expects a hex string\n");
              return 2;
            }
        }
      else if (!strcmp (a, "--bits-file"))
        {
          REQVAL (v);
          char *text = slurp_file (v);
          if (!text)
            {
              (void)fprintf (stderr, "error: cannot read --bits-file %s\n", v);
              return 1;
            }
          free (src.bits);
          src.bits = parse_bit_string (text, &src.n_bits);
          free (text);
          if (!src.bits)
            {
              (void)fprintf (stderr,
                             "error: --bits-file must contain a 0/1 string\n");
              return 2;
            }
        }
      else if (!strcmp (a, "--fs"))
        {
          REQVAL (v);
          seg.fs = strtod (v, NULL);
        }
      else if (!strcmp (a, "--freq"))
        {
          REQVAL (v);
          src.freq = strtod (v, NULL);
        }
      else if (!strcmp (a, "--f_end"))
        {
          REQVAL (v);
          src.f_end = strtod (v, NULL); /* chirp end frequency */
        }
      else if (!strcmp (a, "--fc"))
        {
          REQVAL (v);
          fc = strtod (v, NULL);
        }
      else if (!strcmp (a, "--snr"))
        {
          REQVAL (v);
          src.snr = strtod (v, NULL);
        }
      else if (!strcmp (a, "--seed"))
        {
          REQVAL (v);
          src.seed = (uint32_t)strtoul (v, NULL, 10);
        }
      else if (!strcmp (a, "--sps"))
        {
          REQVAL (v);
          src.sps = (int)strtol (v, NULL, 10);
        }
      else if (!strcmp (a, "--pn_length"))
        {
          REQVAL (v);
          src.pn_length = (int)strtol (v, NULL, 10);
        }
      else if (!strcmp (a, "--pn_poly"))
        {
          REQVAL (v);
          src.pn_poly = (uint64_t)strtoull (v, NULL, 10);
        }
      else if (!strcmp (a, "--count"))
        {
          REQVAL (v);
          seg.num_samples = (size_t)strtoull (v, NULL, 10);
        }
      else if (!strcmp (a, "--off"))
        {
          REQVAL (v);
          seg.off_samples = (size_t)strtoull (v, NULL, 10);
        }
      else if (!strcmp (a, "--repeat"))
        {
          repeat = 1;
        }
      else if (!strcmp (a, "--continuous"))
        {
          continuous = 1;
        }
      else if (!strcmp (a, "--detached"))
        {
          detached = 1;
        }
      else if (!strcmp (a, "--realtime"))
        {
          realtime = 1;
        }
      else if (!strcmp (a, "--level"))
        {
          REQVAL (v);
          src.level = strtod (v, NULL);
        }
      else if (!strcmp (a, "--headroom"))
        {
          REQVAL (v);
          headroom     = strtod (v, NULL);
          headroom_set = 1;
        }
      else if (!strcmp (a, "--clip-report"))
        {
          clip_report = 1;
        }
      else if (!strcmp (a, "--clip-error"))
        {
          clip_error = 1;
        }
      else if (!strcmp (a, "--realtime-resync"))
        {
          realtime        = 1;
          realtime_resync = 1;
        }
      else if (!strcmp (a, "--output") || !strcmp (a, "-o"))
        {
          out_path = NEXT ();
        }
      else if (!strcmp (a, "--record"))
        {
          record_path = NEXT ();
        }
      else
        {
          (void)fprintf (stderr, "error: unknown option '%s' (try --help)\n",
                         a);
          return 2;
        }
    }

  /* Build the composer: from a JSON spec, or the single-segment flags. A
     recorded --headroom rides in the spec file and is reapplied here unless
     an explicit --headroom on this run overrides it. */
  wfm_compose_state_t *comp;
  if (from_file)
    {
      char *spec = slurp_file (from_file);
      if (!spec)
        {
          (void)fprintf (stderr, "error: could not read %s\n", from_file);
          return 1;
        }
      comp = wfm_compose_from_json (spec);
      if (!headroom_set)
        headroom = wfm_spec_headroom (spec);
      free (spec);
    }
  else
    comp = wfm_compose_create (&seg, 1, repeat, continuous);
  if (!comp)
    {
      (void)fprintf (stderr, "error: could not build the waveform spec\n");
      return 1;
    }

  /* Borrow the resolved segments (for --record / SigMF) + the capture fs. */
  size_t               n_segs = 0;
  int                  r = 0, c = 0;
  const wfm_segment_t *segs = wfm_compose_segments (comp, &n_segs, &r, &c);
  double               fs   = n_segs ? segs[0].fs : seg.fs;

  if (record_path)
    {
      char *json = wfm_spec_to_json (segs, n_segs, r, c, headroom);
      if (json)
        {
          FILE *rf = fopen (record_path, "w");
          if (rf)
            {
              (void)fputs (json, rf);
              (void)fputc ('\n', rf);
              (void)fclose (rf);
            }
          free (json);
        }
    }

  /* Real-time pacing: throttle the emit loop to fs, mimicking a sample clock
     driving the output. Anchored once here so the schedule is drift-free. */
  dp_sample_clock_t clk;
  if (realtime)
    dp_sample_clock_init (&clk, fs, realtime_resync);

  int           rc   = 0;
  double        gain = pow (10.0, -headroom / 20.0); /* headroom backoff */
  float complex buf[BLK];
  size_t        n;

  if (out_path && !strncmp (out_path, "zmq://", 6)
      && !wfm_zmq_sink_available ())
    {
      /* The ZMQ sink lives in the optional libdoppler_stream component (it
         pulls in the vendored C++ libzmq).  The pure-C core links only weak
         no-op stubs; wfm_zmq_sink_available() reports 0 unless the real
         component is linked (it provides the strong override). */
      (void)fprintf (
          stderr,
          "error: zmq output (%s) requires the stream component; this "
          "build was not linked against libdoppler_stream\n",
          out_path);
      rc = 1;
    }
  else if (out_path && !strncmp (out_path, "zmq://", 6))
    {
      /* stream to a ZMQ PUB endpoint */
      wfm_zmq_sink_t *sink = wfm_zmq_sink_open (out_path + 6, sample_type);
      if (!sink)
        {
          (void)fprintf (stderr, "error: cannot open zmq sink %s\n", out_path);
          rc = 1;
        }
      else
        {
          wfm_zmq_sink_set_gain (sink, gain);
          if (clip_report)
            wfm_zmq_sink_track_clipping (sink, 1);
          while ((n = wfm_compose_execute (comp, buf, BLK)) > 0)
            {
              wfm_zmq_sink_send (sink, buf, n, fs, fc);
              if (realtime)
                dp_sample_clock_pace (&clk, n);
              if (n < BLK)
                break;
            }
          if (report_clip (wfm_zmq_sink_peak (sink),
                           wfm_zmq_sink_clip_fraction (sink), sample_type,
                           headroom, clip_report, clip_error))
            rc = 1;
          wfm_zmq_sink_close (sink);
        }
    }
  else if (file_type == 2 && detached)
    {
      /* BLUE detached: raw data → <out>.det, full HCB → <out>.hdr. */
      if (!out_path)
        {
          (void)fprintf (stderr, "error: --detached needs --output\n");
          wfm_compose_destroy (comp);
          return 2;
        }
      if (c)
        {
          (void)fprintf (stderr, "error: --detached requires finite output "
                                 "(not --continuous)\n");
          wfm_compose_destroy (comp);
          return 2;
        }
      char det_path[1024];
      if (build_path (det_path, sizeof det_path, out_path, ".det") != 0)
        {
          (void)fprintf (stderr, "error: output path too long\n");
          wfm_compose_destroy (comp);
          return 2;
        }
      FILE *df = fopen (det_path, "wb");
      if (!df)
        {
          (void)fprintf (stderr, "error: cannot open %s\n", det_path);
          rc = 1;
        }
      else
        {
          wfm_writer_t *w     = wfm_writer_open (df, WFM_FT_RAW, sample_type,
                                                 endian, fs, fc, 0);
          size_t        total = 0;
          if (w)
            {
              wfm_writer_set_gain (w, gain);
              if (clip_report)
                wfm_writer_track_clipping (w, 1);
              while ((n = wfm_compose_execute (comp, buf, BLK)) > 0)
                {
                  wfm_writer_write (w, buf, n);
                  total += n;
                  if (n < BLK)
                    break;
                }
              if (report_clip (wfm_writer_peak (w),
                               wfm_writer_clip_fraction (w), sample_type,
                               headroom, clip_report, clip_error))
                rc = 1;
              wfm_writer_close (w);
            }
          (void)fclose (df);
          char  hdr_path[1024];
          FILE *hf = build_path (hdr_path, sizeof hdr_path, out_path, ".hdr")
                         ? NULL
                         : fopen (hdr_path, "wb");
          if (hf)
            {
              wfm_blue_write_hcb (hf, sample_type, endian, fs, fc, 0.0, total,
                                  1);
              (void)fclose (hf);
            }
          else
            {
              rc = 1;
            }
        }
    }
  else
    {
      /* file / stdout container. SigMF writes <base>.sigmf-data + -meta. */
      int   sigmf = (file_type == 3);
      FILE *fp;
      char  data_path[1024];
      if (sigmf)
        {
          if (!out_path)
            {
              (void)fprintf (stderr,
                             "error: --file_type sigmf needs --output\n");
              wfm_compose_destroy (comp);
              return 2;
            }
          if (build_path (data_path, sizeof data_path, out_path, ".sigmf-data")
              != 0)
            {
              (void)fprintf (stderr, "error: output path too long\n");
              wfm_compose_destroy (comp);
              return 2;
            }
          fp = fopen (data_path, "wb");
        }
      else
        {
          /* Refuse to spew raw binary IQ onto an interactive terminal (the
           * footgun when --output is forgotten — `wfmgen` alone defaults to
           * raw to stdout). An explicit `--output -` is stdout too, so it
           * must trip the same guard. CSV is human-readable text so it is
           * allowed; piping/redirecting stdout (not a tty) is always allowed.
           */
          int to_stdout = !out_path || !strcmp (out_path, "-");
          if (to_stdout && file_type != WFM_FT_CSV && isatty (fileno (stdout)))
            {
              (void)fprintf (
                  stderr, "error: refusing to write binary IQ to a terminal — "
                          "pass --output FILE (or redirect/pipe stdout)\n\n");
              (void)fputs (USAGE, stderr);
              wfm_compose_destroy (comp);
              return 1;
            }
          fp = to_stdout ? stdout : fopen (out_path, "wb");
        }
      if (!fp)
        {
          (void)fprintf (stderr, "error: cannot open output\n");
          wfm_compose_destroy (comp);
          return 1;
        }
      int           wft = sigmf ? WFM_FT_RAW : file_type;
      wfm_writer_t *w
          = wfm_writer_open (fp, wft, sample_type, endian, fs, fc, 0);
      if (!w)
        {
          (void)fprintf (stderr, "error: cannot open writer\n");
          rc = 1;
        }
      else
        {
          wfm_writer_set_gain (w, gain);
          if (clip_report)
            wfm_writer_track_clipping (w, 1);
          while ((n = wfm_compose_execute (comp, buf, BLK)) > 0)
            {
              wfm_writer_write (w, buf, n);
              if (realtime)
                dp_sample_clock_pace (&clk, n);
              if (n < BLK)
                break;
            }
          if (report_clip (wfm_writer_peak (w), wfm_writer_clip_fraction (w),
                           sample_type, headroom, clip_report, clip_error))
            rc = 1;
          wfm_writer_close (w);
        }
      if (fp != stdout)
        (void)fclose (fp);

      if (sigmf && rc == 0)
        {
          char *meta = wfm_sigmf_meta_json (sample_type, endian, fs, fc, segs,
                                            n_segs);
          if (meta)
            {
              char  meta_path[1024];
              FILE *mf = build_path (meta_path, sizeof meta_path, out_path,
                                     ".sigmf-meta")
                             ? NULL
                             : fopen (meta_path, "w");
              if (mf)
                {
                  (void)fputs (meta, mf);
                  (void)fclose (mf);
                }
              free (meta);
            }
        }
    }

  if (realtime && clk.underruns)
    (void)fprintf (
        stderr, "wfmgen: %llu underrun(s) — worst %.3f ms behind real time\n",
        (unsigned long long)clk.underruns, (double)clk.max_late_ns / 1e6);

  wfm_compose_destroy (comp);
  free (src.bits); /* the composer deep-copied it; free our CLI-owned copy */
  return rc;
}
