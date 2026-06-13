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
        fprintf (stderr, "wfmgen: peak %.1f dBFS — no clipping\n", dbfs);
      return 0;
    }
  /* peak is *after* any --headroom; total backoff to fit it = current + over.
   */
  int need = (int)ceil (headroom + dbfs);
  fprintf (stderr,
           "wfmgen: warning: %s output clipped — peak is +%.1f dB over full "
           "scale.\n  remedy: --headroom %d, or --sample_type cf32.\n",
           STYPES[stype], dbfs, need);
  if (clip_report)
    fprintf (stderr, "  clipped %.2f%% of I/Q components\n", 100.0 * frac);
  return clip_error ? 1 : 0;
}

/* Read a whole file into a malloc'd NUL-terminated string (caller frees). */
static char *
slurp_file (const char *path)
{
  FILE *f = fopen (path, "rb");
  if (!f)
    return NULL;
  fseek (f, 0, SEEK_END);
  long len = ftell (f);
  if (len < 0)
    {
      fclose (f);
      return NULL;
    }
  rewind (f);
  char *buf = malloc ((size_t)len + 1);
  if (!buf)
    {
      fclose (f);
      return NULL;
    }
  size_t rd = fread (buf, 1, (size_t)len, f);
  fclose (f);
  buf[rd] = '\0';
  return buf;
}

static const char USAGE[]
    = "usage: wfmgen [--from-file SPEC.json] [--type "
      "tone|noise|pn|bpsk|qpsk|chirp|bits]\n"
      "  [--pulse rect|rrc] [--rrc-beta R] [--rrc-span N]\n"
      "  [--fs HZ] [--freq HZ] [--f_end HZ] [--fc HZ] [--snr DB] [--snr_mode "
      "auto|fs|ebno|esno]\n"
      "  [--seed N] [--sps N] [--pn_length N] [--pn_poly N] "
      "[--lfsr galois|fibonacci]\n"
      "  [--bits 0/1-STRING | --bits-hex HEX | --bits-file FILE] "
      "[--modulation none|bpsk|qpsk]\n"
      "  [--count N] [--off N] [--repeat] [--continuous]\n"
      "  [--sample_type cf32|cf64|ci32|ci16|ci8] [--file_type "
      "raw|csv|blue|sigmf]\n"
      "  [--endian le|be] [--detached] [--realtime] [--realtime-resync]\n"
      "  [--level DB] [--headroom DB] [--clip-report] [--clip-error]\n"
      "  [--output FILE|zmq://EP] [--record FILE]\n";

/* The CLI's whole body lives here as a plain callable (argv in, exit-code out)
 * so it can be archived into libdoppler and invoked by a downstream linker —
 * the `wfmgen` binary is a one-line `main` shim over it (wfmgen_main.c). */
int
doppler_wfmgen (int argc, char *argv[])
{
  /* single-segment defaults mirror synth/wavegen: one source in one segment */
  wfm_source_t  src    = { .type       = 0,
                           .freq       = 0.0,
                           .snr        = 100.0,
                           .snr_mode   = 0,
                           .seed       = 1,
                           .sps        = 8,
                           .pn_length  = 7,
                           .pn_poly    = 0,
                           .modulation = 1, /* bits: default bpsk */
                           .rrc_beta   = 0.35,
                           .rrc_span   = 8 };
  wfm_segment_t seg    = { .sources     = &src,
                           .n_sources   = 1,
                           .fs          = 1e6,
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

#define NEXT() (i + 1 < argc ? argv[++i] : NULL)
#define CHOICE(dst, tbl)                                                      \
  do                                                                          \
    {                                                                         \
      const char *v = NEXT ();                                                \
      int         idx                                                         \
          = v ? lookup (v, tbl, (int)(sizeof (tbl) / sizeof (*tbl))) : -1;    \
      if (idx < 0)                                                            \
        {                                                                     \
          fprintf (stderr, "error: bad value for %s\n", a);                   \
          return 2;                                                           \
        }                                                                     \
      dst = idx;                                                              \
    }                                                                         \
  while (0)

  for (int i = 1; i < argc; i++)
    {
      const char *a = argv[i];
      if (!strcmp (a, "--help") || !strcmp (a, "-h"))
        {
          fputs (USAGE, stdout);
          return 0;
        }
      else if (!strcmp (a, "--from-file"))
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
          src.rrc_beta = strtod (NEXT (), NULL);
        }
      else if (!strcmp (a, "--rrc-span"))
        {
          src.rrc_span = (int)strtol (NEXT (), NULL, 10);
        }
      else if (!strcmp (a, "--modulation"))
        {
          CHOICE (src.modulation, BITMODS);
        }
      else if (!strcmp (a, "--bits"))
        {
          const char *v = NEXT ();
          free (src.bits);
          src.bits = v ? parse_bit_string (v, &src.n_bits) : NULL;
          if (!src.bits)
            {
              fprintf (stderr, "error: --bits expects a 0/1 string\n");
              return 2;
            }
        }
      else if (!strcmp (a, "--bits-hex"))
        {
          const char *v = NEXT ();
          free (src.bits);
          src.bits = v ? parse_hex_string (v, &src.n_bits) : NULL;
          if (!src.bits)
            {
              fprintf (stderr, "error: --bits-hex expects a hex string\n");
              return 2;
            }
        }
      else if (!strcmp (a, "--bits-file"))
        {
          const char *v    = NEXT ();
          char       *text = v ? slurp_file (v) : NULL;
          if (!text)
            {
              fprintf (stderr, "error: cannot read --bits-file %s\n",
                       v ? v : "(none)");
              return 1;
            }
          free (src.bits);
          src.bits = parse_bit_string (text, &src.n_bits);
          free (text);
          if (!src.bits)
            {
              fprintf (stderr, "error: --bits-file must contain a 0/1 "
                               "string\n");
              return 2;
            }
        }
      else if (!strcmp (a, "--fs"))
        {
          seg.fs = strtod (NEXT (), NULL);
        }
      else if (!strcmp (a, "--freq"))
        {
          src.freq = strtod (NEXT (), NULL);
        }
      else if (!strcmp (a, "--f_end"))
        {
          src.f_end = strtod (NEXT (), NULL); /* chirp end frequency */
        }
      else if (!strcmp (a, "--fc"))
        {
          fc = strtod (NEXT (), NULL);
        }
      else if (!strcmp (a, "--snr"))
        {
          src.snr = strtod (NEXT (), NULL);
        }
      else if (!strcmp (a, "--seed"))
        {
          src.seed = (uint32_t)strtoul (NEXT (), NULL, 10);
        }
      else if (!strcmp (a, "--sps"))
        {
          src.sps = (int)strtol (NEXT (), NULL, 10);
        }
      else if (!strcmp (a, "--pn_length"))
        {
          src.pn_length = (int)strtol (NEXT (), NULL, 10);
        }
      else if (!strcmp (a, "--pn_poly"))
        {
          src.pn_poly = (uint64_t)strtoull (NEXT (), NULL, 10);
        }
      else if (!strcmp (a, "--count"))
        {
          seg.num_samples = (size_t)strtoull (NEXT (), NULL, 10);
        }
      else if (!strcmp (a, "--off"))
        {
          seg.off_samples = (size_t)strtoull (NEXT (), NULL, 10);
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
          src.level = strtod (NEXT (), NULL);
        }
      else if (!strcmp (a, "--headroom"))
        {
          headroom     = strtod (NEXT (), NULL);
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
          fprintf (stderr, "%s", USAGE);
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
          fprintf (stderr, "error: could not read %s\n", from_file);
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
      fprintf (stderr, "error: could not build the waveform spec\n");
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
              fputs (json, rf);
              fputc ('\n', rf);
              fclose (rf);
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

  if (out_path && !strncmp (out_path, "zmq://", 6) && !wfm_zmq_sink_open)
    {
      /* The ZMQ sink lives in the optional libdoppler_stream component (it
         pulls in the vendored C++ libzmq).  Its symbols are weak in the pure-C
         core, so they resolve to NULL here when the component is absent. */
      fprintf (stderr,
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
          fprintf (stderr, "error: cannot open zmq sink %s\n", out_path);
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
          fprintf (stderr, "error: --detached needs --output\n");
          wfm_compose_destroy (comp);
          return 2;
        }
      if (c)
        {
          fprintf (stderr, "error: --detached requires finite output "
                           "(not --continuous)\n");
          wfm_compose_destroy (comp);
          return 2;
        }
      char det_path[1024];
      snprintf (det_path, sizeof det_path, "%s.det", out_path);
      FILE *df = fopen (det_path, "wb");
      if (!df)
        {
          fprintf (stderr, "error: cannot open %s\n", det_path);
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
          fclose (df);
          char hdr_path[1024];
          snprintf (hdr_path, sizeof hdr_path, "%s.hdr", out_path);
          FILE *hf = fopen (hdr_path, "wb");
          if (hf)
            {
              wfm_blue_write_hcb (hf, sample_type, endian, fs, fc, 0.0, total,
                                  1);
              fclose (hf);
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
              fprintf (stderr, "error: --file_type sigmf needs --output\n");
              wfm_compose_destroy (comp);
              return 2;
            }
          snprintf (data_path, sizeof data_path, "%s.sigmf-data", out_path);
          fp = fopen (data_path, "wb");
        }
      else
        {
          fp = out_path ? fopen (out_path, "wb") : stdout;
        }
      if (!fp)
        {
          fprintf (stderr, "error: cannot open output\n");
          wfm_compose_destroy (comp);
          return 1;
        }
      int           wft = sigmf ? WFM_FT_RAW : file_type;
      wfm_writer_t *w
          = wfm_writer_open (fp, wft, sample_type, endian, fs, fc, 0);
      if (!w)
        {
          fprintf (stderr, "error: cannot open writer\n");
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
        fclose (fp);

      if (sigmf && rc == 0)
        {
          char *meta = wfm_sigmf_meta_json (sample_type, endian, fs, fc, segs,
                                            n_segs);
          if (meta)
            {
              char meta_path[1024];
              snprintf (meta_path, sizeof meta_path, "%s.sigmf-meta",
                        out_path);
              FILE *mf = fopen (meta_path, "w");
              if (mf)
                {
                  fputs (meta, mf);
                  fclose (mf);
                }
              free (meta);
            }
        }
    }

  if (realtime && clk.underruns)
    fprintf (stderr,
             "wfmgen: %llu underrun(s) — worst %.3f ms behind real time\n",
             (unsigned long long)clk.underruns, (double)clk.max_late_ns / 1e6);

  wfm_compose_destroy (comp);
  free (src.bits); /* the composer deep-copied it; free our CLI-owned copy */
  return rc;
}
