/* jm_bench.h — header-only benchmark stats and JSON output.
 *
 * Include in bench_*_core.c.  After timing each section, call
 * jm_bench_add_unit() (or jm_bench_add(), which records calls).  At the end
 * of main() call jm_bench_write_json(), which writes
 * bench_<component>_core.json in the current directory and then frees the
 * rows, leaving the bench empty.
 *
 * A row is `rounds` timings, each of one round of `iters` units of work, and
 * the row names its unit: any word -- JM_BENCH_SAMPLE, JM_BENCH_CALL and
 * JM_BENCH_FRAME are the ones jm's generated benchmarks use, and a project
 * passes its own ("bin", "tap") without editing this file.  The JSON keeps
 * pytest-benchmark's layout, so C and Python results can be compared
 * directly.  All times are in seconds.  `stats` min/max/mean/... are per
 * ROUND; `stats` *_per_unit are the same divided by `iterations`;
 * ops = iterations / mean, in units per second.  `extra_info` names the unit
 * and its rate label (jm_bench_rate_label), and a sample row also carries
 * MSa_s = ops / 1e6 (gh-2282).
 */
#ifndef JM_BENCH_H
#define JM_BENCH_H

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#  include <windows.h>
#else
#  include <sys/utsname.h>
#endif

/* gh-1341: the monotonic clock, in ONE place.
 *
 * Every generated benchmark used to call `clock_gettime(CLOCK_MONOTONIC)` in
 * its own main(). Both are POSIX and the UCRT has neither, so on Windows all
 * of them failed to compile -- 106 of them in doppler. This header is
 * included by all of them, so this is the one place that fixes all of them,
 * and it is not patchable downstream: `jm_bench.h` is vendored and
 * create-only, so a local edit is a private copy that does not survive a
 * re-vendor (doppler has lost two that way).
 *
 * Nanoseconds as `uint64_t` rather than a `struct timespec`, which also drops
 * that type from the generated code -- it is C11 on MSVC and was optional
 * before that.
 */
static inline uint64_t
jm_bench_now_ns(void)
{
#if defined(_WIN32)
    LARGE_INTEGER c, f;
    QueryPerformanceCounter(&c);
    QueryPerformanceFrequency(&f);
    /* Quotient plus remainder, NOT `c * 1000000000 / f`. That product
     * overflows 64 bits after about nine seconds at a 1 GHz QPC frequency,
     * which is well inside a benchmark's runtime -- doppler hit exactly this
     * porting its own timing core. Splitting the division keeps both terms
     * small and loses nothing: the remainder term is exact. */
    return (uint64_t)(c.QuadPart / f.QuadPart) * 1000000000ULL
           + (uint64_t)(c.QuadPart % f.QuadPart) * 1000000000ULL
                 / (uint64_t)f.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}

/* Seconds between two `jm_bench_now_ns()` reads. */
static inline double
jm_bench_elapsed_sec(uint64_t t0, uint64_t t1)
{
    return (double)(t1 - t0) * 1e-9;
}

/* The timer's name, for the JSON `options.timer` field -- derived from the
 * same #if that chose it, so the two cannot disagree about what ran. */
#if defined(_WIN32)
#  define JM_BENCH_TIMER_NAME "QueryPerformanceCounter"
#else
#  define JM_BENCH_TIMER_NAME "clock_gettime"
#endif

#define JM_BENCH_NAME_LEN 64

/* gh-2282: what one of a row's `iters` counts is a WORD, not a closed set.
 *
 * A row used to state nothing: the header said `ops` was samples per second
 * while `iters` was documented as calls, and a reader taking `mean` for a
 * time per call was off by a factor of `iters`. The unit is a string so a
 * project counting something jm never heard of ("bin", "tap") says so from
 * its own source -- a unit it had to add HERE would be a private edit to a
 * vendored file, lost on the next re-vendor (doppler#2314, which is how a
 * private MSa_s line vanished for 25 releases).
 *
 * These three are the vocabulary jm's generated benchmarks use. A unit is a
 * lowercase singular noun: at most JM_BENCH_UNIT_LEN - 1 characters of
 * [a-z0-9_], so it is safe in the JSON as written. */
#define JM_BENCH_SAMPLE "sample"
#define JM_BENCH_CALL   "call"
#define JM_BENCH_FRAME  "frame"

#define JM_BENCH_UNIT_LEN 32
/* Room for the longest label jm_bench_rate_label writes: "<unit>s/s". */
#define JM_BENCH_RATE_LEN (JM_BENCH_UNIT_LEN + 3)

/* The throughput label for `unit`, written into buf[0..n-1]: the ONE place
 * a rate is named. "Sa/s" for a sample (the DSP spelling, and the MSa_s
 * key's), "<unit>s/s" for anything else -- "calls/s", "bins/s". The JSON's
 * `extra_info.rate_unit` is this string, and `jm bench` prints it as read,
 * so neither the generated code nor jm's reader spells a unit. Returns buf,
 * so a printf can take the call directly. */
static inline const char *
jm_bench_rate_label(const char *unit, char *buf, size_t n)
{
    if (strcmp(unit, JM_BENCH_SAMPLE) == 0)
        snprintf(buf, n, "Sa/s");
    else
        snprintf(buf, n, "%ss/s", unit);
    return buf;
}

typedef struct {
    char    name[JM_BENCH_NAME_LEN];
    double *times;  /* heap copy of per-round elapsed seconds */
    int     rounds; /* outer iteration count (ITERATIONS) */
    int     iters;  /* units of work per round (BENCH_N) */
    char    unit[JM_BENCH_UNIT_LEN]; /* what one of `iters` is (gh-2282) */
} jm_bench_entry_t;

/* gh-2188: `entries` is a heap array that jm_bench_add grows, so a benchmark
 * may record any number of rows. It was a fixed array of 32, and
 * jm_bench_add returned without a word once it was full: a 39-row benchmark
 * printed all 39 rows, exited 0 and wrote a JSON holding 32. `= {0}` is
 * still the whole initialisation -- NULL, 0, 0 is an empty bench. */
typedef struct {
    jm_bench_entry_t *entries; /* `count` rows, room for `cap` */
    int count;
    int cap;
} jm_bench_t;

/* Say why on stderr and exit non-zero: the one way this header refuses.
 *
 * gh-2188: a row the header cannot hold, or one with no time in it, must not
 * be left out of the JSON (or written as garbage) while the run exits 0. So
 * the run stops, naming the row when there is one, and `jm bench` reports
 * the benchmark as failed. */
static inline void
jm_bench_fail(const char *row, const char *why)
{
    if (row)
        fprintf(stderr, "jm_bench: row \"%s\": %s\n", row, why);
    else
        fprintf(stderr, "jm_bench: %s\n", why);
    exit(EXIT_FAILURE);
}

/* realloc() that never hands back NULL: every allocation this header makes
 * holds a row the benchmark measured, so one that fails goes through
 * jm_bench_fail rather than dropping the row. `n` is at least 1 because
 * realloc(p, 0) may free p and return NULL. */
static inline void *
jm_bench_xrealloc(void *p, size_t n)
{
    void *q = realloc(p, n ? n : 1);
    if (!q)
        jm_bench_fail(NULL, "out of memory recording results");
    return q;
}

/* Free every row and leave the bench empty, as `= {0}` made it.
 *
 * gh-2188: jm_bench_write_json calls this once the file is written, so a
 * benchmark built with -fsanitize=address exits with nothing leaked. A
 * benchmark that returns without writing -- a guard refusing a short set --
 * calls it itself. */
static inline void
jm_bench_free(jm_bench_t *b)
{
    for (int i = 0; i < b->count; i++)
        free(b->entries[i].times);
    free(b->entries);
    b->entries = NULL;
    b->count   = 0;
    b->cap     = 0;
}

/* Copy src into dst[0..n-1], truncating, always NUL-terminated.
 *
 * gh-1745: the one bounded copy this header makes, for the entry name and
 * the machine fields. `strncpy(dst, src, n - 1)` plus a hand-written NUL is
 * correct, but gcc's -Wstringop-truncation cannot see the NUL, and
 * `snprintf(dst, n, "%s", src)` trips -Wformat-truncation instead whenever
 * src (a 65-byte utsname field) may be longer than dst -- each an error for
 * a project building its benchmarks with -Werror. An explicit length and a
 * memcpy say the same thing with nothing for either warning to guess at. */
static inline void
jm_bench_strcpy(char *dst, size_t n, const char *src)
{
    size_t k = strlen(src);
    if (k >= n)
        k = n - 1;
    memcpy(dst, src, k);
    dst[k] = '\0';
}

/* Refuse a row's unit, quoting it: `unit "in-sample" must be [a-z0-9_]`.
 *
 * The value is the invalid input, so it is shown bounded and printable: a
 * byte outside printable ASCII, a quote or a backslash is written as \xNN,
 * and past JM_BENCH_UNIT_SHOWN bytes the rest is cut to `...`. A NULL unit
 * says so rather than being printed. `rule` is what the unit must be. */
#define JM_BENCH_UNIT_SHOWN 40

static inline void
jm_bench_fail_unit(const char *row, const char *unit, const char *rule)
{
    /* Each shown byte takes at most 4 characters (\xNN), so the loop's
     * bound leaves room for the `...` and the NUL. */
    char   shown[4 * JM_BENCH_UNIT_SHOWN + 4];
    char   why[sizeof(shown) + 64];
    size_t o = 0;
    const char *p;
    if (!unit) {
        snprintf(why, sizeof(why), "unit is NULL; it %s", rule);
        jm_bench_fail(row, why);
        return;
    }
    for (p = unit; *p && p - unit < JM_BENCH_UNIT_SHOWN; p++) {
        unsigned char ch = (unsigned char)*p;
        if (ch >= 0x20 && ch < 0x7f && ch != '"' && ch != '\\')
            shown[o++] = (char)ch;
        else
            o += (size_t)snprintf(shown + o, sizeof(shown) - o, "\\x%02x",
                                  ch);
    }
    if (*p) {
        memcpy(shown + o, "...", 3);
        o += 3;
    }
    shown[o] = '\0';
    snprintf(why, sizeof(why), "unit \"%s\" %s", shown, rule);
    jm_bench_fail(row, why);
}

/* Copy times[0..rounds-1] into the bench: `rounds` timings, each of one
 * round of `iters` units of `unit` -- a steps() call over BENCH_N samples
 * is iters = BENCH_N, unit = JM_BENCH_SAMPLE.
 *
 * gh-840: `static inline`, not plain `static`. A benchmark that has not been
 * filled in yet never calls this, and a plain `static` in a header makes that
 * a -Wunused-function warning -- so a project building its benchmarks with
 * -Werror could not compile a jm scaffold at all. GCC and clang do not warn
 * about an unused `static inline` in a header, which is what the idiom is
 * for. */
static inline void
jm_bench_add_unit(jm_bench_t *b, const char *name, const double *times,
                  int rounds, int iters, const char *unit)
{
    /* A row with no rounds has no time to report, and jm_bench_write_json
     * would read its order statistics out of an empty array. */
    if (rounds < 1)
        jm_bench_fail(name, "rounds must be at least 1");
    /* A row whose unit the JSON cannot name as written would be an
     * unlabelled row or a broken file; truncating one would label it
     * wrong. Refused, as the header refuses everything (gh-2188). */
    {
        size_t k = unit ? strlen(unit) : 0;
        if (k == 0 || k >= JM_BENCH_UNIT_LEN)
            jm_bench_fail_unit(name, unit, "must be 1 to 31 characters");
        for (size_t c = 0; c < k; c++) {
            char ch = unit[c];
            if (!((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9')
                  || ch == '_'))
                jm_bench_fail_unit(name, unit, "must be [a-z0-9_]");
        }
    }
    /* Grow by doubling: N rows cost O(log N) reallocs. Each entry's `times`
     * is its own allocation, so moving the array moves only the pointer. */
    if (b->count == b->cap) {
        int cap = b->cap ? 2 * b->cap : 16;
        b->entries = (jm_bench_entry_t *)jm_bench_xrealloc(
            b->entries, (size_t)cap * sizeof(*b->entries));
        b->cap = cap;
    }
    jm_bench_entry_t *e = &b->entries[b->count++];
    jm_bench_strcpy(e->name, sizeof(e->name), name);
    e->times = (double *)jm_bench_xrealloc(
        NULL, (size_t)rounds * sizeof(double));
    memcpy(e->times, times, (size_t)rounds * sizeof(double));
    e->rounds = rounds;
    e->iters  = iters;
    jm_bench_strcpy(e->unit, sizeof(e->unit), unit);
}

/* A row of `iters` CALLS per round: jm_bench_add_unit(..., JM_BENCH_CALL).
 *
 * gh-2282: the five-argument form every benchmark written before the unit
 * existed calls, kept so they compile unchanged. A call is what `iters` was
 * documented to count. A row of samples or frames says so through
 * jm_bench_add_unit. */
static inline void
jm_bench_add(jm_bench_t *b, const char *name,
             const double *times, int rounds, int iters)
{
    jm_bench_add_unit(b, name, times, rounds, iters, JM_BENCH_CALL);
}

/* qsort comparator for double */
static int
jm_dcmp(const void *a, const void *b)
{
    double da = *(const double *)a, db = *(const double *)b;
    return (da > db) - (da < db);
}

/* Linear-interpolation quantile on sorted array s[0..n-1]. */
static double
jm_quantile(const double *s, int n, double p)
{
    double pos = p * (double)(n - 1);
    int    lo  = (int)pos;
    double f   = pos - (double)lo;
    if (lo + 1 >= n) return s[n - 1];
    return s[lo] * (1.0 - f) + s[lo + 1] * f;
}

/* Write pytest-benchmark-compatible JSON to bench_<component>_core.json,
 * then free the rows (jm_bench_free): the bench is empty afterwards. */
static void
jm_bench_write_json(jm_bench_t *b, const char *component)
{
    char fname[256];
    snprintf(fname, sizeof(fname), "bench_%s_core.json", component);
    FILE *fp = fopen(fname, "w");
    if (!fp) {
        fprintf(stderr, "jm_bench: cannot open %s\n", fname);
        jm_bench_free(b);
        return;
    }

    /* Collect machine info. */
    char sys_name[64]  = "unknown";
    char node_name[64] = "unknown";
    char release[64]   = "unknown";
    char machine[64]   = "unknown";

#if defined(_WIN32)
    jm_bench_strcpy(sys_name, sizeof(sys_name), "Windows");
    {
        DWORD n = (DWORD)sizeof(node_name);
        GetComputerNameA(node_name, &n);
    }
    jm_bench_strcpy(machine, sizeof(machine), "x86_64");
    jm_bench_strcpy(release, sizeof(release), "unknown");
#else
    {
        struct utsname u;
        if (uname(&u) == 0) {
            jm_bench_strcpy(sys_name,  sizeof(sys_name),  u.sysname);
            jm_bench_strcpy(node_name, sizeof(node_name), u.nodename);
            jm_bench_strcpy(release,   sizeof(release),   u.release);
            jm_bench_strcpy(machine,   sizeof(machine),   u.machine);
        }
    }
#endif

    /* Timestamp. */
    time_t now = time(NULL);
    char ts[32] = "1970-01-01T00:00:00.000000";
    {
        struct tm *tm_info = localtime(&now);
        if (tm_info)
            strftime(ts, sizeof(ts),
                     "%Y-%m-%dT%H:%M:%S.000000", tm_info);
    }

    fprintf(fp, "{\n");
    fprintf(fp, "  \"machine_info\": {\n");
    fprintf(fp, "    \"node\": \"%s\",\n", node_name);
    fprintf(fp, "    \"processor\": \"%s\",\n", machine);
    fprintf(fp, "    \"machine\": \"%s\",\n", machine);
    fprintf(fp, "    \"python_implementation\": null,\n");
    fprintf(fp, "    \"python_version\": null,\n");
    fprintf(fp, "    \"python_build\": null,\n");
    fprintf(fp, "    \"release\": \"%s\",\n", release);
    fprintf(fp, "    \"system\": \"%s\"\n", sys_name);
    fprintf(fp, "  },\n");
    fprintf(fp, "  \"commit_info\": null,\n");
    fprintf(fp, "  \"benchmarks\": [\n");

    for (int i = 0; i < b->count; i++) {
        const jm_bench_entry_t *e = &b->entries[i];
        int n = e->rounds;

        /* Sort a copy for order statistics. */
        double *s = (double *)jm_bench_xrealloc(
            NULL, (size_t)n * sizeof(double));
        memcpy(s, e->times, (size_t)n * sizeof(double));
        qsort(s, (size_t)n, sizeof(double), jm_dcmp);

        double mn  = s[0], mx = s[n - 1];
        double sum = 0.0;
        for (int j = 0; j < n; j++) sum += s[j];
        double mean = sum / (double)n;
        double var  = 0.0;
        for (int j = 0; j < n; j++) {
            double d = s[j] - mean;
            var += d * d;
        }
        double stddev = (n > 1) ? sqrt(var / (double)(n - 1)) : 0.0;
        double median = jm_quantile(s, n, 0.5);
        double q1     = jm_quantile(s, n, 0.25);
        double q3     = jm_quantile(s, n, 0.75);
        double iqr    = q3 - q1;
        double ops    = (double)e->iters / mean;
        /* gh-2282: the per-round figures over the units in a round. A row
         * of no units has no time per unit, so it says null rather than
         * writing `inf`, which is not JSON. */
        const char  *pu_key[] = {"min", "max", "mean", "stddev", "median"};
        const double pu_val[] = {mn, mx, mean, stddev, median};
        const int    pu_n     = (int)(sizeof(pu_val) / sizeof(pu_val[0]));

        fprintf(fp, "    {\n");
        fprintf(fp, "      \"group\": null,\n");
        fprintf(fp, "      \"name\": \"%s\",\n", e->name);
        fprintf(fp, "      \"fullname\": \"bench_%s_core::%s\",\n",
                component, e->name);
        fprintf(fp, "      \"params\": null,\n");
        fprintf(fp, "      \"param\": null,\n");
        /* gh-2282: the unit and its rate label (jm_bench_rate_label); a
         * sample row also states its rate in MSa/s. This was a literal
         * `{}`, and a downstream's private `MSa_s` line was lost to a
         * re-vendor for 25 releases. */
        int  is_sample = strcmp(e->unit, JM_BENCH_SAMPLE) == 0;
        char rate[JM_BENCH_RATE_LEN];
        fprintf(fp, "      \"extra_info\": {\n");
        fprintf(fp, "        \"unit\": \"%s\",\n", e->unit);
        fprintf(fp, "        \"rate_unit\": \"%s\"%s\n",
                jm_bench_rate_label(e->unit, rate, sizeof(rate)),
                is_sample ? "," : "");
        if (is_sample)
            fprintf(fp, "        \"MSa_s\": %.17g\n", ops / 1e6);
        fprintf(fp, "      },\n");
        fprintf(fp, "      \"options\": {\n");
        fprintf(fp, "        \"disable_gc\": false,\n");
        fprintf(fp, "        \"timer\": \"%s\",\n",
                JM_BENCH_TIMER_NAME);
        fprintf(fp, "        \"min_rounds\": %d,\n", n);
        fprintf(fp, "        \"max_time\": null,\n");
        fprintf(fp, "        \"min_time\": null,\n");
        fprintf(fp, "        \"warmup\": true\n");
        fprintf(fp, "      },\n");
        fprintf(fp, "      \"stats\": {\n");
        fprintf(fp, "        \"min\": %.17g,\n", mn);
        fprintf(fp, "        \"max\": %.17g,\n", mx);
        fprintf(fp, "        \"mean\": %.17g,\n", mean);
        fprintf(fp, "        \"stddev\": %.17g,\n", stddev);
        fprintf(fp, "        \"rounds\": %d,\n", n);
        fprintf(fp, "        \"median\": %.17g,\n", median);
        fprintf(fp, "        \"iqr\": %.17g,\n", iqr);
        fprintf(fp, "        \"q1\": %.17g,\n", q1);
        fprintf(fp, "        \"q3\": %.17g,\n", q3);
        fprintf(fp, "        \"iqr_outliers\": 0,\n");
        fprintf(fp, "        \"stddev_outliers\": 0,\n");
        fprintf(fp, "        \"outliers\": \"0;0\",\n");
        fprintf(fp, "        \"ld15iqr\": %.17g,\n", mn);
        fprintf(fp, "        \"hd15iqr\": %.17g,\n", mx);
        fprintf(fp, "        \"ops\": %.17g,\n", ops);
        fprintf(fp, "        \"total\": %.17g,\n", sum);
        fprintf(fp, "        \"iterations\": %d,\n", e->iters);
        for (int k = 0; k < pu_n; k++) {
            fprintf(fp, "        \"%s_per_unit\": ", pu_key[k]);
            if (e->iters > 0)
                fprintf(fp, "%.17g", pu_val[k] / (double)e->iters);
            else
                fprintf(fp, "null");
            fprintf(fp, "%s\n", k < pu_n - 1 ? "," : "");
        }
        fprintf(fp, "      }\n");
        fprintf(fp, "    }%s\n", i < b->count - 1 ? "," : "");
        free(s);
    }

    fprintf(fp, "  ],\n");
    fprintf(fp, "  \"datetime\": \"%s\",\n", ts);
    fprintf(fp, "  \"version\": \"4.0.0\"\n");
    fprintf(fp, "}\n");
    fclose(fp);
    printf("  json    bench_%s_core.json\n", component);

    /* gh-806: a benchmark that timed nothing exits 0 and writes a
     * well-formed file with an empty "benchmarks" array -- indistinguishable,
     * in a CI log, from one that measured everything.  Said out loud here
     * rather than at each call site because this is the one function every
     * bench target reaches, so no shape can be added later that forgets to
     * say it.  On stderr as well as stdout: the JSON consumer reads the file,
     * the human reads the log, and the log is where this was missed. */
    if (b->count == 0) {
        printf("  EMPTY   bench_%s_core: no measurements recorded\n",
               component);
        fprintf(stderr,
                "bench_%s_core: recorded 0 measurements -- this target"
                " measures nothing.\n", component);
    }
    jm_bench_free(b);
}

#endif /* JM_BENCH_H */
