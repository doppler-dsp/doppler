/* jm_bench.h — header-only benchmark stats and JSON output.
 *
 * Include in bench_*_core.c.  After timing each section, call
 * jm_bench_add().  At the end of main() call jm_bench_write_json(),
 * which writes bench_<component>_core.json in the current directory.
 * The JSON format is compatible with pytest-benchmark so C and Python
 * results can be compared directly.  All times are in seconds;
 * ops = iterations / mean (samples per second).
 */
#ifndef JM_BENCH_H
#define JM_BENCH_H

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/utsname.h>
#endif

#define JM_BENCH_MAX_ENTRIES 32
#define JM_BENCH_NAME_LEN 64

typedef struct
{
  char    name[JM_BENCH_NAME_LEN];
  double *times;  /* heap copy of per-round elapsed seconds */
  int     rounds; /* outer iteration count (ITERATIONS) */
  int     iters;  /* inner calls per round (BENCH_N) */
} jm_bench_entry_t;

typedef struct
{
  jm_bench_entry_t entries[JM_BENCH_MAX_ENTRIES];
  int              count;
} jm_bench_t;

/* Copy times[0..rounds-1] into the bench.  iters = BENCH_N. */
static void
jm_bench_add (jm_bench_t *b, const char *name, const double *times, int rounds,
              int iters)
{
  if (b->count >= JM_BENCH_MAX_ENTRIES)
    return;
  jm_bench_entry_t *e = &b->entries[b->count++];
  strncpy (e->name, name, JM_BENCH_NAME_LEN - 1);
  e->name[JM_BENCH_NAME_LEN - 1] = '\0';
  e->times = (double *)malloc ((size_t)rounds * sizeof (double));
  if (!e->times)
    {
      b->count--;
      return;
    }
  memcpy (e->times, times, (size_t)rounds * sizeof (double));
  e->rounds = rounds;
  e->iters  = iters;
}

/* qsort comparator for double */
static int
_jm_dcmp (const void *a, const void *b)
{
  double da = *(const double *)a, db = *(const double *)b;
  return (da > db) - (da < db);
}

/* Linear-interpolation quantile on sorted array s[0..n-1]. */
static double
_jm_quantile (const double *s, int n, double p)
{
  double pos = p * (double)(n - 1);
  int    lo  = (int)pos;
  double f   = pos - (double)lo;
  if (lo + 1 >= n)
    return s[n - 1];
  return s[lo] * (1.0 - f) + s[lo + 1] * f;
}

/* Write pytest-benchmark-compatible JSON to bench_<component>_core.json. */
static void
jm_bench_write_json (const jm_bench_t *b, const char *component)
{
  char fname[256];
  snprintf (fname, sizeof (fname), "bench_%s_core.json", component);
  FILE *fp = fopen (fname, "w");
  if (!fp)
    {
      fprintf (stderr, "jm_bench: cannot open %s\n", fname);
      return;
    }

  /* Collect machine info. */
  char sys_name[64]  = "unknown";
  char node_name[64] = "unknown";
  char release[64]   = "unknown";
  char machine[64]   = "unknown";

#if defined(_WIN32)
  strncpy (sys_name, "Windows", 63);
  {
    DWORD n = (DWORD)sizeof (node_name);
    GetComputerNameA (node_name, &n);
  }
  strncpy (machine, "x86_64", 63);
  strncpy (release, "unknown", 63);
#else
  {
    struct utsname u;
    if (uname (&u) == 0)
      {
        strncpy (sys_name, u.sysname, 63);
        strncpy (node_name, u.nodename, 63);
        strncpy (release, u.release, 63);
        strncpy (machine, u.machine, 63);
      }
  }
#endif

  /* Timestamp. */
  time_t now    = time (NULL);
  char   ts[32] = "1970-01-01T00:00:00.000000";
  {
    struct tm *tm_info = localtime (&now);
    if (tm_info)
      strftime (ts, sizeof (ts), "%Y-%m-%dT%H:%M:%S.000000", tm_info);
  }

  fprintf (fp, "{\n");
  fprintf (fp, "  \"machine_info\": {\n");
  fprintf (fp, "    \"node\": \"%s\",\n", node_name);
  fprintf (fp, "    \"processor\": \"%s\",\n", machine);
  fprintf (fp, "    \"machine\": \"%s\",\n", machine);
  fprintf (fp, "    \"python_implementation\": null,\n");
  fprintf (fp, "    \"python_version\": null,\n");
  fprintf (fp, "    \"python_build\": null,\n");
  fprintf (fp, "    \"release\": \"%s\",\n", release);
  fprintf (fp, "    \"system\": \"%s\"\n", sys_name);
  fprintf (fp, "  },\n");
  fprintf (fp, "  \"commit_info\": null,\n");
  fprintf (fp, "  \"benchmarks\": [\n");

  for (int i = 0; i < b->count; i++)
    {
      const jm_bench_entry_t *e = &b->entries[i];
      int                     n = e->rounds;

      /* Sort a copy for order statistics. */
      double *s = (double *)malloc ((size_t)n * sizeof (double));
      if (!s)
        continue;
      memcpy (s, e->times, (size_t)n * sizeof (double));
      qsort (s, (size_t)n, sizeof (double), _jm_dcmp);

      double mn = s[0], mx = s[n - 1];
      double sum = 0.0;
      for (int j = 0; j < n; j++)
        sum += s[j];
      double mean = sum / (double)n;
      double var  = 0.0;
      for (int j = 0; j < n; j++)
        {
          double d = s[j] - mean;
          var += d * d;
        }
      double stddev = (n > 1) ? sqrt (var / (double)(n - 1)) : 0.0;
      double median = _jm_quantile (s, n, 0.5);
      double q1     = _jm_quantile (s, n, 0.25);
      double q3     = _jm_quantile (s, n, 0.75);
      double iqr    = q3 - q1;
      double ops    = (double)e->iters / mean;

      fprintf (fp, "    {\n");
      fprintf (fp, "      \"group\": null,\n");
      fprintf (fp, "      \"name\": \"%s\",\n", e->name);
      fprintf (fp, "      \"fullname\": \"bench_%s_core::%s\",\n", component,
               e->name);
      fprintf (fp, "      \"params\": null,\n");
      fprintf (fp, "      \"param\": null,\n");
      fprintf (fp, "      \"extra_info\": {},\n");
      fprintf (fp, "      \"options\": {\n");
      fprintf (fp, "        \"disable_gc\": false,\n");
      fprintf (fp, "        \"timer\": \"clock_gettime\",\n");
      fprintf (fp, "        \"min_rounds\": %d,\n", n);
      fprintf (fp, "        \"max_time\": null,\n");
      fprintf (fp, "        \"min_time\": null,\n");
      fprintf (fp, "        \"warmup\": true\n");
      fprintf (fp, "      },\n");
      fprintf (fp, "      \"stats\": {\n");
      fprintf (fp, "        \"min\": %.17g,\n", mn);
      fprintf (fp, "        \"max\": %.17g,\n", mx);
      fprintf (fp, "        \"mean\": %.17g,\n", mean);
      fprintf (fp, "        \"stddev\": %.17g,\n", stddev);
      fprintf (fp, "        \"rounds\": %d,\n", n);
      fprintf (fp, "        \"median\": %.17g,\n", median);
      fprintf (fp, "        \"iqr\": %.17g,\n", iqr);
      fprintf (fp, "        \"q1\": %.17g,\n", q1);
      fprintf (fp, "        \"q3\": %.17g,\n", q3);
      fprintf (fp, "        \"iqr_outliers\": 0,\n");
      fprintf (fp, "        \"stddev_outliers\": 0,\n");
      fprintf (fp, "        \"outliers\": \"0;0\",\n");
      fprintf (fp, "        \"ld15iqr\": %.17g,\n", mn);
      fprintf (fp, "        \"hd15iqr\": %.17g,\n", mx);
      fprintf (fp, "        \"ops\": %.17g,\n", ops);
      fprintf (fp, "        \"total\": %.17g,\n", sum);
      fprintf (fp, "        \"iterations\": %d\n", e->iters);
      fprintf (fp, "      }\n");
      fprintf (fp, "    }%s\n", i < b->count - 1 ? "," : "");
      free (s);
    }

  fprintf (fp, "  ],\n");
  fprintf (fp, "  \"datetime\": \"%s\",\n", ts);
  fprintf (fp, "  \"version\": \"4.0.0\"\n");
  fprintf (fp, "}\n");
  fclose (fp);
  printf ("  json    bench_%s_core.json\n", component);
}

#endif /* JM_BENCH_H */
