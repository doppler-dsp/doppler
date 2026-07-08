

# File timing\_core.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**timing**](dir_0a8cc616bc028a416e339204953e39da.md) **>** [**timing\_core.h**](timing__core_8h.md)

[Go to the documentation of this file](timing__core_8h.md)


```C++
/*
 * timing_core.h — sample-clock pacing + timestamping.
 *
 * A ``dp_sample_clock_t`` models an ideal sample clock running at ``fs`` Hz.
 * It does two things off one shared, drift-free timeline anchored when the
 * clock is created or reset:
 *
 *   - PACE a producer so each block leaves at its real-time deadline
 *     ``epoch + n/fs`` — mimicking a hardware sample clock feeding a DAC.
 *   - STAMP a block with the ideal wall-clock time of its samples
 *     (``epoch_real + n/fs``), for reproducible capture metadata.
 *
 * Because both derive from the cumulative sample count ``n`` against a fixed
 * epoch — not from summed per-block sleeps — the schedule is drift-free: an
 * over- or under-sleep on one block is corrected on the next. The residual
 * error is per-block jitter bounded by the OS scheduler, which averages out.
 *
 * Pacing is POSIX only (``clock_gettime`` + an absolute-deadline sleep); the
 * build guards this translation unit out on Windows, mirroring the stream sink.
 */
#ifndef DP_TIMING_CORE_H
#define DP_TIMING_CORE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct
  {
    double   fs;            
    uint64_t epoch_mono_ns; 
    uint64_t epoch_real_ns; 
    uint64_t n;             
    uint64_t underruns;     
    uint64_t max_late_ns;   
    int      resync;        
  } dp_sample_clock_t;

  uint64_t dp_mono_ns (void);

  uint64_t dp_real_ns (void);

  void dp_sample_clock_init (dp_sample_clock_t *c, double fs, int resync);

  double dp_sample_clock_pace (dp_sample_clock_t *c, size_t count);

  uint64_t dp_sample_clock_stamp (const dp_sample_clock_t *c);

  void dp_sample_clock_reset (dp_sample_clock_t *c);

  void dp_sample_clock_resync (dp_sample_clock_t *c);

  /* A stats snapshot for the generated `SampleClock` handle (jm kind="handle"):
   * the decoded-getter face wants one call that fills an out-struct, so this
   * copies the (public) clock struct out. The handle constructs init-in-place
   * via dp_sample_clock_init above (jm#320 `init_fn`); jm owns that malloc/free. */
  void dp_sample_clock_stats (const dp_sample_clock_t *c,
                              dp_sample_clock_t       *out);

  dp_sample_clock_t *dp_sample_clock_create (double fs, int resync);

  void dp_sample_clock_destroy (dp_sample_clock_t *c);

#ifdef __cplusplus
}
#endif

#endif /* DP_TIMING_CORE_H */
```


