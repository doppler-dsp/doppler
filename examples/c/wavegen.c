// doppler — wavegen: waveform generator (stand-alone C binary).
//
// Scaffolded by `just-makeit app`, then hand-owned: builds a synth from CLI
// flags, generates IQ in blocks, converts to the requested wire sample type,
// and streams to a file or stdout.
//
//   wavegen --type tone --freq 0.1 --snr 20 --count 65536 \
//           --sample-type ci16 -o tone.ci16
//
// Sample types (interleaved I/Q): cf32 (float, default), cf64 (double),
// ci32/ci16/ci8 (signed int, full-scale = 1.0).

#include <complex.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "synth/synth_core.h"

enum { ST_CF32, ST_CF64, ST_CI32, ST_CI16, ST_CI8 };

static int
parse_sample_type(const char *s)
{
    if (!strcmp(s, "cf32")) return ST_CF32;
    if (!strcmp(s, "cf64")) return ST_CF64;
    if (!strcmp(s, "ci32")) return ST_CI32;
    if (!strcmp(s, "ci16")) return ST_CI16;
    if (!strcmp(s, "ci8")) return ST_CI8;
    return -1;
}

static int
parse_wave_type(const char *s)
{
    if (!strcmp(s, "tone")) return SYNTH_TONE;
    if (!strcmp(s, "noise")) return SYNTH_NOISE;
    return -1;
}

/* Clamp v to [-1,1], scale to a signed integer of full-scale `fs_val`. */
static long
quantize(float v, double fs_val)
{
    if (v > 1.0f) v = 1.0f;
    if (v < -1.0f) v = -1.0f;
    return (long)(v * fs_val);
}

/* Convert a cf32 block to `st` into `bytes`; returns bytes written. */
static size_t
convert_block(const float _Complex *in, size_t n, int st, unsigned char *bytes)
{
    switch (st) {
    case ST_CF32:
        memcpy(bytes, in, n * sizeof(float _Complex));
        return n * sizeof(float _Complex);
    case ST_CF64: {
        double _Complex *o = (double _Complex *)bytes;
        for (size_t i = 0; i < n; i++)
            o[i] = (double)crealf(in[i]) + (double)cimagf(in[i]) * I;
        return n * sizeof(double _Complex);
    }
    case ST_CI32: {
        int32_t *o = (int32_t *)bytes;
        for (size_t i = 0; i < n; i++) {
            o[2 * i] = (int32_t)quantize(crealf(in[i]), 2147483647.0);
            o[2 * i + 1] = (int32_t)quantize(cimagf(in[i]), 2147483647.0);
        }
        return n * 2 * sizeof(int32_t);
    }
    case ST_CI16: {
        int16_t *o = (int16_t *)bytes;
        for (size_t i = 0; i < n; i++) {
            o[2 * i] = (int16_t)quantize(crealf(in[i]), 32767.0);
            o[2 * i + 1] = (int16_t)quantize(cimagf(in[i]), 32767.0);
        }
        return n * 2 * sizeof(int16_t);
    }
    case ST_CI8: {
        int8_t *o = (int8_t *)bytes;
        for (size_t i = 0; i < n; i++) {
            o[2 * i] = (int8_t)quantize(crealf(in[i]), 127.0);
            o[2 * i + 1] = (int8_t)quantize(cimagf(in[i]), 127.0);
        }
        return n * 2 * sizeof(int8_t);
    }
    }
    return 0;
}

static void
usage(void)
{
    fprintf(stderr,
            "usage: wavegen [--type tone|noise] [--freq F] [--snr DB]\n"
            "               [--fs HZ] [--seed N] [--count N]\n"
            "               [--sample-type cf32|cf64|ci32|ci16|ci8]\n"
            "               [--output FILE | -o FILE]\n"
            "  --freq is the normalised frequency offset (cycles/sample).\n");
}

int
main(int argc, char *argv[])
{
    int type = SYNTH_TONE;
    double fs = 1000000.0, freq = 0.0, snr_db = 100.0;
    uint32_t seed = 1;
    size_t count = 65536;
    int st = ST_CF32;
    const char *out_path = NULL;

#define NEED_ARG(name)                                                     \
    if (i + 1 >= argc) {                                                   \
        fprintf(stderr, "error: %s needs a value\n", name);               \
        return 2;                                                         \
    }
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--type")) {
            NEED_ARG("--type");
            type = parse_wave_type(argv[++i]);
            if (type < 0) { fprintf(stderr, "error: bad --type\n"); return 2; }
        } else if (!strcmp(argv[i], "--freq")) {
            NEED_ARG("--freq"); freq = strtod(argv[++i], NULL);
        } else if (!strcmp(argv[i], "--fs")) {
            NEED_ARG("--fs"); fs = strtod(argv[++i], NULL);
        } else if (!strcmp(argv[i], "--snr")) {
            NEED_ARG("--snr"); snr_db = strtod(argv[++i], NULL);
        } else if (!strcmp(argv[i], "--seed")) {
            NEED_ARG("--seed"); seed = (uint32_t)strtoul(argv[++i], NULL, 10);
        } else if (!strcmp(argv[i], "--count")) {
            NEED_ARG("--count"); count = (size_t)strtoull(argv[++i], NULL, 10);
        } else if (!strcmp(argv[i], "--sample-type")) {
            NEED_ARG("--sample-type");
            st = parse_sample_type(argv[++i]);
            if (st < 0) {
                fprintf(stderr, "error: bad --sample-type\n");
                return 2;
            }
        } else if (!strcmp(argv[i], "--output") || !strcmp(argv[i], "-o")) {
            NEED_ARG("--output"); out_path = argv[++i];
        } else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            usage();
            return 0;
        } else {
            fprintf(stderr, "error: unknown arg %s\n", argv[i]);
            usage();
            return 2;
        }
    }
#undef NEED_ARG

    FILE *out = out_path ? fopen(out_path, "wb") : stdout;
    if (!out) {
        fprintf(stderr, "error: cannot open %s\n", out_path);
        return 1;
    }

    synth_state_t *state = synth_create(type, fs, freq, snr_db, seed);
    if (!state) {
        fprintf(stderr, "error: synth_create() failed\n");
        if (out != stdout) fclose(out);
        return 1;
    }

    float _Complex outbuf[4096];
    unsigned char bytes[4096 * sizeof(double _Complex)]; /* widest type */
    size_t produced = 0;
    while (produced < count) {
        size_t k = (count - produced) < 4096 ? (count - produced) : 4096;
        synth_steps(state, outbuf, k);
        size_t nb = convert_block(outbuf, k, st, bytes);
        fwrite(bytes, 1, nb, out);
        produced += k;
    }

    synth_destroy(state);
    if (out != stdout) fclose(out);
    return 0;
}
