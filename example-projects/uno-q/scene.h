/* scene.h — the synthetic RTL-SDR capture both uno-q programs share.
 *
 * One definition of the test scene: the receiver's self-test builds it
 * locally, and `uno_q_pub --synthetic` publishes the same bytes over NATS, so
 * `uno_q --check` can hold data that crossed the transport to exactly the
 * predictions the self-test uses.
 */
#ifndef UNO_Q_SCENE_H
#define UNO_Q_SCENE_H

#include <stddef.h>
#include <stdint.h>

/* A tone in noise, well inside the 8-bit range (peaks near 0.45 of full
   scale, so nothing clips). */
#define SCENE_TONE_AMP 0.25    /* complex tone amplitude; power -12.0 dBFS */
#define SCENE_NOISE_SIGMA 0.05 /* per-component std dev; complex -23 dBFS */
#define SCENE_SEED 0x5eedu

/**
 * @brief The dongle's 8-bit ADC, as a model: analog `v` to its cu8 code.
 *
 * The RTL2832U's zero sits BETWEEN codes 127 and 128, so a code covers
 * `[c - 128, c - 127) / 128` and the analog zero reads 127.5 on average —
 * which is what makes U8ToF32's `shift` read 0.5/128 low. A model of the
 * hardware, not a DSP stage.
 */
uint8_t scene_rtl_code (float v);

/**
 * @brief `n` complex samples of the scene as interleaved cu8 (2n bytes):
 * a tone at `offset` Hz plus AWGN, through scene_rtl_code(). Built with the
 * library's own LO and AWGN sources. Returns a malloc'd buffer, or NULL.
 */
uint8_t *scene_cu8 (double fs, double offset, size_t n);

#endif /* UNO_Q_SCENE_H */
