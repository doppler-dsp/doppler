/**
 * @file doppler.h
 * @brief Doppler - Unified DSP and Streaming Library
 *
 * Convenience header that includes all Doppler modules:
 * - Core: Initialization and version (dp_init, dp_cleanup, dp_version)
 * - Buffer: Lock-free circular buffers (dp_f32_buffer_*, etc.)
 * - FFT: Fast Fourier Transform (dp_fft_*)
 * - FIR: Finite Impulse Response filters (dp_fir_*)
 * - SIMD: SIMD-accelerated operations (dp_c16_mul)
 * - Stream: PUB/SUB, PUSH/PULL, REQ/REP streaming (dp_pub_*, dp_sub_*, etc.)
 *
 * For selective inclusion, use individual headers:
 * ```c
 * #include <dp/core.h>
 * #include <dp/buffer.h>
 * #include <dp/fft.h>
 * #include <dp/fir.h>
 * #include <dp/simd.h>
 * #include <dp/stream.h>
 * ```
 *
 * Or simply include everything:
 * ```c
 * #include <doppler.h>
 * ```
 */

#ifndef DP_H
#define DP_H

/* Define _GNU_SOURCE before any standard headers (required for memfd_create on
 * Linux) */
#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include "dp/buffer.h"
#include "dp/core.h"
#include "dp/fft.h"
#include "dp/fir.h"
#include "dp/simd.h"
#include "dp/stream.h"

#endif /* DP_H */
