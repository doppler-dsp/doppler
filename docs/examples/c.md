# C Examples

## NCO

### Free-running IQ

```c
#include <dp/nco.h>
#include <stdio.h>

int main(void) {
    dp_nco_t *nco = dp_nco_create(0.25f);  // quarter-rate tone

    dp_cf32_t out[8];
    dp_nco_execute_cf32(nco, out, 8);

    for (int i = 0; i < 8; i++)
        printf("out[%d]: %.3f + %.3fi\n", i, out[i].i, out[i].q);
    // out[0]:  1.000 + 0.000i
    // out[1]:  0.000 + 1.000i
    // out[2]: -1.000 + 0.000i
    // out[3]:  0.000 - 1.000i
    // out[4]:  1.000 + 0.000i  (repeats every 4 samples)
    // ...

    dp_nco_destroy(nco);
    return 0;
}
```

### Raw uint32 phase + overflow carry

```c
dp_nco_t *nco = dp_nco_create(0.25f);

uint32_t phase[16];
uint8_t  carry[16];
dp_nco_execute_u32_ovf(nco, phase, carry, 16);
// carry fires at indices 3, 7, 11, 15 (once per full cycle)

dp_nco_destroy(nco);
```

### FM modulation via control port

```c
dp_nco_t *nco = dp_nco_create(0.1f);   // base freq f_n = 0.1

float ctrl[1024];
for (int i = 0; i < 1024; i++)
    ctrl[i] = 0.002f * sinf(2.0f * M_PI * 0.01f * i);  // FM deviation

dp_cf32_t out[1024];
dp_nco_execute_cf32_ctrl(nco, ctrl, out, 1024);
// base freq unchanged; reset restores clean phase
dp_nco_destroy(nco);
```

---

## FIR filter

```c
#include <dp/fir.h>
#include <math.h>

#define N_TAPS 19

int main(void) {
    // Windowed-sinc low-pass filter (fc = 0.2 * fs)
    dp_cf32_t taps[N_TAPS];
    int half = N_TAPS / 2;
    for (int k = 0; k < N_TAPS; k++) {
        int    n    = k - half;
        double sinc = (n == 0) ? 1.0
                                : sin(M_PI * 0.2 * n) / (M_PI * 0.2 * n);
        double win  = 0.5 * (1.0 - cos(2.0 * M_PI * k / (N_TAPS - 1)));
        taps[k].i = (float)(sinc * win);
        taps[k].q = 0.0f;
    }

    dp_fir_t *fir = dp_fir_create(taps, N_TAPS);

    // CF32 input
    dp_cf32_t in[1024], out[1024];
    dp_fir_execute_cf32(fir, in, out, 1024);

    // CI16 input (SDR-style 16-bit IQ)
    dp_ci16_t in16[1024];
    dp_fir_execute_ci16(fir, in16, out, 1024);

    dp_fir_destroy(fir);
    return 0;
}
```

---

## FFT

### 1D FFT (out-of-place)

```c
#include <doppler.h>
#include <dp/fft.h>
#include <complex.h>
#include <math.h>
#include <stdio.h>

int main(void) {
    dp_init();

    const size_t N = 1024;
    size_t shape[] = {N};
    dp_fft_global_setup(shape, 1, -1, 1, "estimate", NULL);

    double complex in[N], out[N];
    for (size_t i = 0; i < N; i++)
        in[i] = cos(2.0 * M_PI * 10.0 * i / N) + 0.0 * I;

    dp_fft1d_execute(in, out);
    printf("DC bin: %.4f + %.4fi\n", creal(out[0]), cimag(out[0]));

    dp_cleanup();
    return 0;
}
```

### 1D FFT (in-place)

```c
dp_fft1d_execute_inplace(data);  // modifies data in-place
```

### 2D FFT

```c
size_t shape[] = {64, 64};
dp_fft_global_setup(shape, 2, -1, 1, "estimate", NULL);
dp_fft2d_execute(in2d, out2d);
dp_fft2d_execute_inplace(in2d);
```

---

## SIMD arithmetic

```c
#include <dp/util.h>
#include <complex.h>
#include <stdio.h>

int main(void) {
    double complex a = 1.0 + 2.0 * I;
    double complex b = 3.0 + 4.0 * I;
    double complex c = dp_c16_mul(a, b);
    printf("result: %.1f + %.1fi\n", creal(c), cimag(c));  // -5.0 + 10.0i
    return 0;
}
```
