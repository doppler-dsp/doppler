# C Examples

## LO — complex phasor generator

The `LO` type chains a 32-bit NCO with a 2¹⁶-entry sin/cos LUT to produce
CF32 IQ phasors at ~96 dBc SFDR.

### Free-running IQ

```c
#include <lo/lo_core.h>
#include <complex.h>
#include <stdio.h>

int main(void) {
    lo_state_t *lo = lo_create(0.25);  // quarter-rate tone

    float complex out[8];
    lo_steps(lo, 8, out);

    for (int i = 0; i < 8; i++)
        printf("out[%d]: %.3f + %.3fi\n", i, crealf(out[i]), cimagf(out[i]));
    // out[0]:  1.000 + 0.000i
    // out[1]:  0.000 + 1.000i
    // out[2]: -1.000 + 0.000i
    // out[3]:  0.000 - 1.000i
    // out[4]:  1.000 + 0.000i  (repeats every 4 samples)
    // ...

    lo_destroy(lo);
    return 0;
}
```

### FM modulation via control port

```c
#include <lo/lo_core.h>
#include <complex.h>
#include <math.h>

lo_state_t *lo = lo_create(0.1);   // base freq f_n = 0.1

float ctrl[1024];
for (int i = 0; i < 1024; i++)
    ctrl[i] = 0.002f * sinf(2.0f * (float)M_PI * 0.01f * i);

float complex out[1024];
lo_steps_ctrl(lo, ctrl, 1024, out);
// base freq unchanged; reset restores clean phase
lo_destroy(lo);
```

---

## AWGN — Additive White Gaussian Noise

### One-shot (no persistent state)

```c
#include <awgn/awgn_core.h>
#include <complex.h>

float complex out[1024];
awgn(0, 1.0f, 1024, out);   /* seed=0, amplitude=1.0 — 0 on success, -1 on failure */
```

### Stateful generator (streaming / reproducible replay)

```c
#include <awgn/awgn_core.h>
#include <complex.h>
#include <stdio.h>

int main(void) {
    awgn_state_t *g = awgn_create(42, 1.0f);   /* seed, amplitude */

    float complex buf[4096];
    awgn_generate(g, 4096, buf);                /* fill buf */

    /* Retune amplitude without disturbing RNG state */
    awgn_set_amplitude(g, 0.5f);
    awgn_generate(g, 4096, buf);

    /* Deterministic replay */
    awgn_reset(g);
    awgn_generate(g, 4096, buf);               /* identical to first call */

    awgn_destroy(g);
    return 0;
}
```

### Noisy carrier

```c
#include <awgn/awgn_core.h>
#include <lo/lo_core.h>
#include <complex.h>

#define N 4096

int main(void) {
    lo_state_t   *lo   = lo_create(0.1f);
    awgn_state_t *noise = awgn_create(0, 0.3f);   /* σ=0.3 per component */

    float complex carrier[N], n[N], rx[N];
    lo_steps(lo, N, carrier);
    awgn_generate(noise, N, n);
    for (size_t i = 0; i < N; i++)
        rx[i] = carrier[i] + n[i];

    lo_destroy(lo);
    awgn_destroy(noise);
    return 0;
}
```

---

## NCO — raw phase accumulator

`NCO` exposes the bare uint32 phase accumulator — useful for driving
a polyphase resampler clock or generating carry events.

### Raw uint32 phase + overflow carry

```c
#include <nco/nco_core.h>

nco_state_t *nco = nco_create(0.25, 0);  // nmax=0 → raw [0, 2^32)

uint32_t phase[16];
uint8_t  carry[16];
nco_steps_u32_ovf(nco, 16, phase, carry);
// carry fires at indices 3, 7, 11, 15 (once per full cycle)

nco_destroy(nco);
```

---

## FIR filter

```c
#include <fir/fir_core.h>
#include <complex.h>
#include <math.h>

#define N_TAPS 19

int main(void) {
    // Windowed-sinc low-pass filter (fc = 0.2 * fs) — real taps
    float taps[N_TAPS];
    int half = N_TAPS / 2;
    for (int k = 0; k < N_TAPS; k++) {
        int    n    = k - half;
        double sinc = (n == 0) ? 1.0
                                : sin(M_PI * 0.2 * n) / (M_PI * 0.2 * n);
        double win  = 0.5 * (1.0 - cos(2.0 * M_PI * k / (N_TAPS - 1)));
        taps[k] = (float)(sinc * win);
    }

    fir_state_t *fir = fir_create_real(taps, N_TAPS);

    float complex in[1024], out[1024];
    fir_execute(fir, in, 1024, out, 1024);

    fir_destroy(fir);
    return 0;
}
```

---

## FFT

Each FFT instance holds its own plan — create once, reuse across calls.
CF32 is ~2× faster than CF64 for the same transform length.

### 1D FFT (double precision)

```c
#include "fft/fft_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>

int main(void) {
    const size_t N = 1024;
    fft_state_t *fft = fft_create(N, -1, 1);

    double complex in[N], out[N];
    for (size_t i = 0; i < N; i++)
        in[i] = cos(2.0 * M_PI * 10.0 * i / N) + 0.0 * I;

    fft_execute_cf64(fft, in, N, out);
    printf("DC bin: %.4f + %.4fi\n", creal(out[0]), cimag(out[0]));

    fft_destroy(fft);
    return 0;
}
```

### 1D FFT (single precision / CF32, ~2× faster)

```c
#include "fft/fft_core.h"
#include <complex.h>
#include <math.h>

const size_t N = 1024;
fft_state_t *fft = fft_create(N, -1, 1);

float complex in32[N], out32[N];
for (size_t i = 0; i < N; i++)
    in32[i] = cosf(2.0f * M_PI * 10.0f * i / N) + 0.0f * I;

fft_execute_cf32(fft, in32, N, out32);    // out-of-place
fft_execute_inplace_cf32(fft, in32, N);   // in-place

fft_destroy(fft);
```

### 2D FFT

```c
#include "fft2d/fft2d_core.h"
#include <complex.h>

fft2d_state_t *fft2d = fft2d_create(64, 64, -1, 1);

double complex in2d[64 * 64], out2d[64 * 64];
fft2d_execute_cf64(fft2d, in2d, 64 * 64, out2d);

float complex in32_2d[64 * 64], out32_2d[64 * 64];
fft2d_execute_cf32(fft2d, in32_2d, 64 * 64, out32_2d);

fft2d_destroy(fft2d);
```

---

## Halfband decimator

2:1 decimation with a symmetric FIR. Input length must be even;
output length is exactly `n_in / 2`.

```c
#include <HalfbandDecimator/HalfbandDecimator_core.h>
#include <complex.h>
#include <stdio.h>

#define N_TAPS 4
#define N_IN   32

/* Minimal 4-tap symmetric halfband coefficients. */
static const float H_FIR[N_TAPS] = { -0.2122f, 0.6366f, 0.6366f, -0.2122f };

int main(void) {
    HalfbandDecimator_state_t *dec = HalfbandDecimator_create(N_TAPS, H_FIR);

    float _Complex in[N_IN], out[N_IN / 2];
    /* ... fill in[] with your signal ... */

    size_t n_out = HalfbandDecimator_execute(dec, in, N_IN, out);
    printf("output samples: %zu\n", n_out);   /* 16 */

    HalfbandDecimator_destroy(dec);
    return 0;
}
```

Build and run the full demo:

```sh
make build
./build/examples/c/hbdecim_demo
```

---

## AGC — automatic gain control

The AGC drives output power to `ref_db` using a first-order loop filter.
`agc_step()` processes one sample at a time; the loop is linear in the
dB domain so settling time is independent of the step size.

```c
#include <agc/agc_core.h>
#include <complex.h>
#include <math.h>
#include <stdio.h>

#define N      6000
#define N_STEP 3000
#define F_TONE 0.02

int main(void) {
    agc_state_t *agc = agc_create(
        0.0,      /* ref_db  — target output power */
        0.00125,  /* loop_bw — noise bandwidth, cycles/sample */
        0.02      /* alpha   — power-detector EMA coefficient */
    );

    for (int n = 0; n < N; n++) {
        double amp = (n < N_STEP) ? pow(10, -10.0/20) : pow(10, 10.0/20);
        float _Complex x = (float)(amp * cos(2*M_PI*F_TONE*n))
                         + (float)(amp * sin(2*M_PI*F_TONE*n)) * I;
        float _Complex y = agc_step(agc, x);
        (void)y;
    }

    printf("gain_db = %.2f\n", agc->gain_db);
    agc_destroy(agc);
    return 0;
}
```

Build and run the full demo (prints a convergence table and writes
`agc_step_response.csv`):

```sh
make build
./build/examples/c/agc_demo
```

---

## PUSH/PULL pipeline

Two threads in-process — producer pushes 100 batches of 1024 CF64
samples over a ZMQ PUSH/PULL socket; consumer receives and prints power.

```c
#include <doppler.h>
#include <stream/stream.h>
#include <complex.h>

/* Producer thread */
dp_push_t *ctx = dp_push_create("ipc:///tmp/dp.ipc", CF64);
dp_header_t hdr = { .sample_type = CF64, .num_samples = 1024,
                    .sample_rate = 1e6, .center_freq = 0 };
dp_push_send(ctx, &hdr, samples, 1024 * sizeof(double _Complex));
dp_push_destroy(ctx);

/* Consumer thread */
dp_pull_t *rx = dp_pull_create("ipc:///tmp/dp.ipc");
dp_header_t rhdr;
void *buf = malloc(DP_MAX_PAYLOAD);
dp_pull_recv(rx, &rhdr, buf, DP_MAX_PAYLOAD);
dp_pull_destroy(rx);
free(buf);
```

Build and run the in-process demo (producer + consumer threads, 100
batches):

```sh
make build
./build/examples/c/pipeline_demo
```

