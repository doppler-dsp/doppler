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
#include "wfmgen/wfm_compose.h"
#include "wfmgen/wfm_sink.h"
#include "wfmgen/wfm_writer.h"

#define BLK 4096

static const char *const TYPES[]   = { "tone", "noise", "pn", "bpsk", "qpsk" };
static const char *const MODES[]   = { "auto", "fs", "ebno", "esno" };
static const char *const STYPES[]  = { "cf32", "cf64", "ci32", "ci16", "ci8" };
static const char *const FTYPES[]  = { "raw", "csv", "blue", "sigmf" };
static const char *const ENDIANS[] = { "le", "be" };
static const char *const LFSRS[]   = { "galois", "fibonacci" };

/* Look name up in a NULL-free table of n entries; -1 if absent. */
static int
lookup (const char *s, const char *const *tbl, int n)
{
  for (int i = 0; i < n; i++)
    if (!strcmp (s, tbl[i]))
      return i;
  return -1;
}

/* Warn (and optionally fail) when an integer wire type clipped. peak > 1 means
 * the composite ran past full-scale; report the overshoot in dB (the headroom
 * it would need) and how to capture it losslessly. Float types never clip.
 * Shared by the writer and sink paths. Returns non-zero when --clip-error
 * should fail the run. */
static int
report_clip (double peak, double frac, int stype, int clip_report,
             int clip_error)
{
  double dbfs = peak > 0.0 ? 20.0 * log10 (peak) : -120.0;
  if (stype < 2 || peak <= 1.0)
    {
      if (clip_report)
        fprintf (stderr, "wfmgen: peak %.1f dBFS — no clipping\n", dbfs);
      return 0;
    }
  fprintf (stderr,
           "wfmgen: warning: %s output clipped — peak is +%.1f dB over full "
           "scale.\n  use --sample_type cf32 to capture it losslessly.\n",
           STYPES[stype], dbfs);
  if (clip_report)
    fprintf (stderr, "  clipped %.2f%% of I/Q components\n", 100.0 * frac);
  return clip_error ? 1 : 0;
}

static const char USAGE[]
    = "usage: wfmgen [--from-file SPEC.json] [--type "
      "tone|noise|pn|bpsk|qpsk]\n"
      "  [--fs HZ] [--freq HZ] [--fc HZ] [--snr DB] [--snr_mode "
      "auto|fs|ebno|esno]\n"
      "  [--seed N] [--sps N] [--pn_length N] [--pn_poly N] "
      "[--lfsr galois|fibonacci]\n"
      "  [--count N] [--off N] [--repeat] [--continuous]\n"
      "  [--sample_type cf32|cf64|ci32|ci16|ci8] [--file_type "
      "raw|csv|blue|sigmf]\n"
      "  [--endian le|be] [--detached] [--realtime] [--realtime-resync]\n"
      "  [--clip-report] [--clip-error] [--output FILE|zmq://EP] [--record "
      "FILE]\n";

int
main (int argc, char *argv[])
{
  /* single-segment defaults mirror synth/wavegen */
  wfm_segment_t seg    = { .type        = 0,
                           .fs          = 1e6,
                           .freq        = 0.0,
                           .snr         = 100.0,
                           .snr_mode    = 0,
                           .seed        = 1,
                           .sps         = 8,
                           .pn_length   = 7,
                           .pn_poly     = 0,
                           .num_samples = 1024,
                           .off_samples = 0 };
  int           repeat = 0, continuous = 0, detached = 0;
  int           realtime = 0, realtime_resync = 0;
  int           clip_report = 0, clip_error = 0;
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
          CHOICE (seg.type, TYPES);
        }
      else if (!strcmp (a, "--snr_mode"))
        {
          CHOICE (seg.snr_mode, MODES);
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
          CHOICE (seg.lfsr, LFSRS);
        }
      else if (!strcmp (a, "--fs"))
        {
          seg.fs = strtod (NEXT (), NULL);
        }
      else if (!strcmp (a, "--freq"))
        {
          seg.freq = strtod (NEXT (), NULL);
        }
      else if (!strcmp (a, "--fc"))
        {
          fc = strtod (NEXT (), NULL);
        }
      else if (!strcmp (a, "--snr"))
        {
          seg.snr = strtod (NEXT (), NULL);
        }
      else if (!strcmp (a, "--seed"))
        {
          seg.seed = (uint32_t)strtoul (NEXT (), NULL, 10);
        }
      else if (!strcmp (a, "--sps"))
        {
          seg.sps = (int)strtol (NEXT (), NULL, 10);
        }
      else if (!strcmp (a, "--pn_length"))
        {
          seg.pn_length = (int)strtol (NEXT (), NULL, 10);
        }
      else if (!strcmp (a, "--pn_poly"))
        {
          seg.pn_poly = (uint64_t)strtoull (NEXT (), NULL, 10);
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

  /* Build the composer: from a JSON spec, or the single-segment flags. */
  wfm_compose_state_t *comp
      = from_file ? wfm_compose_from_file (from_file)
                  : wfm_compose_create (&seg, 1, repeat, continuous);
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
      char *json = wfm_spec_to_json (segs, n_segs, r, c);
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

  int           rc = 0;
  float complex buf[BLK];
  size_t        n;

  if (out_path && !strncmp (out_path, "zmq://", 6))
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
                           clip_report, clip_error))
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
                               clip_report, clip_error))
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
                           sample_type, clip_report, clip_error))
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
  return rc;
}
