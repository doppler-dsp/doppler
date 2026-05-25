#include "i16_to_f32/i16_to_f32_core.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(cond) \
    do { if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
        _fails++; \
    } } while (0)

static inline int _feq(float a, float b, float tol)
    { return fabsf(a - b) <= tol; }

int main(void)
{
    int _fails = 0;

    /* ── Invalid args → NULL ──────────────────────────────────────────────── */
    CHECK(i16_to_f32_create(0.0f)  == NULL);
    CHECK(i16_to_f32_create(-1.0f) == NULL);

    /* ── Q15 boundary values ──────────────────────────────────────────────── */
    {
        i16_to_f32_state_t *obj = i16_to_f32_create(32768.0f);
        CHECK(obj != NULL);
        CHECK(_feq(i16_to_f32_step(obj,  32767),  32767.0f / 32768.0f, 1e-6f));
        CHECK(_feq(i16_to_f32_step(obj, -32768), -1.0f, 1e-6f));
        CHECK(_feq(i16_to_f32_step(obj,  0),      0.0f, 1e-7f));
        i16_to_f32_destroy(obj);
    }

    /* ── Custom scale ─────────────────────────────────────────────────────── */
    {
        i16_to_f32_state_t *obj = i16_to_f32_create(1.0f);
        CHECK(obj != NULL);
        CHECK(_feq(i16_to_f32_step(obj, 100), 100.0f, 1e-6f));
        CHECK(_feq(i16_to_f32_step(obj, -50), -50.0f, 1e-6f));
        i16_to_f32_destroy(obj);
    }

    /* ── Midscale: 0x4000 (16384) → 0.5 ─────────────────────────────────── */
    {
        i16_to_f32_state_t *obj = i16_to_f32_create(32768.0f);
        CHECK(obj != NULL);
        CHECK(_feq(i16_to_f32_step(obj, 16384), 0.5f, 1.0f / 32768.0f));
        i16_to_f32_destroy(obj);
    }

    /* ── steps() matches per-sample step() loop ──────────────────────────── */
    {
        i16_to_f32_state_t *obj_bulk = i16_to_f32_create(32768.0f);
        i16_to_f32_state_t *obj_loop = i16_to_f32_create(32768.0f);
        CHECK(obj_bulk && obj_loop);

        int16_t input[64];
        float out_bulk[64], out_loop[64];
        for (int i = 0; i < 64; i++)
            input[i] = (int16_t)(-32768 + i * 1024);

        i16_to_f32_steps(obj_bulk, input, out_bulk, 64);
        for (int i = 0; i < 64; i++)
            out_loop[i] = i16_to_f32_step(obj_loop, input[i]);

        CHECK(memcmp(out_bulk, out_loop, 64 * sizeof(float)) == 0);
        i16_to_f32_destroy(obj_bulk);
        i16_to_f32_destroy(obj_loop);
    }

    if (_fails) {
        fprintf(stderr, "test_i16_to_f32_core FAILED (%d)\n", _fails);
        return 1;
    }
    printf("test_i16_to_f32_core PASSED\n");
    return 0;
}
