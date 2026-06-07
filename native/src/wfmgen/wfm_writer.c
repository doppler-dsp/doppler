/*
 * wfm_writer.c — output containers (raw / csv / BLUE-1000) + SigMF meta.
 *
 * Host is assumed little-endian (doppler's targets); big-endian output is
 * produced by reversing each element on the way out.
 */
#include "wfmgen/wfm_writer.h"

#include <complex.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"

/* per sample_type (wavegen order 0 cf32,1 cf64,2 ci32,3 ci16,4 ci8) */
static const size_t ELEM[5] = {4, 8, 4, 2, 1};         /* bytes per I or Q */
static const size_t BPS[5] = {8, 16, 8, 4, 2};         /* bytes per sample */
static const double SCALE[5] = {0, 0, 2147483647.0, 32767.0, 127.0};
static const char FMTCH[5] = {'F', 'D', 'L', 'I', 'B'}; /* BLUE format char */

static const char *const TYPE_NAMES[5] = {"tone", "noise", "pn", "bpsk",
                                          "qpsk"};
static const char *const MODE_NAMES[4] = {"auto", "fs", "ebno", "esno"};

struct wfm_writer {
    FILE *fp;
    int ft;
    int stype;
    int be;
    size_t written; /* complex samples emitted */
    uint8_t *buf;   /* convert scratch */
    size_t cap;
};

static long
qz(float v, double scale)
{
    if (v > 1.0f) v = 1.0f;
    if (v < -1.0f) v = -1.0f;
    return (long)(v * scale);
}

/* Copy sz host (LE) bytes of *src into *p, reversed when big-endian. */
static void
put(uint8_t **p, const void *src, size_t sz, int be)
{
    const uint8_t *s = src;
    for (size_t k = 0; k < sz; k++)
        (*p)[k] = be ? s[sz - 1 - k] : s[k];
    *p += sz;
}

static void
put_at(uint8_t *h, size_t off, const void *src, size_t sz, int be)
{
    uint8_t *p = h + off;
    put(&p, src, sz, be);
}

static int
grow(wfm_writer_t *w, size_t need)
{
    if (w->cap >= need)
        return 0;
    uint8_t *q = realloc(w->buf, need);
    if (!q)
        return -1;
    w->buf = q;
    w->cap = need;
    return 0;
}

/* Write the 512-byte BLUE type-1000 header. */
static int
write_blue_header(wfm_writer_t *w, double fs, size_t total_samples)
{
    uint8_t h[512] = {0};
    int be = w->be;
    memcpy(h + 0, "BLUE", 4);
    memcpy(h + 4, be ? "IEEE" : "EEEI", 4); /* head_rep */
    memcpy(h + 8, be ? "IEEE" : "EEEI", 4); /* data_rep */
    double data_start = 512.0;
    put_at(h, 32, &data_start, 8, be);
    double data_size = (double)(total_samples * BPS[w->stype]);
    put_at(h, 40, &data_size, 8, be);
    int32_t type = 1000;
    put_at(h, 48, &type, 4, be);
    h[52] = 'C'; /* complex mode */
    h[53] = FMTCH[w->stype];
    /* type-1000 adjunct at offset 256 */
    double xstart = 0.0;
    put_at(h, 256, &xstart, 8, be);
    double xdelta = (fs != 0.0) ? 1.0 / fs : 0.0;
    put_at(h, 264, &xdelta, 8, be);
    int32_t xunits = 1; /* seconds (time) */
    put_at(h, 272, &xunits, 4, be);
    return fwrite(h, 1, 512, w->fp) == 512 ? 0 : -1;
}

wfm_writer_t *
wfm_writer_open(FILE *fp, wfm_filetype_t ft, int sample_type, int endian,
                double fs, double fc, size_t total_samples)
{
    (void)fc;
    if (!fp || sample_type < 0 || sample_type > 4 || ft < 0 || ft > 3)
        return NULL;
    wfm_writer_t *w = calloc(1, sizeof(*w));
    if (!w)
        return NULL;
    w->fp = fp;
    w->ft = ft;
    w->stype = sample_type;
    w->be = endian ? 1 : 0;
    if (ft == WFM_FT_BLUE && write_blue_header(w, fs, total_samples)) {
        free(w);
        return NULL;
    }
    return w;
}

/* CSV: one complex sample per line. */
static size_t
write_csv(wfm_writer_t *w, const float _Complex *iq, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        float re = crealf(iq[i]), im = cimagf(iq[i]);
        int ok;
        if (w->stype == 0)
            ok = fprintf(w->fp, "%0.9f,%0.9f\n", (double)re, (double)im) > 0;
        else if (w->stype == 1)
            ok = fprintf(w->fp, "%0.17g,%0.17g\n", (double)re, (double)im) > 0;
        else
            ok = fprintf(w->fp, "%ld,%ld\n", qz(re, SCALE[w->stype]),
                         qz(im, SCALE[w->stype])) > 0;
        if (!ok)
            return i;
    }
    return n;
}

/* raw / blue body: interleaved I/Q in the wire type + byte order. */
static size_t
write_binary(wfm_writer_t *w, const float _Complex *iq, size_t n)
{
    size_t elem = ELEM[w->stype], be = w->be;
    if (grow(w, n * 2 * elem))
        return 0;
    uint8_t *p = w->buf;
    for (size_t i = 0; i < n; i++) {
        float re = crealf(iq[i]), im = cimagf(iq[i]);
        switch (w->stype) {
        case 0: { /* cf32 */
            put(&p, &re, 4, be);
            put(&p, &im, 4, be);
            break;
        }
        case 1: { /* cf64 */
            double dr = re, di = im;
            put(&p, &dr, 8, be);
            put(&p, &di, 8, be);
            break;
        }
        case 2: { /* ci32 */
            int32_t vr = (int32_t)qz(re, SCALE[2]);
            int32_t vi = (int32_t)qz(im, SCALE[2]);
            put(&p, &vr, 4, be);
            put(&p, &vi, 4, be);
            break;
        }
        case 3: { /* ci16 */
            int16_t vr = (int16_t)qz(re, SCALE[3]);
            int16_t vi = (int16_t)qz(im, SCALE[3]);
            put(&p, &vr, 2, be);
            put(&p, &vi, 2, be);
            break;
        }
        default: { /* ci8 */
            int8_t vr = (int8_t)qz(re, SCALE[4]);
            int8_t vi = (int8_t)qz(im, SCALE[4]);
            put(&p, &vr, 1, be);
            put(&p, &vi, 1, be);
            break;
        }
        }
    }
    size_t bytes = n * 2 * elem;
    return fwrite(w->buf, 1, bytes, w->fp) / (2 * elem);
}

size_t
wfm_writer_write(wfm_writer_t *w, const float _Complex *iq, size_t n)
{
    if (!w || (n && !iq))
        return 0;
    size_t done = (w->ft == WFM_FT_CSV) ? write_csv(w, iq, n)
                                        : write_binary(w, iq, n);
    w->written += done;
    return done;
}

int
wfm_writer_close(wfm_writer_t *w)
{
    int rc = 0;
    if (w) {
        /* patch BLUE data_size from the actual count when the stream seeks */
        if (w->ft == WFM_FT_BLUE && fseek(w->fp, 40, SEEK_SET) == 0) {
            double data_size = (double)(w->written * BPS[w->stype]);
            uint8_t b[8];
            uint8_t *p = b;
            put(&p, &data_size, 8, w->be);
            if (fwrite(b, 1, 8, w->fp) != 8)
                rc = -1;
            fseek(w->fp, 0, SEEK_END);
        }
        free(w->buf);
        free(w);
    }
    return rc;
}

/* SigMF "cf32_le"-style datatype string (ci8 has no endian suffix). */
static void
sigmf_datatype(int stype, int be, char *out, size_t cap)
{
    static const char *const base[5] = {"cf32", "cf64", "ci32", "ci16", "ci8"};
    if (stype == 4)
        snprintf(out, cap, "ci8");
    else
        snprintf(out, cap, "%s_%s", base[stype], be ? "be" : "le");
}

char *
wfm_sigmf_meta_json(int sample_type, int endian, double fs, double fc,
                    const wfm_segment_t *segs, size_t n_segs)
{
    cJSON *root = cJSON_CreateObject();
    if (!root)
        return NULL;

    cJSON *g = cJSON_AddObjectToObject(root, "global");
    char dt[16];
    sigmf_datatype(sample_type, endian, dt, sizeof dt);
    cJSON_AddStringToObject(g, "core:datatype", dt);
    cJSON_AddNumberToObject(g, "core:sample_rate", fs);
    cJSON_AddStringToObject(g, "core:version", "1.0.0");
    cJSON_AddStringToObject(g, "core:description", "doppler wfmgen");
    cJSON_AddStringToObject(g, "core:author", "doppler wfmgen");

    cJSON *caps = cJSON_AddArrayToObject(root, "captures");
    cJSON *cap0 = cJSON_CreateObject();
    cJSON_AddNumberToObject(cap0, "core:sample_start", 0);
    cJSON_AddNumberToObject(cap0, "core:frequency", fc);
    cJSON_AddItemToArray(caps, cap0);

    cJSON *anns = cJSON_AddArrayToObject(root, "annotations");
    size_t start = 0;
    for (size_t i = 0; i < n_segs; i++) {
        const wfm_segment_t *s = &segs[i];
        cJSON *a = cJSON_CreateObject();
        cJSON_AddNumberToObject(a, "core:sample_start", (double)start);
        cJSON_AddNumberToObject(a, "core:sample_count", (double)s->num_samples);
        double bw = (s->type >= 2 && s->sps > 0) ? s->fs / (double)s->sps : 0.0;
        double center = fc + s->freq;
        cJSON_AddNumberToObject(a, "core:freq_lower_edge", center - bw / 2.0);
        cJSON_AddNumberToObject(a, "core:freq_upper_edge", center + bw / 2.0);
        if (s->type >= 0 && s->type < 5)
            cJSON_AddStringToObject(a, "core:label", TYPE_NAMES[s->type]);
        cJSON_AddNumberToObject(a, "wfmgen:snr", s->snr);
        if (s->snr_mode >= 0 && s->snr_mode < 4)
            cJSON_AddStringToObject(a, "wfmgen:snr_mode",
                                    MODE_NAMES[s->snr_mode]);
        cJSON_AddNumberToObject(a, "wfmgen:sps", s->sps);
        cJSON_AddNumberToObject(a, "wfmgen:seed", s->seed);
        cJSON_AddNumberToObject(a, "wfmgen:pn_length", s->pn_length);
        cJSON_AddNumberToObject(a, "wfmgen:pn_poly", s->pn_poly);
        cJSON_AddItemToArray(anns, a);
        start += s->num_samples + s->off_samples;
    }

    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return out;
}
