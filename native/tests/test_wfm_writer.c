/*
 * test_wfm_writer.c — raw/csv/BLUE writers + SigMF meta (Phase C).
 *
 * Uses tmpfile() (seekable) so the BLUE data_size patch-on-close is exercised.
 * Host is little-endian (x86), matching the writer's assumption.
 */
#include "wfmgen/wfm_writer.h"

#include <complex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c, m)                                                        \
    do {                                                                   \
        if (!(c)) {                                                        \
            fprintf(stderr, "FAIL: %s\n", m);                             \
            return 1;                                                     \
        }                                                                  \
    } while (0)

/* read the whole tmpfile into buf, return byte length */
static size_t
slurp(FILE *fp, uint8_t *buf, size_t cap)
{
    fflush(fp);
    fseek(fp, 0, SEEK_SET);
    return fread(buf, 1, cap, fp);
}

int
main(void)
{
    uint8_t bytes[1024];

    /* ── raw cf32 LE: interleaved float I/Q, host order ── */
    {
        float _Complex s[2] = {1.0f + 2.0f * I, -1.0f - 2.0f * I};
        FILE *fp = tmpfile();
        wfm_writer_t *w = wfm_writer_open(fp, WFM_FT_RAW, 0, 0, 1e6, 0, 2);
        CHECK(w, "raw open");
        CHECK(wfm_writer_write(w, s, 2) == 2, "raw write");
        CHECK(wfm_writer_close(w) == 0, "raw close");
        size_t nb = slurp(fp, bytes, sizeof bytes);
        CHECK(nb == 16, "raw cf32 byte count");
        float f[4];
        memcpy(f, bytes, 16);
        CHECK(f[0] == 1.0f && f[1] == 2.0f && f[2] == -1.0f && f[3] == -2.0f,
              "raw cf32 interleaved values");
        fclose(fp);
    }

    /* ── endian: ci16 BE is the byte-reverse of ci16 LE ── */
    {
        float _Complex s[1] = {0.5f - 0.5f * I};
        uint8_t le[4], be[4];
        FILE *fl = tmpfile(), *fb = tmpfile();
        wfm_writer_t *wl = wfm_writer_open(fl, WFM_FT_RAW, 3, 0, 1e6, 0, 1);
        wfm_writer_t *wb = wfm_writer_open(fb, WFM_FT_RAW, 3, 1, 1e6, 0, 1);
        wfm_writer_write(wl, s, 1);
        wfm_writer_write(wb, s, 1);
        wfm_writer_close(wl);
        wfm_writer_close(wb);
        CHECK(slurp(fl, le, 4) == 4 && slurp(fb, be, 4) == 4, "ci16 sizes");
        /* two 2-byte elements, each reversed */
        CHECK(be[0] == le[1] && be[1] == le[0], "ci16 BE I swapped");
        CHECK(be[2] == le[3] && be[3] == le[2], "ci16 BE Q swapped");
        fclose(fl);
        fclose(fb);
    }

    /* ── csv cf32: one "%0.9f,%0.9f" line per sample ── */
    {
        float _Complex s[1] = {0.25f + (-0.5f) * I};
        FILE *fp = tmpfile();
        wfm_writer_t *w = wfm_writer_open(fp, WFM_FT_CSV, 0, 0, 1e6, 0, 1);
        CHECK(wfm_writer_write(w, s, 1) == 1, "csv write");
        wfm_writer_close(w);
        size_t nb = slurp(fp, bytes, sizeof bytes - 1);
        bytes[nb] = 0;
        CHECK(strcmp((char *)bytes, "0.250000000,-0.500000000\n") == 0,
              "csv cf32 line");
        fclose(fp);
    }

    /* ── BLUE type-1000 header + data_size patch on close ── */
    {
        float _Complex s[2] = {1.0f + 0.0f * I, 0.0f + 1.0f * I};
        FILE *fp = tmpfile();
        /* total unknown at open (0) → close must patch it */
        wfm_writer_t *w = wfm_writer_open(fp, WFM_FT_BLUE, 0, 0, 1e6, 0, 0);
        CHECK(w, "blue open");
        wfm_writer_write(w, s, 2);
        CHECK(wfm_writer_close(w) == 0, "blue close");
        size_t nb = slurp(fp, bytes, sizeof bytes);
        CHECK(nb == 512 + 16, "blue header+data size");
        CHECK(memcmp(bytes, "BLUE", 4) == 0, "blue magic");
        CHECK(memcmp(bytes + 8, "EEEI", 4) == 0, "blue data_rep LE");
        int32_t type;
        memcpy(&type, bytes + 48, 4);
        CHECK(type == 1000, "blue type 1000");
        CHECK(bytes[52] == 'C' && bytes[53] == 'F', "blue format CF");
        double xdelta;
        memcpy(&xdelta, bytes + 264, 8);
        CHECK(xdelta == 1e-6, "blue xdelta = 1/fs");
        double data_size;
        memcpy(&data_size, bytes + 40, 8);
        CHECK(data_size == 16.0, "blue data_size patched (2*cf32=16)");
        fclose(fp);
    }

    /* ── SigMF meta: datatype + one annotation per segment ── */
    {
        wfm_segment_t segs[2] = {
            {.type = 0, .fs = 1e6, .freq = 1e5, .snr = 100.0, .snr_mode = 1,
             .seed = 1, .sps = 8, .pn_length = 7, .pn_poly = 0,
             .num_samples = 1000, .off_samples = 500},
            {.type = 4, .fs = 1e6, .freq = 0, .snr = 9.0, .snr_mode = 3,
             .seed = 1, .sps = 8, .pn_length = 7, .pn_poly = 0,
             .num_samples = 4096, .off_samples = 0},
        };
        char *j = wfm_sigmf_meta_json(3, 0, 1e6, 2.4e9, segs, 2);
        CHECK(j, "sigmf meta");
        CHECK(strstr(j, "\"core:datatype\":\"ci16_le\""), "sigmf datatype");
        CHECK(strstr(j, "\"core:sample_rate\":1000000"), "sigmf rate");
        CHECK(strstr(j, "\"core:label\":\"tone\"")
                  && strstr(j, "\"core:label\":\"qpsk\""),
              "sigmf per-segment labels");
        CHECK(strstr(j, "\"core:sample_start\":1500"), "sigmf 2nd seg start");
        CHECK(strstr(j, "\"wfmgen:snr\":9"), "sigmf custom snr");
        free(j);
    }

    printf("test_wfm_writer: OK (raw/endian/csv/blue + sigmf meta)\n");
    return 0;
}
