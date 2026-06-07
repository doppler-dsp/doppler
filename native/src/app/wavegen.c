// doppler — wavegen: Synth-powered stream tool.
// Scaffolded by just-makeit.  Build:  make && ./build/wavegen
// Re-running `just-makeit app` overwrites this file; edit for custom logic.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "synth/synth_core.h"
static int
jm_parse_type(const char *s)
{
    if (!strcmp(s, "tone")) return 0;
    if (!strcmp(s, "noise")) return 1;
    if (!strcmp(s, "pn")) return 2;
    if (!strcmp(s, "bpsk")) return 3;
    if (!strcmp(s, "qpsk")) return 4;
    return -1;
}

static int
jm_parse_snr_mode(const char *s)
{
    if (!strcmp(s, "auto")) return 0;
    if (!strcmp(s, "fs")) return 1;
    if (!strcmp(s, "ebno")) return 2;
    if (!strcmp(s, "esno")) return 3;
    return -1;
}

static int
jm_parse_sample_type(const char *s)
{
    if (!strcmp(s, "cf32")) return 0;
    if (!strcmp(s, "cf64")) return 1;
    if (!strcmp(s, "ci32")) return 2;
    if (!strcmp(s, "ci16")) return 3;
    if (!strcmp(s, "ci8")) return 4;
    return -1;
}

#include <complex.h>

/* Clamp v to [-1, 1] and scale to a signed integer of full-scale fs_val. */
static long
jm_q(float v, double fs_val)
{
    if (v > 1.0f) v = 1.0f;
    if (v < -1.0f) v = -1.0f;
    return (long)(v * fs_val);
}

/* Convert a cf32 block to the selected wire type into `bytes` (interleaved
   I/Q); returns bytes written.  0=cf32 1=cf64 2=ci32 3=ci16 4=ci8. */
static size_t
jm_convert_block(const float _Complex *in, size_t n, int st,
                 unsigned char *bytes)
{
    switch (st) {
    case 1: {
        double _Complex *o = (double _Complex *)bytes;
        for (size_t i = 0; i < n; i++)
            o[i] = (double)crealf(in[i]) + (double)cimagf(in[i]) * I;
        return n * sizeof(double _Complex);
    }
    case 2: {
        int32_t *o = (int32_t *)bytes;
        for (size_t i = 0; i < n; i++) {
            o[2 * i] = (int32_t)jm_q(crealf(in[i]), 2147483647.0);
            o[2 * i + 1] = (int32_t)jm_q(cimagf(in[i]), 2147483647.0);
        }
        return n * 2 * sizeof(int32_t);
    }
    case 3: {
        int16_t *o = (int16_t *)bytes;
        for (size_t i = 0; i < n; i++) {
            o[2 * i] = (int16_t)jm_q(crealf(in[i]), 32767.0);
            o[2 * i + 1] = (int16_t)jm_q(cimagf(in[i]), 32767.0);
        }
        return n * 2 * sizeof(int16_t);
    }
    case 4: {
        int8_t *o = (int8_t *)bytes;
        for (size_t i = 0; i < n; i++) {
            o[2 * i] = (int8_t)jm_q(crealf(in[i]), 127.0);
            o[2 * i + 1] = (int8_t)jm_q(cimagf(in[i]), 127.0);
        }
        return n * 2 * sizeof(int8_t);
    }
    default: /* 0 = cf32: raw passthrough */
        memcpy(bytes, in, n * sizeof(float _Complex));
        return n * sizeof(float _Complex);
    }
}

int
main(int argc, char *argv[])
{
    /* --- parse args ------------------------------------------------------ */
    int type = 0;
    double fs = 1000000.0;
    double freq = 0.0;
    double snr = 100.0;
    int snr_mode = 0;
    uint32_t seed = 1;
    int sps = 8;
    int pn_length = 7;
    uint32_t pn_poly = 0;
    size_t count = 1024;
    int sample_type = 0;
    const char *out_path = NULL;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            fputs("usage: wavegen [--type tone|noise|pn|bpsk|qpsk] [--fs V] [--freq V] [--snr V] [--snr_mode auto|fs|ebno|esno] [--seed V] [--sps V] [--pn_length V] [--pn_poly V] [--count V] [--sample_type cf32|cf64|ci32|ci16|ci8] [--output FILE]\n  --type tone|noise|pn|bpsk|qpsk\n  --fs V\n  --freq V\n  --snr V\n  --snr_mode auto|fs|ebno|esno\n  --seed V\n  --sps V\n  --pn_length V\n  --pn_poly V\n  --count V                 number of samples to generate\n  --sample_type cf32|cf64|ci32|ci16|ci8  output wire sample type\n  --output, -o FILE         output file (default: stdout)\n", stdout);
            return 0;
        } else if (!strcmp(argv[i], "--type") && i + 1 < argc) {
            type = jm_parse_type(argv[++i]);
            if (type < 0) {
                fprintf(stderr, "error: --type must be one of: tone noise pn bpsk qpsk\n");
                return 2;
            }
        } else if (!strcmp(argv[i], "--fs") && i + 1 < argc) {
            fs = strtod(argv[++i], NULL);
        } else if (!strcmp(argv[i], "--freq") && i + 1 < argc) {
            freq = strtod(argv[++i], NULL);
        } else if (!strcmp(argv[i], "--snr") && i + 1 < argc) {
            snr = strtod(argv[++i], NULL);
        } else if (!strcmp(argv[i], "--snr_mode") && i + 1 < argc) {
            snr_mode = jm_parse_snr_mode(argv[++i]);
            if (snr_mode < 0) {
                fprintf(stderr, "error: --snr_mode must be one of: auto fs ebno esno\n");
                return 2;
            }
        } else if (!strcmp(argv[i], "--seed") && i + 1 < argc) {
            seed = (uint32_t)strtoul(argv[++i], NULL, 10);
        } else if (!strcmp(argv[i], "--sps") && i + 1 < argc) {
            sps = (int)strtol(argv[++i], NULL, 10);
        } else if (!strcmp(argv[i], "--pn_length") && i + 1 < argc) {
            pn_length = (int)strtol(argv[++i], NULL, 10);
        } else if (!strcmp(argv[i], "--pn_poly") && i + 1 < argc) {
            pn_poly = (uint32_t)strtoul(argv[++i], NULL, 10);
        } else if (!strcmp(argv[i], "--count") && i + 1 < argc) {
            count = (size_t)strtoull(argv[++i], NULL, 10);
        } else if (!strcmp(argv[i], "--sample_type") && i + 1 < argc) {
            sample_type = jm_parse_sample_type(argv[++i]);
            if (sample_type < 0) {
                fprintf(stderr, "error: --sample_type must be one of: cf32 cf64 ci32 ci16 ci8\n");
                return 2;
            }
        } else if ((!strcmp(argv[i], "--output") || !strcmp(argv[i], "-o"))
                   && i + 1 < argc) {
            out_path = argv[++i];
        } else {
            fprintf(stderr, "usage: wavegen [--type tone|noise|pn|bpsk|qpsk] [--fs V] [--freq V] [--snr V] [--snr_mode auto|fs|ebno|esno] [--seed V] [--sps V] [--pn_length V] [--pn_poly V] [--count V] [--sample_type cf32|cf64|ci32|ci16|ci8] [--output FILE]\n");
            return 2;
        }
    }

    FILE *out = out_path ? fopen(out_path, "wb") : stdout;
    if (!out) {
        fprintf(stderr, "error: cannot open input/output\n");
        return 1;
    }

    /* --- create ---------------------------------------------------------- */
    synth_state_t *state = synth_create(type, fs, freq, snr, snr_mode, seed, sps, pn_length, pn_poly);
    if (!state) {
        fprintf(stderr, "error: synth_create() failed\n");
        return 1;
    }

    /* --- process --------------------------------------------------------- */
    float _Complex outbuf[4096];
    unsigned char jm_bytes[4096 * sizeof(double _Complex)];
    size_t produced = 0;
    while (produced < count) {
        size_t k = (count - produced) < 4096
                       ? (count - produced) : (size_t)4096;
        synth_steps(state, outbuf, k);
        size_t nb = jm_convert_block(outbuf, k, sample_type, jm_bytes);
        fwrite(jm_bytes, 1, nb, out);
        produced += k;
    }

    /* --- cleanup --------------------------------------------------------- */
    synth_destroy(state);
    if (out != stdout) fclose(out);
    return 0;
}
