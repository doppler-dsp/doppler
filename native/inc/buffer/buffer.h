/**
 * @file buffer.h
 * @brief High-performance x86-64 Circular Buffer for RF Streaming.
 *
 * @section buf_vmem Virtual Memory Buffers
 * doppler uses virtual memory mirroring to eliminate the "wrap-around" problem
 * in circular buffers. This allows for zero-copy, branchless access to
 * contiguous blocks of data across the buffer boundary.
 *
 * @section buf_mirror Virtual Memory Mirroring
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
/* NO feature-test macro here. _GNU_SOURCE / _DARWIN_C_SOURCE are defined on
   the compile line (see the top-level CMakeLists.txt), because glibc's
   features.h latches on its first inclusion: a #define in this header is a
   no-op for every translation unit that reached libc before including it,
   which is most of them. This file carried one and it was inert exactly that
   way -- doppler#986. A downstream compiling the installed headers needs the
   same definition on ITS compile line. */
#include <fcntl.h> /* shm_open on macOS; open() for a file-backed ring */
#include <stdio.h>    /* snprintf */
#include <sys/mman.h> /* mmap, msync */
#include <sys/stat.h> /* fstat -- is this file already a ring? */
#include <unistd.h>
#endif /* _WIN32 */

/* The ring's wait consults the same interrupt flag as every other
   blocking wait in doppler; see docs/design/io-termination.md. */
#include "dp_interrupt.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "jm_perf.h" /* JM_FORCEINLINE */
#include "util/util_core.h" /* next_pow_two */

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

/**
 * @brief Returns the granularity the double-mapped views must align to.
 *
 * This is the unit the ring-buffer mirror is rounded up to. On POSIX that is
 * the page size. On Windows it is the *allocation granularity* (64 KiB),
 * which is ≥ dwPageSize: MapViewOfFileEx requires each view's base address to
 * be a multiple of the allocation granularity, so the second (mirror) view at
 * base + bytes is only placeable when @c bytes is a whole multiple of it.
 * Using dwPageSize (4 KiB) here would let a sub-64-KiB buffer pass the size
 * check and then fail to map.
 */
static inline size_t
dp__page_size (void)
{
#ifdef _WIN32
  SYSTEM_INFO si;
  GetSystemInfo (&si);
  return (size_t)si.dwAllocationGranularity;
#else
  return (size_t)sysconf (_SC_PAGESIZE);
#endif
}

#ifdef _WIN32
/**
 * @brief Maps @p mapping twice, back to back, and returns the first view.
 *
 * The one Windows mirror primitive: dp__buf_alloc() hands it an anonymous
 * mapping and dp__buf_alloc_file() a file's, so a fix to how the pair is
 * placed lands in both at once.
 *
 * @return Base address of the double-mapped region, or NULL on failure.
 */
static inline void *
dp__win_map_twice (HANDLE mapping, size_t bytes)
{
  /*
   * Windows ring-buffer using only kernel32.dll functions (Win7+).
   *
   * Strategy: CreateFileMapping + MapViewOfFileEx at adjacent addresses.
   * Reserve 2*bytes of address space to find a hole BOTH views fit in,
   * release it, then map the two views into it. A retry loop handles the
   * one way that can still fail: another thread taking the address between
   * the release and the maps.
   *
   * The hole has to be probed at 2*bytes. This used to probe with a
   * `bytes`-long MapViewOfFile, which proves only that the FIRST view fits;
   * whether the mirror did depended on whatever sat after it, which ASLR
   * moves on every run. When it did not, every retry found the same hint,
   * all 1024 failed, and create() returned NULL -- seen in CI as acq and
   * dp_tlm constructors returning NULL, a dp_xnn abort (0xc0000409), heap
   * corruption and segfaults, on commits that passed the run before.
   */
  void *addr = NULL;
  for (int attempt = 0; attempt < 1024; ++attempt)
    {
      /* 1. Find a free region big enough for BOTH views. VirtualAlloc
         returns it aligned to the allocation granularity, which is what
         MapViewOfFileEx needs of each view's address. */
      void *hint = VirtualAlloc (NULL, 2 * bytes, MEM_RESERVE, PAGE_NOACCESS);
      if (!hint)
        break;
      VirtualFree (hint, 0, MEM_RELEASE);

      /* 2. Remap first view at the hint address. */
      void *v1
          = MapViewOfFileEx (mapping, FILE_MAP_ALL_ACCESS, 0, 0, bytes, hint);
      if (!v1)
        continue; /* someone stole the address – retry */

      /* 3. Remap second view immediately after the first. */
      void *v2 = MapViewOfFileEx (mapping, FILE_MAP_ALL_ACCESS, 0, 0, bytes,
                                  (char *)v1 + bytes);
      if (v2)
        {
          addr = v1;
          break;
        } /* success */

      UnmapViewOfFile (v1); /* second map failed – retry with new hint */
    }

  return addr;
}
#endif /* _WIN32 */

/**
 * @brief Allocates a double-mapped ring-buffer region of @p bytes.
 *
 * The returned address @c addr satisfies:
 *   - addr(0..bytes-1)           ← first  view (writable)
 *   - addr(bytes..2*bytes-1)    ← second view (same physical pages)
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
  DWORD size_hi = (DWORD)((DWORD64)bytes >> 32);
  DWORD size_lo = (DWORD)(bytes & 0xFFFFFFFFULL);

  HANDLE h = CreateFileMappingA (INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                                 size_hi, size_lo, NULL);
  if (!h)
    return NULL;

  void *addr = dp__win_map_twice (h, bytes);
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
 * @brief As dp__buf_alloc(), but the pages are backed by a FILE.
 *
 * The mirror trick is indifferent to where the fd came from, so a persistent
 * ring is the same double mapping over an `open()`ed path instead of an
 * anonymous one. Because the mapping is `MAP_SHARED`, the ring's samples ARE
 * the file's contents: there is no separate write path to disk, no copy, and
 * no way for the two to disagree. The kernel writes the pages back on its own
 * schedule; dp__buf_sync() forces the point.
 *
 * The file is created if absent and truncated to @p bytes. An EXISTING file of
 * the right size is mapped as it stands, which is what lets a ring survive the
 * process that filled it: the caller restores the head/tail positions and the
 * samples are simply there.
 *
 * @param bytes      Size of ONE mapping (the mirror unit is 2x this).
 * @param handle_out Set to NULL on POSIX (as dp__buf_alloc).
 * @param path       File to back the ring with.
 * @param existed    If non-NULL, set to 1 when the file was already the right
 *                   size (so its contents are the ring's), 0 when it was
 *                   created or resized.
 * @return Base address of the double-mapped region, or NULL on failure.
 *
 * @note On Windows the file is a CreateFileMapping over CreateFileA, mirrored
 *       by the same dp__win_map_twice() as the anonymous ring, and
 *       dp__buf_sync() flushes the view but does not wait for the disk.
 */
static inline void *
dp__buf_alloc_file (size_t bytes, void **handle_out, const char *path,
                    int *existed)
{
  *handle_out = NULL;
  if (existed)
    *existed = 0;
#ifdef _WIN32
  if (!path || !*path)
    return NULL;

  HANDLE f = CreateFileA (path, GENERIC_READ | GENERIC_WRITE,
                          FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                          OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (f == INVALID_HANDLE_VALUE)
    return NULL;

  /* Same rule as POSIX: exactly this long is a ring somebody filled, and is
     mapped as it stands; anything else is cut to zero and regrown, which
     sizes it AND zeroes it -- NTFS reads an extension back as zeros. */
  LARGE_INTEGER have, zero, want;
  zero.QuadPart = 0;
  want.QuadPart = (LONGLONG)bytes;
  if (GetFileSizeEx (f, &have) && (size_t)have.QuadPart == bytes)
    {
      if (existed)
        *existed = 1;
    }
  else if (!SetFilePointerEx (f, zero, NULL, FILE_BEGIN) || !SetEndOfFile (f)
           || !SetFilePointerEx (f, want, NULL, FILE_BEGIN)
           || !SetEndOfFile (f))
    {
      CloseHandle (f);
      return NULL;
    }

  HANDLE h = CreateFileMappingA (f, NULL, PAGE_READWRITE,
                                 (DWORD)((DWORD64)bytes >> 32),
                                 (DWORD)(bytes & 0xFFFFFFFFULL), NULL);
  CloseHandle (f); /* the mapping holds the file open from here */
  if (!h)
    return NULL;

  void *addr = dp__win_map_twice (h, bytes);
  if (!addr)
    {
      CloseHandle (h);
      return NULL;
    }
  *handle_out = (void *)h;
  return addr;
#else
  if (!path || !*path)
    return NULL;

  int fd = open (path, O_RDWR | O_CREAT, 0600);
  if (fd == -1)
    return NULL;

  /* A file already exactly this long is a ring somebody filled: map it as it
     stands. Anything else -- absent, short, or a different geometry -- is
     cut to ZERO and regrown, which sizes it AND zeroes it.

     Two truncates, not one. ftruncate() to a LARGER size zeroes only the
     extension and keeps what was there; to a smaller one it keeps the
     leading bytes. Either way a ring of a new geometry came back holding
     the old one's samples while `existed` said 0 -- "created, and zeroed".
     The Windows branch above has always done it this way. */
  struct stat st;
  if (fstat (fd, &st) == 0 && (size_t)st.st_size == bytes)
    {
      if (existed)
        *existed = 1;
    }
  else if (ftruncate (fd, 0) == -1 || ftruncate (fd, (off_t)bytes) == -1)
    {
      close (fd);
      return NULL;
    }

  /* Reserve 2*bytes of address space, then place both views in it -- the
     same sequence dp__buf_alloc() uses, over a different fd. */
  void *addr
      = mmap (NULL, 2 * bytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (addr == MAP_FAILED)
    {
      close (fd);
      return NULL;
    }
  if (mmap (addr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0)
          == MAP_FAILED
      || mmap ((char *)addr + bytes, bytes, PROT_READ | PROT_WRITE,
               MAP_SHARED | MAP_FIXED, fd, 0)
             == MAP_FAILED)
    {
      munmap (addr, 2 * bytes);
      close (fd);
      return NULL;
    }
  close (fd); /* both views hold the file open */
  return addr;
#endif
}

/**
 * @brief Flush a file-backed region to disk.
 *
 * A no-op for an anonymous ring, and harmless there. Call it where a
 * checkpoint is TAKEN: the samples are in the page cache until the kernel
 * decides otherwise, so a blob written without this names a history that a
 * crash can still lose.
 *
 * @param addr  Base address (the first view).
 * @param bytes Size of ONE mapping.
 */
static inline void
dp__buf_sync (void *addr, size_t bytes)
{
#ifdef _WIN32
  /* Writes the dirty pages to the file, which is what survives the PROCESS.
     msync(MS_SYNC) also waits for the disk; the Windows equivalent needs
     FlushFileBuffers on a file handle this mapping no longer keeps. */
  (void)FlushViewOfFile (addr, bytes);
#else
  (void)msync (addr, bytes, MS_SYNC);
#endif
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

/**
 * @brief Why a ring's wait can or cannot be satisfied right now.
 *
 * dp_*_wait() and dp_*_peek() return NULL for more than one reason,
 * and the reasons call for different responses: end of stream is normal and a
 * consumer loop catches it, an interrupt means stop, too-large is a caller
 * bug, and "not yet" is no failure at all. dp_*_wait_status() owns the
 * PRECEDENCE between them, so a binding or a consumer asks one question
 * instead of re-deriving the order from three -- which is what the Python
 * binding did, in three hand-written copies.
 */
typedef enum
{
  DP_WAIT_OK          = 0, /**< @p n samples are readable now.              */
  DP_WAIT_PENDING     = 1, /**< Fewer than @p n so far; nothing is wrong.    */
  DP_WAIT_TOO_LARGE   = 2, /**< @p n exceeds capacity: never satisfiable.    */
  DP_WAIT_CLOSED      = 3, /**< Closed with fewer than @p n left: the end.   */
  DP_WAIT_INTERRUPTED = 4  /**< The process was asked to stop.               */
} dp_wait_status_t;

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
  /** @struct dp_##name##_t */                                                \
  typedef struct                                                              \
  {                                                                           \
    type *data;      /**< Double-mapped memory address. */                    \
    size_t mask;     /**< MAPPED samples - 1: a power of two, for indexing. */\
    size_t capacity; /**< Samples the ring holds: what was asked for. */      \
    void *_handle;   /**< Platform handle (Win32: HANDLE, POSIX: NULL). */    \
    DP_ALIGN (DP_CACHELINE) volatile size_t head;    /**< Producer idx. */    \
    /* Shares the producer's line on purpose: the producer writes both, \
       and a consumer reading them together reads one line. */          \
    volatile int closed; /**< Producer said no more data is coming. */  \
    DP_ALIGN (DP_CACHELINE) volatile size_t tail;    /**< Consumer idx. */    \
    /** Samples in REFUSED writes -- not samples lost. dp_##name##_write()  \
        adds @p n on each rejection and copies nothing, so a caller that     \
        retries keeps its data and still moves this counter. */              \
    DP_ALIGN (DP_CACHELINE) volatile size_t dropped;                          \
  } dp_##name##_t;                                                            \
                                                                              \
  /**                                                                         \
   * @brief Wrap an already-mapped mirrored region in a buffer struct.        \
   *                                                                          \
   * The tail both constructors share. It exists so the struct allocation is  \
   * written ONCE: two copies of it is two places for the field                \
   * initialisation to drift, and the second one raised this file's           \
   * bare-allocation count against a ratchet that may only shrink.            \
   */                                                                         \
  static inline dp_##name##_t *dp_##name##_wrap_ (                            \
      void *addr, size_t capacity, size_t mapped, void *handle)               \
  {                                                                           \
    dp_##name##_t *ab = (dp_##name##_t *)calloc (1, sizeof (dp_##name##_t));  \
    if (!ab)                                                                  \
      {                                                                       \
        dp__buf_free (addr, mapped * sizeof (type) * 2, handle);              \
        return NULL;                                                          \
      }                                                                       \
    ab->data = (type *)addr;                                                  \
    ab->capacity = capacity;                                                  \
    ab->mask = mapped - 1;                                                    \
    ab->_handle = handle;                                                     \
    return ab;                                                                \
  }                                                                           \
                                                                              \
  /**                                                                         \
   * @brief How many samples to MAP for a ring that holds @p n_samples.       \
   *                                                                          \
   * Indexing is a mask, so the mapping is a power of two; and the mirror is  \
   * built from whole pages, so it spans at least one. The smallest size      \
   * that is both. The ring's CAPACITY stays exactly what was asked for --    \
   * this is only how much address space stands behind it.                    \
   *                                                                          \
   * @return Samples to map, or 0 if @p n_samples is 0 or too large to round. \
   */                                                                         \
  static inline size_t dp_##name##_mapped_for_ (size_t n_samples)             \
  {                                                                           \
    size_t mapped = n_samples ? next_pow_two (n_samples) : 0;                 \
    size_t page   = dp__page_size ();                                         \
    size_t elem   = sizeof (type) * 2;                                        \
    /* elem and page are powers of two and so is `mapped`, so doubling until  \
       the unit reaches a page lands on an exact page multiple. */            \
    while (mapped && mapped * elem < page)                                    \
      mapped <<= 1;                                                           \
    return mapped;                                                            \
  }                                                                           \
                                                                              \
  /**                                                                         \
   * @brief Creates a double-mapped circular buffer.                          \
   *                                                                          \
   * Uses virtual memory mirroring so reads/writes that cross the buffer      \
   * boundary wrap transparently — zero-copy, branchless.                     \
   *                                                                          \
   * @param n_samples Capacity in complex samples: ANY size from 1 up, and the\
   *                  ring holds exactly that many -- ->capacity is the number\
   *                  passed here, on every machine. What is rounded is the   \
   *                  MAPPING behind it: up to a power of two, because        \
   *                  indexing is a mask, and up to a whole page, because the \
   *                  mirror is built from pages (->mask + 1 samples). A      \
   *                  capacity that is not a power of two costs address       \
   *                  space, under 2x, and nothing per call.                  \
   * @return Pointer to an initialised dp_##name##_t, or NULL on failure.     \
   */                                                                         \
  static inline dp_##name##_t *dp_##name##_create (size_t n_samples)          \
  {                                                                           \
    size_t mapped = dp_##name##_mapped_for_ (n_samples);                      \
    if (!mapped)                                                              \
      return NULL;                                                            \
    void *handle = NULL;                                                      \
    void *addr = dp__buf_alloc (mapped * sizeof (type) * 2, &handle);         \
    if (!addr)                                                                \
      return NULL;                                                            \
    return dp_##name##_wrap_ (addr, n_samples, mapped, handle);               \
  }                                                                           \
                                                                              \
  /**                                                                         \
   * @brief Creates a double-mapped circular buffer BACKED BY A FILE.         \
   *                                                                          \
   * Identical to dp_##name##_create() except for where the pages come from:  \
   * the mapping is MAP_SHARED over @p path, so the ring's samples ARE the    \
   * file's contents. There is no separate write-to-disk path and no copy.    \
   *                                                                          \
   * An existing file of exactly the right size is mapped AS IT STANDS, which \
   * is what lets a ring outlive the process that filled it -- restore the    \
   * head/tail positions and the history is already there. Anything else is   \
   * created or truncated, which sizes and zeroes it.                         \
   *                                                                          \
   * The mapping is sized exactly as dp_##name##_create() sizes it, so the    \
   * FILE is `(->mask + 1) * sizeof(type) * 2` bytes -- the MAPPED samples,   \
   * not the capacity. A file is recognised by that size alone, and it does   \
   * not record the capacity: two requests that round to one mapping          \
   * re-attach the same file, and it is the caller who knows how much of it   \
   * was in use.                                                              \
   *                                                                          \
   * @param n_samples Capacity in complex samples, any size from 1 up.        \
   * @param path      File to back the ring with.                             \
   * @param existed   If non-NULL, set to 1 when the file already held a ring \
   *                  of this exact size (its samples are now the ring's).    \
   * @return Initialised buffer, or NULL on failure.                          \
   */                                                                         \
  static inline dp_##name##_t *dp_##name##_create_backed (                    \
      size_t n_samples, const char *path, int *existed)                       \
  {                                                                           \
    size_t mapped = dp_##name##_mapped_for_ (n_samples);                      \
    if (!mapped)                                                              \
      return NULL;                                                            \
    void *handle = NULL;                                                      \
    void *addr = dp__buf_alloc_file (mapped * sizeof (type) * 2, &handle,     \
                                     path, existed);                          \
    if (!addr)                                                                \
      return NULL;                                                            \
    return dp_##name##_wrap_ (addr, n_samples, mapped, handle);               \
  }                                                                           \
                                                                              \
  /**                                                                         \
   * @brief Flush a file-backed ring to disk; a no-op for an anonymous one.   \
   *                                                                          \
   * Call it where a CHECKPOINT is taken. Until then the samples live in the  \
   * page cache, so a snapshot written without this names a history a crash   \
   * can still lose.                                                          \
   */                                                                         \
  static inline void dp_##name##_sync (dp_##name##_t *ab)                     \
  {                                                                           \
    dp__buf_sync (ab->data, (ab->mask + 1) * sizeof (type) * 2);              \
  }                                                                           \
                                                                              \
  /** @brief Destroys the buffer and releases virtual memory. */              \
  static inline void dp_##name##_destroy (dp_##name##_t *ab)                  \
  {                                                                           \
    dp__buf_free (ab->data, (ab->mask + 1) * sizeof (type) * 2,               \
                  ab->_handle);                                               \
    free (ab);                                                                \
  }                                                                           \
                                                                              \
  /**                                                                         \
   * @brief Non-blocking write to the buffer.                                 \
   *                                                                          \
   * @param ab  Pointer to buffer.                                            \
   * @param src Source data array (n complex samples).                        \
   * @param n   Number of complex samples to write.                           \
   * Nothing is dropped here: the write is REFUSED, whole. @p src is not\
   * read on the refusal path, so the caller still holds every sample and\
   * may retry after the consumer makes room. Samples are lost only if the\
   * caller discards them -- which is what ignoring the return value does.\
   *                                                                          \
   * @p ab->dropped is therefore a count of samples in REFUSED CALLS, not a\
   * count of samples lost: a producer that spins on this until it succeeds\
   * inflates it without losing one (measured: 5,960,438 over a 60,000\
   * sample run). Wait for room if you want it to mean what it sounds like.\
   *                                                                          \
   * @return true if the samples were written, false if the ring had no room\
   *         for all @p n of them and the call was refused.                   \
   */                                                                         \
  JM_FORCEINLINE bool dp_##name##_write (dp_##name##_t *ab,                  \
                                         const type *src, size_t n)          \
  {                                                                           \
    size_t h = DP_LOAD_RLX (&ab->head);                                       \
    size_t t = DP_LOAD_ACQ (&ab->tail);                                       \
    /* n > capacity FIRST, and not only because it can never fit: the room    \
       below is computed from two indices, and if they are ever wrong --      \
       tail past head -- it comes out LARGER than the ring. Believing it      \
       copied past the mapping. consume() now refuses the one way in, but     \
       a memcpy must not depend on that: capacity is already loaded, so       \
       this costs the hot path one compare and no atomic. */                  \
    if (n > ab->capacity || (ab->capacity - (h - t)) < n)                     \
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
   * @brief Say that no more data is coming.                                  \
   *                                                                          \
   * The producer's half of end-of-stream. Until this exists, a consumer      \
   * cannot tell "the producer is slow" from "the producer has finished" --   \
   * both look like an empty ring -- so dp_##name##_wait() had nothing to do  \
   * but spin forever. Call it once, after the last write.                    \
   *                                                                          \
   * Release ordering: every sample written before this is visible to a       \
   * consumer that observes the flag.                                         \
   *                                                                          \
   * @param ab Pointer to buffer.                                             \
   */                                                                         \
  static inline void dp_##name##_close (dp_##name##_t *ab)                    \
  {                                                                           \
    __atomic_store_n (&ab->closed, 1, __ATOMIC_RELEASE);                      \
  }                                                                           \
                                                                              \
  /** @brief Non-zero once the producer has called dp_##name##_close(). */    \
  static inline int dp_##name##_closed (const dp_##name##_t *ab)              \
  {                                                                           \
    return __atomic_load_n (&ab->closed, __ATOMIC_ACQUIRE);                   \
  }                                                                           \
                                                                              \
  /**                                                                         \
   * @brief Blocking wait for a contiguous batch of samples.                  \
   *                                                                          \
   * Because of double-mapping, the returned pointer is guaranteed contiguous \
   * for @p n samples, allowing direct SIMD/AVX processing without copying.   \
   *                                                                          \
   * Returns NULL rather than spinning forever when the wait cannot be     \
   * satisfied, for any of three reasons the caller tells apart with        \
   * dp_##name##_closed(), dp_interrupted(), and @p n against capacity:     \
   *                                                                          \
   * - **end of stream** -- the producer has closed the ring AND fewer than \
   *   @p n samples remain. The tail is drained: whatever is left is less   \
   *   than a batch and no more is coming.                                  \
   * - **interrupted** -- somebody asked this process to stop.              \
   * - **unsatisfiable** -- @p n exceeds the ring's capacity, so no producer\
   *   can ever make it true. This one is a caller bug rather than a state, \
   *   and it is checked FIRST: it used to fall through to the spin and hang\
   *   forever at 100% CPU with nothing to read (doppler#1335). Sizing a    \
   *   block from the producer's chunk rather than from the ->capacity field\
   *   is the way in, and capacity is ROUNDED UP from what was requested,   \
   *   so the bound is not the number the caller passed to create().        \
   *                                                                          \
   * Both checks exist because this used to be an unbounded busy-spin with  \
   * no exit at all: a producer that stopped left the consumer spinning     \
   * forever at 100% CPU, and because the loop read no flag, no signal     \
   * handler could rescue it. See docs/design/io-termination.md.            \
   *                                                                          \
   * @param ab Pointer to buffer.                                             \
   * @param n  Minimum samples required.                                      \
   * @return   Pointer to the read-head, or NULL at end of stream or on an  \
   *           interrupt.                                                    \
   */                                                                         \
  static inline type *dp_##name##_wait (dp_##name##_t *ab, size_t n)          \
  {                                                                           \
    size_t h, t;                                                              \
    /* Unsatisfiable by construction -- the ring holds at most `capacity`,  \
       so head - tail can never reach n. Checked before the loop because    \
       the loop has no exit for it. */                                      \
    if (n > ab->capacity)                                                     \
      return NULL;                                                            \
    while (((h = DP_LOAD_ACQ (&ab->head)) - (t = DP_LOAD_RLX (&ab->tail)))    \
           < n)                                                               \
      {                                                                       \
        /* The re-load after observing `closed` is required, not tidy.    \
           close() is a RELEASE store and closed() an ACQUIRE load, so a    \
           consumer that observes the flag is guaranteed to see every write \
           that preceded it -- but only if it looks again. Using the `h`    \
           read before the acquire would discard a final batch the producer \
           had already published.                                           \
                                                                            \
           Narrow enough that no deterministic test reaches it: the branch  \
           needs the consumer to see the flag and NOT yet the data, and if  \
           the write is already visible the loop exits above without coming \
           here at all. Sabotaging it leaves the suite green, which is why  \
           the reasoning is written down rather than left to a test. */     \
        if (dp_##name##_closed (ab))                                          \
          {                                                                   \
            h = DP_LOAD_ACQ (&ab->head);                                      \
            t = DP_LOAD_RLX (&ab->tail);                                      \
            if (h - t < n)                                                    \
              return NULL;                                                    \
            break;                                                            \
          }                                                                   \
        if (dp_interrupted ())                                                \
          return NULL;                                                        \
        DP_SPIN_HINT ();                                                      \
      }                                                                       \
    (void)h;                                                                  \
    return &ab->data[(t & ab->mask) * 2];                                     \
  }                                                                           \
                                                                              \
  /**                                                                         \
   * @brief Samples written but not yet consumed.                             \
   *                                                                          \
   * The largest @p n for which dp_##name##_wait() is guaranteed to return    \
   * without spinning. Read from the consumer side: the producer may only     \
   * grow this concurrently, so the value is a lower bound that never goes    \
   * stale in the unsafe direction.                                           \
   *                                                                          \
   * @param ab Pointer to buffer.                                             \
   * @return   Number of samples currently readable.                          \
   */                                                                         \
  static inline size_t dp_##name##_available (const dp_##name##_t *ab)        \
  {                                                                           \
    size_t h = DP_LOAD_ACQ (&ab->head);                                       \
    size_t t = DP_LOAD_RLX (&ab->tail);                                       \
    return h - t;                                                             \
  }                                                                           \
                                                                              \
  /**                                                                         \
   * @brief Free room, in complex samples -- the producer's view.             \
   *                                                                          \
   * The largest @p n dp_##name##_write() is guaranteed to accept. Read from  \
   * the PRODUCER side: the consumer may only grow it concurrently, so the    \
   * value is a lower bound that never goes stale in the unsafe direction.    \
   *                                                                          \
   * @param ab Pointer to buffer.                                             \
   * @return   Samples that can be written without being refused.             \
   */                                                                         \
  static inline size_t dp_##name##_space (const dp_##name##_t *ab)            \
  {                                                                           \
    size_t h = DP_LOAD_RLX (&ab->head);                                       \
    size_t t = DP_LOAD_ACQ (&ab->tail);                                       \
    return ab->capacity - (h - t);                                            \
  }                                                                           \
                                                                              \
  /**                                                                         \
   * @brief Write as much of @p src as fits; return how much that was.        \
   *                                                                          \
   * The streaming half of the ring. dp_##name##_write() is all-or-nothing,   \
   * which is right for a frame and wrong for a stream: a chunk larger than   \
   * the free room is refused whole, and one larger than the CAPACITY can     \
   * never be written at all. This takes what fits, so a producer feeds any   \
   * chunk -- larger than the ring included -- by looping, draining between   \
   * calls:                                                                   \
   *                                                                          \
   * @code                                                                    \
   *     while (off < n)                                                      \
   *       {                                                                  \
   *         off += dp_f32_write_some (ab, src + 2 * off, n - off);           \
   *         while ((frame = dp_f32_peek (ab, nfft)))                         \
   *           { process (frame); dp_f32_consume (ab, hop); }                 \
   *       }                                                                  \
   * @endcode                                                                 \
   *                                                                          \
   * It never touches @c dropped: nothing is refused, so there is nothing to  \
   * count. A return of 0 means the ring is full.                             \
   *                                                                          \
   * @param ab  Pointer to buffer.                                            \
   * @param src Source data array (@p n complex samples).                     \
   * @param n   Complex samples offered.                                      \
   * @return    Complex samples actually written, 0..@p n.                    \
   */                                                                         \
  JM_FORCEINLINE size_t dp_##name##_write_some (dp_##name##_t *ab,            \
                                                const type *src, size_t n)    \
  {                                                                           \
    size_t h     = DP_LOAD_RLX (&ab->head);                                   \
    size_t t     = DP_LOAD_ACQ (&ab->tail);                                   \
    size_t space = ab->capacity - (h - t);                                    \
    if (space > ab->capacity) /* indices corrupt (see write()): stay in */    \
      space = ab->capacity;   /* the mapping whatever they claim       */     \
    if (n > space)                                                            \
      n = space;                                                              \
    if (n == 0)                                                               \
      return 0;                                                               \
    memcpy (&ab->data[(h & ab->mask) * 2], src, n * sizeof (type) * 2);       \
    DP_STORE_REL (&ab->head, h + n);                                          \
    return n;                                                                 \
  }                                                                           \
                                                                              \
  /**                                                                         \
   * @brief The non-blocking dp_##name##_wait(): @p n samples, or NULL.       \
   *                                                                          \
   * Returns the same contiguous, zero-copy pointer wait() does when @p n     \
   * samples are readable, and NULL at once when they are not -- it never     \
   * spins. This is what a SINGLE-THREADED user needs, where wait() would     \
   * deadlock: the same thread that would produce the samples is the one      \
   * waiting for them. Accumulate until a frame is there, then take it:       \
   *                                                                          \
   * @code                                                                    \
   *     dp_f32_write_some (ab, x, n);                                        \
   *     if ((frame = dp_f32_peek (ab, N)))                                   \
   *       { process (frame); dp_f32_consume (ab, N); }                       \
   * @endcode                                                                 \
   *                                                                          \
   * Like wait(), it does not consume: call dp_##name##_consume() with        \
   * however many samples to release, which need not be @p n -- releasing     \
   * fewer is how overlapped frames (hop < frame) are read.                   \
   *                                                                          \
   * NULL here is usually not a failure. Ask dp_##name##_wait_status() when   \
   * the difference between "not yet" and "never" matters.                    \
   *                                                                          \
   * @param ab Pointer to buffer.                                             \
   * @param n  Samples required.                                              \
   * @return   Pointer to the read-head, or NULL if fewer than @p n are       \
   *           readable (always, when @p n exceeds capacity).                 \
   */                                                                         \
  JM_FORCEINLINE type *dp_##name##_peek (dp_##name##_t *ab, size_t n)         \
  {                                                                           \
    size_t h = DP_LOAD_ACQ (&ab->head);                                       \
    size_t t = DP_LOAD_RLX (&ab->tail);                                       \
    if (h - t < n)                                                            \
      return NULL;                                                            \
    return &ab->data[(t & ab->mask) * 2];                                     \
  }                                                                           \
                                                                              \
  /**                                                                         \
   * @brief Why @p n samples can or cannot be had right now.                  \
   *                                                                          \
   * The one owner of the precedence dp_##name##_wait() applies. Too-large    \
   * is checked first because it is a caller bug whatever the ring holds;     \
   * readable samples win over closed, since a closed ring still drains;      \
   * and closed wins over interrupted, since "no more is coming" is the       \
   * more final of the two. After observing @c closed it looks at the count   \
   * AGAIN, for the reason wait() does: a close() is a release store, so a    \
   * consumer that sees the flag is guaranteed to see every write before      \
   * it -- but only if it looks again.                                        \
   *                                                                          \
   * @param ab Pointer to buffer.                                             \
   * @param n  Samples wanted.                                                \
   * @return   A dp_wait_status_t.                                            \
   */                                                                         \
  static inline dp_wait_status_t dp_##name##_wait_status (                    \
      const dp_##name##_t *ab, size_t n)                                      \
  {                                                                           \
    if (n > ab->capacity)                                                     \
      return DP_WAIT_TOO_LARGE;                                               \
    if (dp_##name##_available (ab) >= n)                                      \
      return DP_WAIT_OK;                                                      \
    if (dp_##name##_closed (ab))                                              \
      return dp_##name##_available (ab) >= n ? DP_WAIT_OK : DP_WAIT_CLOSED;   \
    if (dp_interrupted ())                                                    \
      return DP_WAIT_INTERRUPTED;                                             \
    return DP_WAIT_PENDING;                                                   \
  }                                                                           \
                                                                              \
  /**                                                                         \
   * @brief Empty the ring and reopen it.                                     \
   *                                                                          \
   * Both positions return to zero and @c closed is cleared, so a ring that   \
   * reached end of stream can carry the next one. @c dropped is a lifetime   \
   * count and is kept. NOT safe against a concurrent producer or consumer:   \
   * it writes both sides' indices, so both sides must be quiescent -- it is  \
   * for the single-threaded user and for between-runs.                       \
   *                                                                          \
   * @param ab Pointer to buffer.                                             \
   */                                                                         \
  static inline void dp_##name##_reset (dp_##name##_t *ab)                    \
  {                                                                           \
    DP_STORE_REL (&ab->head, 0);                                              \
    DP_STORE_REL (&ab->tail, 0);                                              \
    __atomic_store_n (&ab->closed, 0, __ATOMIC_RELEASE);                      \
  }                                                                           \
                                                                              \
  /**                                                                         \
   * @brief Releases @p n samples after processing is complete.               \
   *                                                                          \
   * Refuses, releasing NOTHING, when @p n exceeds dp_##name##_available().   \
   * Past that the tail overtakes the head and every later count is wrong --  \
   * space() comes out larger than the ring -- so the bound is what keeps     \
   * the two indices a description of the ring at all. A caller that          \
   * releases only what wait() or peek() lent never meets it, and may         \
   * ignore the return.                                                       \
   *                                                                          \
   * The bound reads the producer's index. That costs nothing measurable,     \
   * one thread or two: the consumer's preceding wait() or peek() has just    \
   * loaded the same line (docs/design/ring-buffer-measurements.md).          \
   *                                                                          \
   * @param ab Pointer to buffer.                                             \
   * @param n  Samples to release; 0 is a no-op.                              \
   * @return DP_OK, or DP_ERR_INVALID if @p n exceeds what is readable.       \
   */                                                                         \
  static inline int dp_##name##_consume (dp_##name##_t *ab, size_t n)         \
  {                                                                           \
    size_t h = DP_LOAD_ACQ (&ab->head);                                       \
    size_t t = DP_LOAD_RLX (&ab->tail);                                       \
    if (n > h - t)                                                            \
      return DP_ERR_INVALID;                                                  \
    DP_STORE_REL (&ab->tail, t + n);                                          \
    return DP_OK;                                                             \
  }

/* -------------------------------------------------------------------------
 * The element-typed face
 * ---------------------------------------------------------------------- */

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define DP_ASSERT_2X(tag, elem, type)                                         \
  _Static_assert (sizeof (elem) == 2 * sizeof (type),                         \
                  "view element must span two stored scalars")
#else
#define DP_ASSERT_2X(tag, elem, type)                                         \
  typedef char dp_assert_2x_##tag[sizeof (elem) == 2 * sizeof (type) ? 1 : -1]
#endif

/**
 * @def DECLARE_DP_BUFFER_VIEW(name, type, elem)
 * @brief Declares the element-typed face of the @p name ring.
 *
 * The ring stores SCALARS (`type *data`, two per complex sample); a caller
 * that thinks in samples -- the Python binding above all -- wants one
 * ELEMENT per sample. `dp_<name>_wait_view()`, `_peek_view()`,
 * `_write_view()` and `_write_some_view()` are the same four calls
 * addressed that way. Each is a cast and nothing else: every count in this
 * header is already in samples, so no length arithmetic is introduced that
 * could disagree with the scalar face, and the view stays zero-copy.
 *
 * Siblings rather than a change to the scalar face, because the scalar face
 * is what every C consumer addresses. One macro rather than three pairs,
 * because a cast written three times is three places for the element type
 * to drift -- which is what doppler#1346 was.
 *
 * It is instantiated by the header that owns the element type
 * (`f32_buffer/f32_buffer_core.h` and its two siblings), not here: the
 * complex element is spelled through the portability header those include,
 * and this file stays free of it.
 *
 * @param name  Ring instance suffix, as passed to #DECLARE_DP_BUFFER.
 * @param type  Stored scalar type (`float`, `int16_t`, ...).
 * @param elem  Element type spanning exactly two scalars.
 *
 * @code
 * dp_f32_t *ab = dp_f32_create (1024);
 * float _Complex x[4] = { 1, 2, 3, 4 };
 * dp_f32_write_some_view (ab, x, 4);              // 4 samples
 * float _Complex *v = dp_f32_peek_view (ab, 4);   // 4 samples, not 8 floats
 * dp_f32_consume (ab, 4);
 * dp_f32_destroy (ab);
 * @endcode
 */
#define DECLARE_DP_BUFFER_VIEW(name, type, elem)                              \
                                                                              \
  DP_ASSERT_2X (name, elem, type);                                            \
                                                                              \
  static inline elem *dp_##name##_wait_view (dp_##name##_t *ab, size_t n)     \
  {                                                                           \
    return (elem *)dp_##name##_wait (ab, n);                                  \
  }                                                                           \
                                                                              \
  static inline elem *dp_##name##_peek_view (dp_##name##_t *ab, size_t n)     \
  {                                                                           \
    return (elem *)dp_##name##_peek (ab, n);                                  \
  }                                                                           \
                                                                              \
  static inline bool dp_##name##_write_view (dp_##name##_t *ab,               \
                                             const elem *src, size_t n)       \
  {                                                                           \
    return dp_##name##_write (ab, (const type *)src, n);                      \
  }                                                                           \
                                                                              \
  static inline size_t dp_##name##_write_some_view (                          \
      dp_##name##_t *ab, const elem *src, size_t n)                           \
  {                                                                           \
    return dp_##name##_write_some (ab, (const type *)src, n);                 \
  }

/* --- Type instantiations --- */

DECLARE_DP_BUFFER (f32, float)  /**< 32-bit float  complex (8 bytes/sample)  */
DECLARE_DP_BUFFER (f64, double) /**< 64-bit double _Complex (16 bytes/sample) */
DECLARE_DP_BUFFER (i16, int16_t) /**< 16-bit int    complex (4 bytes/sample) */

#endif /* DP_BUFFER_H */
