/*
 * wfm_json.c — JSON spec (de)serialisation for the composer (Phase B).
 *
 * One canonical, sample-exact schema shared by `--record` (write) and
 * `--from-file` (read), so a recorded run reproduces byte-for-byte. Uses the
 * vendored cJSON.
 */
#include "wfmgen/wfm_compose.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"

static const char *const TYPE_NAMES[] = {"tone", "noise", "pn", "bpsk",
                                         "qpsk"};
static const char *const MODE_NAMES[] = {"auto", "fs", "ebno", "esno"};

static int
name_index(const char *s, const char *const *names, int n)
{
    if (s)
        for (int i = 0; i < n; i++)
            if (strcmp(s, names[i]) == 0)
                return i;
    return -1;
}

/* cJSON number field with a fallback when absent/non-numeric. */
static double
num(const cJSON *obj, const char *key, double fallback)
{
    const cJSON *it = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsNumber(it) ? it->valuedouble : fallback;
}

char *
wfm_spec_to_json(const wfm_segment_t *segs, size_t n_segs, int repeat,
                 int continuous)
{
    cJSON *root = cJSON_CreateObject();
    if (!root)
        return NULL;
    cJSON_AddStringToObject(root, "version", "wfmgen-1");
    cJSON_AddBoolToObject(root, "repeat", repeat != 0);
    cJSON_AddBoolToObject(root, "continuous", continuous != 0);
    cJSON *arr = cJSON_AddArrayToObject(root, "segments");
    for (size_t i = 0; i < n_segs; i++) {
        const wfm_segment_t *g = &segs[i];
        cJSON *s = cJSON_CreateObject();
        int t = (g->type >= 0 && g->type < 5) ? g->type : 0;
        int m = (g->snr_mode >= 0 && g->snr_mode < 4) ? g->snr_mode : 0;
        cJSON_AddStringToObject(s, "type", TYPE_NAMES[t]);
        cJSON_AddNumberToObject(s, "fs", g->fs);
        cJSON_AddNumberToObject(s, "freq", g->freq);
        cJSON_AddNumberToObject(s, "snr", g->snr);
        cJSON_AddStringToObject(s, "snr_mode", MODE_NAMES[m]);
        cJSON_AddNumberToObject(s, "seed", (double)g->seed);
        cJSON_AddNumberToObject(s, "sps", g->sps);
        cJSON_AddNumberToObject(s, "pn_length", g->pn_length);
        cJSON_AddNumberToObject(s, "pn_poly", (double)g->pn_poly);
        cJSON_AddNumberToObject(s, "num_samples", (double)g->num_samples);
        cJSON_AddNumberToObject(s, "off_samples", (double)g->off_samples);
        cJSON_AddItemToArray(arr, s);
    }
    char *out = cJSON_Print(root);
    cJSON_Delete(root);
    return out;
}

wfm_compose_state_t *
wfm_compose_from_json(const char *json)
{
    cJSON *root = cJSON_Parse(json);
    if (!root)
        return NULL;
    const cJSON *arr = cJSON_GetObjectItemCaseSensitive(root, "segments");
    if (!cJSON_IsArray(arr) || cJSON_GetArraySize(arr) == 0) {
        cJSON_Delete(root);
        return NULL;
    }
    int repeat = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(root, "repeat"));
    int cont =
        cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(root, "continuous"));
    size_t n = (size_t)cJSON_GetArraySize(arr);
    wfm_segment_t *segs = calloc(n, sizeof(*segs));
    if (!segs) {
        cJSON_Delete(root);
        return NULL;
    }
    size_t i = 0;
    const cJSON *s = NULL;
    cJSON_ArrayForEach(s, arr)
    {
        const cJSON *ty = cJSON_GetObjectItemCaseSensitive(s, "type");
        int t = name_index(cJSON_GetStringValue(ty), TYPE_NAMES, 5);
        if (t < 0) { /* unknown/missing waveform type → reject the whole spec */
            free(segs);
            cJSON_Delete(root);
            return NULL;
        }
        const cJSON *md = cJSON_GetObjectItemCaseSensitive(s, "snr_mode");
        int m = name_index(cJSON_GetStringValue(md), MODE_NAMES, 4);
        segs[i] = (wfm_segment_t){
            .type = t,
            .fs = num(s, "fs", 1000000.0),
            .freq = num(s, "freq", 0.0),
            .snr = num(s, "snr", 100.0),
            .snr_mode = (m < 0) ? 0 : m,
            .seed = (uint32_t)num(s, "seed", 1),
            .sps = (int)num(s, "sps", 8),
            .pn_length = (int)num(s, "pn_length", 7),
            .pn_poly = (uint32_t)num(s, "pn_poly", 0),
            .num_samples = (size_t)num(s, "num_samples", 0),
            .off_samples = (size_t)num(s, "off_samples", 0),
        };
        i++;
    }
    cJSON_Delete(root);
    wfm_compose_state_t *c = wfm_compose_create(segs, n, repeat, cont);
    free(segs);
    return c;
}

wfm_compose_state_t *
wfm_compose_from_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    if (len < 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    char *buf = malloc((size_t)len + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    size_t rd = fread(buf, 1, (size_t)len, f);
    fclose(f);
    buf[rd] = '\0';
    wfm_compose_state_t *c = wfm_compose_from_json(buf);
    free(buf);
    return c;
}
