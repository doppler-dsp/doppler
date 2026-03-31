/**
 * @file buffer.h
 * @brief High-performance x86-64 Circular Buffer for RF Streaming.
 *
 * @section Virtual Memory Buffers
 * doppler uses virtual memory mirroring to eliminate the "wrap-around" problem
 * in circular buffers. This allows for zero-copy, branchless access to
 * contiguous blocks of data across the buffer boundary.
 *
 * @section Virtual Memory Mirroring
 * By mapping the same physical memory to two adjacent virtual addresses (A and
 * A + N), we exploit the CPU's MMU to handle circular indexing at the hardware
 * level.
 *
 * @section Power of Two Masking
 * We use & mask instead of % capacity. On x86-64, bitwise AND is a
 * single-cycle instruction, whereas integer modulo can take 20-80 cycles.
 *
 * @section False Sharing
 * The head and tail pointers are separated by 64 bytes to prevent the
 * "Ping-Pong" effect where two CPU cores constantly invalidate each other's
 * cache lines when updating indices.
 *
 * @section Spin-Wait Optimization
 * DP_SPIN_HINT() is used in the consumer loop to reduce power consumption
 * and prevent the CPU from mispredicting the "loop end" during high-frequency
 * polling.
 */

#ifndef DP_BUFFER_H
#define DP_BUFFER_H

/* -------------------------------------------------------------------------
 * Platform includes
 * ---------------------------------------------------------------------- */
#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00 /* Windows 10 – required for VirtualAlloc2 */
#elif _WIN32_WINNT < 0x0A00
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else /* POSIX ------------------------------------------------------------   \
       */
#ifdef __linux__
#define _GNU_SOURCE /* memfd_create */
#endif
#ifdef __APPLE__
#define _DARWIN_C_SOURCE /* MAP_ANON */
#endif
#include <fcntl.h> /* shm_open on macOS */
#include <stdio.h> /* snprintf */
#include <sys/mman.h>
#include <unistd.h>
#endif /* _WIN32 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* macOS uses MAP_ANON, Linux uses MAP_ANONYMOUS. Normalize to MAP_ANONYMOUS.
 */
#if !defined(_WIN32) && !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
#define MAP_ANONYMOUS MAP_ANON
#endif
#include <stdlib.h>
#include <string.h>

/* -------------------------------------------------------------------------
 * Architecture: spin-hint
 * ---------------------------------------------------------------------- */
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__)
#include <immintrin.h>
#define DP_SPIN_HINT() _mm_pause ()
#else
#define DP_SPIN_HINT() ((void)0)
#endif

/* -------------------------------------------------------------------------
 * Compat: alignment
 * ---------------------------------------------------------------------- */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#include <stdalign.h>
#define DP_ALIGN(n) alignas (n)
#else
#define DP_ALIGN(n) __attribute__ ((aligned (n)))
#endif

/* -------------------------------------------------------------------------
 * Compat: compile-time power-of-two assert
 * ---------------------------------------------------------------------- */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define DP_ASSERT_PWR2(n)                                                     \
  _Static_assert (((n) & ((n) - 1)) == 0, "Size must be power of 2")
#else
#define DP_ASSERT_PWR2(n)                                                     \
  typedef char dp_assert_pwr2_##n[((n) & ((n) - 1)) == 0 ? 1 : -1]
#endif

/* -------------------------------------------------------------------------
 * Atomics (GCC / Clang __atomic built-ins)
 * ---------------------------------------------------------------------- */
#if defined(__GNUC__) || defined(__clang__)
#define DP_LOAD_ACQ(ptr)                                                      \
  __extension__ ({                                                            \
    size_t _v;                                                                \
    __atomic_load ((const size_t *)(ptr), &_v, __ATOMIC_ACQUIRE);             \
    _v;                                                                       \
  })
#define DP_LOAD_RLX(ptr)                                                      \
  __extension__ ({                                                            \
    size_t _v;                                                                \
    __atomic_load ((const size_t *)(ptr), &_v, __ATOMIC_RELAXED);             \
    _v;                                                                       \
  })
#define DP_STORE_REL(ptr, val)                                                \
  __extension__ ({                                                            \
    size_t _v = (val);                                                        \
    __atomic_store ((size_t *)(ptr), &_v, __ATOMIC_RELEASE);                  \
  })
#else
#error "doppler requires GCC or Clang for atomic built-ins."
#endif

/** @brief Standard x86-64 cache-line size (64 bytes). */
#define DP_CACHELINE 64

/* =========================================================================
 * Platform-agnostic memory helpers
 * All platform-specific logic lives HERE, outside any macro, so that
 * #ifdef / #else can be used freely.
 * ====================================================================== */

/** @brief Returns the system page size. */
static inline size_t
dp__page_size (void)
{
#ifdef _WIN32
  SYSTEM_INFO si;
  GetSystemInfo (&si);
  return (size_t)si.dwPageSize;
#else
  return (size_t)sysconf (_SC_PAGESIZE);
#endif
}

/**
 * @brief Allocates a double-mapped ring-buffer region of @p bytes.
 *
 * The returned address @c addr satisfies:
 *   - addr[0 .. bytes-1]        ← first  view (writable)
 *   - addr[bytes .. 2*bytes-1]  ← second view (same physical pages)
 *
 * On Windows, a HANDLE to the file-mapping object is written to
 * @p handle_out and must be passed to dp__buf_free().
 * On POSIX, @p handle_out is set to NULL.
 *
 * @return Base address of the double-mapped region, or NULL on failure.
 */
static inline void *
dp__buf_alloc (size_t bytes, void **handle_out)
{
  *handle_out = NULL;

#ifdef _WIN32
  /*
   * Windows ring-buffer using only kernel32.dll functions (Win7+).
   *
   * Strategy: CreateFileMapping + MapViewOfFileEx at adjacent addresses.
   * We use MapViewOfFile to discover a free region, unmap it, then
   * immediately remap twice. A tight retry loop handles the rare case
   * where another thread grabs the address between the unmap and remap.
   */
  DWORD size_hi = (DWORD)((DWORD64)bytes >> 32);
  DWORD size_lo = (DWORD)(bytes & 0xFFFFFFFFULL);

  HANDLE h = CreateFileMappingA (INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                                 size_hi, size_lo, NULL);
  if (!h)
    return NULL;

  void *addr = NULL;
  for (int attempt = 0; attempt < 1024; ++attempt)
    {
      /* 1. Map once to find a suitably aligned free region. */
      void *hint = MapViewOfFile (h, FILE_MAP_ALL_ACCESS, 0, 0, bytes);
      if (!hint)
        break;
      UnmapViewOfFile (hint);

      /* 2. Remap first view at the hint address. */
      void *v1 = MapViewOfFileEx (h, FILE_MAP_ALL_ACCESS, 0, 0, bytes, hint);
      if (!v1)
        continue; /* someone stole the address – retry */

      /* 3. Remap second view immediately after the first. */
      void *v2 = MapViewOfFileEx (h, FILE_MAP_ALL_ACCESS, 0, 0, bytes,
                                  (char *)v1 + bytes);
      if (v2)
        {
          addr = v1;
          break;
        } /* success */

      UnmapViewOfFile (v1); /* second map failed – retry with new hint */
    }

  if (!addr)
    {
      CloseHandle (h);
      return NULL;
    }
  *handle_out = (void *)h;
  return addr;

#else /* POSIX ----------------------------------------------------------- */

  /* Obtain an anonymous fd for `bytes` of shared memory. */
  int fd;
#ifdef __linux__
  /* Use memfd_create when available; otherwise fall back to the raw
   * syscall to avoid implicit-declaration warnings on older libc. */
#if defined(__GLIBC__)
#include <features.h>
#endif
#include <sys/syscall.h>
#ifndef SYS_memfd_create
#if defined(__x86_64__)
#define SYS_memfd_create 319
#elif defined(__aarch64__)
#define SYS_memfd_create 279
#elif defined(__i386__)
#define SYS_memfd_create 356
#endif
#endif
  fd = (int)syscall (SYS_memfd_create, "dp_shm", 0);
#else
  char name[64];
  /* PID + pointer makes name unique even across concurrent tests. */
  (void)snprintf (name, sizeof (name), "/dp_%d_%p", (int)getpid (),
                  (void *)&bytes);
  fd = shm_open (name, O_RDWR | O_CREAT | O_EXCL, 0600);
  if (fd != -1)
    shm_unlink (name); /* unlink immediately; fd keeps it alive */
#endif
  if (fd == -1)
    return NULL;
  if (ftruncate (fd, (off_t)bytes) == -1)
    {
      close (fd);
      return NULL;
    }

  /* Reserve 2*bytes of virtual address space. */
  void *addr
      = mmap (NULL, 2 * bytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (addr == MAP_FAILED)
    {
      close (fd);
      return NULL;
    }

  /* Map lower half. */
  if (mmap (addr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0)
      == MAP_FAILED)
    {
      munmap (addr, 2 * bytes);
      close (fd);
      return NULL;
    }
  /* Map upper half (same physical pages). */
  if (mmap ((char *)addr + bytes, bytes, PROT_READ | PROT_WRITE,
            MAP_SHARED | MAP_FIXED, fd, 0)
      == MAP_FAILED)
    {
      munmap (addr, 2 * bytes);
      close (fd);
      return NULL;
    }
  close (fd); /* fd no longer needed once both views are established */
  return addr;
#endif /* _WIN32 */
}

/**
 * @brief Releases a double-mapped region created by dp__buf_alloc().
 *
 * @param addr   Base address returned by dp__buf_alloc().
 * @param bytes  Size of ONE mapping (same value passed to dp__buf_alloc).
 * @param handle Platform handle returned via handle_out (Win32: HANDLE, else
 * NULL).
 */
static inline void
dp__buf_free (void *addr, size_t bytes, void *handle)
{
#ifdef _WIN32
  UnmapViewOfFile (addr);
  UnmapViewOfFile ((char *)addr + bytes);
  if (handle)
    CloseHandle ((HANDLE)handle);
#else
  munmap (addr, 2 * bytes);
  (void)handle;
#endif
}

/* =========================================================================
 * DECLARE_DP_BUFFER(name, type)
 *
 * Generates a complete type-specific circular-buffer implementation.
 * The macro itself contains NO #ifdef, keeping it portable by delegating
 * all platform work to dp__buf_alloc / dp__buf_free above.
 * ====================================================================== */

/**
 * @def DECLARE_DP_BUFFER(name, type)
 * @brief Generates a type-specific circular buffer implementation.
 *
 * @param name  Suffix for generated names (e.g., f32, i16).
 * @param type  Underlying primitive type for complex I/Q samples.
 */
#define DECLARE_DP_BUFFER(name, type)                                         \
                                                                              \
  /** @struct dp_##name */                                                    \
  typedef struct                                                              \
  {                                                                           \
    type *data;      /**< Double-mapped memory address. */                    \
    size_t mask;     /**< Bitmask for power-of-two indexing. */               \
    size_t capacity; /**< Total capacity in complex samples. */               \
    void *_handle;   /**< Platform handle (Win32: HANDLE, POSIX: NULL). */    \
    DP_ALIGN (DP_CACHELINE) volatile size_t head;    /**< Producer idx. */    \
    DP_ALIGN (DP_CACHELINE) volatile size_t tail;    /**< Consumer idx. */    \
    DP_ALIGN (DP_CACHELINE) volatile size_t dropped; /**< Overrun ctr. */     \
  } dp_##name;                                                                \
                                                                              \
  /**                                                                         \
   * @brief Creates a double-mapped circular buffer.                          \
   *                                                                          \
   * Uses virtual memory mirroring so reads/writes that cross the buffer      \
   * boundary wrap transparently — zero-copy, branchless.                   \
   *                                                                          \
   * @param n_samples Buffer size in complex samples. MUST be a power of 2,   \
   *                  and n_samples * sizeof(type) * 2 must be page-aligned.  \
   * @return Pointer to an initialised dp_##name, or NULL on failure.         \
   */                                                                         \
  static inline dp_##name *dp_##name##_create (size_t n_samples)              \
  {                                                                           \
    size_t bytes = n_samples * sizeof (type) * 2;                             \
    if ((n_samples & (n_samples - 1)) != 0)                                   \
      return NULL;                                                            \
    if (bytes % dp__page_size () != 0)                                        \
      return NULL;                                                            \
    void *handle = NULL;                                                      \
    void *addr = dp__buf_alloc (bytes, &handle);                              \
    if (!addr)                                                                \
      return NULL;                                                            \
    dp_##name *ab = (dp_##name *)calloc (1, sizeof (dp_##name));              \
    if (!ab)                                                                  \
      {                                                                       \
        dp__buf_free (addr, bytes, handle);                                   \
        return NULL;                                                          \
      }                                                                       \
    ab->data = (type *)addr;                                                  \
    ab->capacity = n_samples;                                                 \
    ab->mask = n_samples - 1;                                                 \
    ab->_handle = handle;                                                     \
    return ab;                                                                \
  }                                                                           \
                                                                              \
  /** @brief Destroys the buffer and releases virtual memory. */              \
  static inline void dp_##name##_destroy (dp_##name *ab)                      \
  {                                                                           \
    dp__buf_free (ab->data, ab->capacity * sizeof (type) * 2, ab->_handle);   \
    free (ab);                                                                \
  }                                                                           \
                                                                              \
  /**                                                                         \
   * @brief Non-blocking write to the buffer.                                 \
   *                                                                          \
   * @param ab  Pointer to buffer.                                            \
   * @param src Source data array (n complex samples).                        \
   * @param n   Number of complex samples to write.                           \
   * @return true if successful, false on buffer overrun (data dropped).      \
   */                                                                         \
  static inline bool dp_##name##_write (dp_##name *ab, const type *src,       \
                                        size_t n)                             \
  {                                                                           \
    size_t h = DP_LOAD_RLX (&ab->head);                                       \
    size_t t = DP_LOAD_ACQ (&ab->tail);                                       \
    if ((ab->capacity - (h - t)) < n)                                         \
      {                                                                       \
        __atomic_fetch_add (&ab->dropped, n, __ATOMIC_RELAXED);               \
        return false;                                                         \
      }                                                                       \
    memcpy (&ab->data[(h & ab->mask) * 2], src, n * sizeof (type) * 2);       \
    DP_STORE_REL (&ab->head, h + n);                                          \
    return true;                                                              \
  }                                                                           \
                                                                              \
  /**                                                                         \
   * @brief Blocking wait for a contiguous batch of samples.                  \
   *                                                                          \
   * Because of double-mapping, the returned pointer is guaranteed contiguous \
   * for @p n samples, allowing direct SIMD/AVX processing without copying.   \
   *                                                                          \
   * @param ab Pointer to buffer.                                             \
   * @param n  Minimum samples required.                                      \
   * @return   Pointer to the read-head in the buffer.                        \
   */                                                                         \
  static inline type *dp_##name##_wait (dp_##name *ab, size_t n)              \
  {                                                                           \
    size_t h, t;                                                              \
    while (((h = DP_LOAD_ACQ (&ab->head)) - (t = DP_LOAD_RLX (&ab->tail)))    \
           < n)                                                               \
      {                                                                       \
        DP_SPIN_HINT ();                                                      \
      }                                                                       \
    (void)h;                                                                  \
    return &ab->data[(t & ab->mask) * 2];                                     \
  }                                                                           \
                                                                              \
  /** @brief Releases @p n samples after processing is complete. */           \
  static inline void dp_##name##_consume (dp_##name *ab, size_t n)            \
  {                                                                           \
    size_t t = DP_LOAD_RLX (&ab->tail);                                       \
    DP_STORE_REL (&ab->tail, t + n);                                          \
  }

/* --- Type instantiations --- */

DECLARE_DP_BUFFER (f32, float)  /**< 32-bit float  complex (8 bytes/sample)  */
DECLARE_DP_BUFFER (f64, double) /**< 64-bit double complex (16 bytes/sample) */
DECLARE_DP_BUFFER (i16, int16_t) /**< 16-bit int    complex (4 bytes/sample) */

#endif /* DP_BUFFER_H */
