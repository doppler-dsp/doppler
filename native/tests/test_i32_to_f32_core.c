#include "i32_to_f32/i32_to_f32_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>

#define CHECK(cond) \
    do { if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
        _fails++; \
    } } while (0)

/* Floating-point helpers — use inline functions, not macros, so arguments
 * are evaluated exactly once.  Safe to call with stateful step() results. */
static inline int _almost_eq(float a, float b, float tol)
    { return fabsf(a - b) <= tol; }
static inline int _almost_eq_c(float complex a, float complex b, float tol)
    { return _almost_eq(crealf(a), crealf(b), tol)
          && _almost_eq(cimagf(a), cimagf(b), tol); }
#define ALMOST_EQ(a, b, tol)   _almost_eq((float)(a),         (float)(b),         tol)
#define ALMOST_EQ_C(a, b, tol) _almost_eq_c((float complex)(a), (float complex)(b), tol)

int main(void)
{
    int _fails = 0;
    CHECK(i32_to_f32_create(0.0f)  == NULL);
    CHECK(i32_to_f32_create(-1.0f) == NULL);
    i32_to_f32_state_t *obj = i32_to_f32_create(2147483648.0f);
    CHECK(obj != NULL);
    if (!obj) return 1;

    /* full-scale boundaries: +max -> ~+1, -min -> -1, 0 -> 0 */
    CHECK(ALMOST_EQ(i32_to_f32_step(obj, INT32_MAX),
                    2147483647.0f / 2147483648.0f, 1e-6f));
    CHECK(ALMOST_EQ(i32_to_f32_step(obj, INT32_MIN), -1.0f, 1e-6f));
    CHECK(ALMOST_EQ(i32_to_f32_step(obj, 0),          0.0f, 1e-7f));

    /* reset */
    i32_to_f32_reset(obj);

    i32_to_f32_destroy(obj);
    if (_fails) {
        fprintf(stderr, "test_i32_to_f32_core FAILED (%d)\n", _fails);
        return 1;
    }
    printf("test_i32_to_f32_core PASSED\n");
    return 0;
}
