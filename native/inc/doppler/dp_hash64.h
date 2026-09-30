/**
 * @file dp_hash64.h
 * @brief FNV-1a 64 over bytes, incrementally: the one content hash in
 * doppler.
 *
 * It identifies content and does not protect it. A record names a
 * `--data-from-file` source by its path, its length in bits and this hash,
 * and a replay refuses a file whose hash differs
 * (docs/design/payload-data-source.md §4.8). A path alone replays whatever
 * the file holds on the day, and a length alone misses an edit of the same
 * size.
 *
 * FNV-1a rather than xxHash64 because identity needs no speed: the hash
 * runs at the rate a file is read, and a file is read at the rate frames
 * are sent. FNV-1a is a few lines, works a byte at a time and so is
 * incremental by construction, and has published test vectors. Not for
 * anything an adversary chooses: a collision can be constructed.
 *
 * Header-only, for the reason dp_crc16.h gives: no component grows a
 * link-line dependency for a few lines of arithmetic.
 */
#ifndef DP_HASH64_H
#define DP_HASH64_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief The FNV-1a 64 offset basis: the hash of no bytes, and where a
   *  hash starts. */
#define DP_HASH64_INIT UINT64_C (0xcbf29ce484222325)

  /** @brief The FNV-1a 64 prime. */
#define DP_HASH64_PRIME UINT64_C (0x100000001b3)

  /**
   * @brief Continue an FNV-1a 64 hash over @p n more bytes.
   *
   * Pass @ref DP_HASH64_INIT to start, then feed each block as it is read,
   * passing back what the last call returned. The result does not depend
   * on where the input was split, so a file hashed read by read gives the
   * same value as the whole file hashed at once, and is never read twice.
   * The state is the returned `uint64_t` itself, so a source that
   * serializes its state carries the hash by value.
   *
   * @param h     the hash so far; @ref DP_HASH64_INIT for none.
   * @param data  the next bytes; may be NULL when @p n is 0.
   * @param n     how many.
   * @return the hash over everything fed so far.
   *
   * @code
   * uint64_t h = DP_HASH64_INIT;
   * h = dp_hash64 (h, "foo", 3);
   * h = dp_hash64 (h, "bar", 3);   // 0x85944171f73967e8, as for "foobar"
   * @endcode
   */
  static inline uint64_t
  dp_hash64 (uint64_t h, const void *data, size_t n)
  {
    const unsigned char *p = (const unsigned char *)data;
    for (size_t i = 0; i < n; i++)
      {
        h ^= p[i];
        h *= DP_HASH64_PRIME;
      }
    return h;
  }

#ifdef __cplusplus
}
#endif

#endif /* DP_HASH64_H */
