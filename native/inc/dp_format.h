/**
 * @file dp_format.h
 * @brief Complex sample formats, named by their BLUE/Platinum codes.
 *
 * One vocabulary for the five interleaved-I/Q encodings doppler moves, used
 * by both containers that carry them: the streaming wire header
 * (`stream/stream.h`) and the BLUE file writer (`wfm_writer`). It lives in
 * neither of those because it belongs to neither — the codes are Midas BLUE
 * 1.1 Table 6's, and a transport that had to be linked in order to name a
 * file's sample format would be the wrong dependency in the wrong
 * direction.
 *
 * There used to be three enumerations of these five types: the stream's
 * `dp_sample_type_t`, `wfm_writer`'s `stype` in "wavegen order", and
 * `wfm_sink.c`'s `WT_*`, plus a `FMTCH[]` table mapping one of them to BLUE
 * and a `BPS[]` table repeating the sizes. They agreed on nothing and were
 * reconciled by hand at every boundary. Naming a format by the code the file
 * format already defines leaves one vocabulary and nothing to translate.
 *
 * Everything here is a `static inline` over a switch, so a consumer needs
 * the header and no link edge.
 *
 * @code
 * // The wire code and the file's HCB bytes 52/53 are the same two chars.
 * char code[2];
 * dp_format_chars (CF64, code);      // code = { 'C', 'D' }
 * size_t n = dp_format_size (CF64);  // 16 bytes per complex sample
 * @endcode
 */
#ifndef DP_FORMAT_H
#define DP_FORMAT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Pack a BLUE two-character format code into a @c uint16_t.
   *
   * Mode in the low byte, element type in the high byte, so a little-endian
   * hex dump of the wire field reads as the two characters in order.
   * doppler uses mode @c 'C' — complex, two components per element.
   */
#define DP_FMT(mode, type)                                                    \
  ((uint16_t)((uint16_t)(unsigned char)(mode)                                 \
              | ((uint16_t)(unsigned char)(type) << 8)))

  /**
   * @brief A complex sample format. The value IS the BLUE code.
   *
   * There is no code for a quad or extended float because BLUE defines
   * none — which is the format agreeing with why doppler retired CF128: its
   * representation differs between x86-64 and aarch64 at identical size, so
   * a frame crossed an architecture boundary and decoded to nonsense.
   */
  typedef enum
  {
    CI8  = DP_FMT ('C', 'B'), /**< int8_t I/Q    (2 bytes/sample).  */
    CI16 = DP_FMT ('C', 'I'), /**< int16_t I/Q   (4 bytes/sample).  */
    CI32 = DP_FMT ('C', 'L'), /**< int32_t I/Q   (8 bytes/sample).  */
    CF32 = DP_FMT ('C', 'F'), /**< float I/Q     (8 bytes/sample).  */
    CF64 = DP_FMT ('C', 'D'), /**< double I/Q    (16 bytes/sample). */
  } dp_sample_type_t;

  /**
   * @brief Bytes occupied by one complex sample of @p type, or 0 if the code
   * is not one doppler sends.
   *
   * This switch is the single table: validity, element size and the wire
   * layout all derive from it, so a format added here needs no second edit
   * and a code that is not here is not a doppler format anywhere.
   *
   * @param type Sample format.
   * @return Bytes per complex sample, or 0.
   */
  static inline size_t
  dp_format_size (dp_sample_type_t type)
  {
    switch (type)
      {
      case CI8:
        return 2u * sizeof (int8_t);
      case CI16:
        return 2u * sizeof (int16_t);
      case CI32:
        return 2u * sizeof (int32_t);
      case CF32:
        return 2u * sizeof (float);
      case CF64:
        return 2u * sizeof (double);
      default:
        return 0u;
      }
  }

  /**
   * @brief Full-scale magnitude of one I or Q component of @p type.
   *
   * The divisor that puts an integer format on the same footing as a float
   * one, so a number derived from samples (a power, an RMS, a headroom)
   * means the same thing whatever the wire carried. 1.0 for the float
   * formats, which are already full-scale, and 0 for a code this build does
   * not know.
   *
   * @param type Sample format.
   * @return Full-scale value per component, or 0.
   */
  static inline double
  dp_format_full_scale (dp_sample_type_t type)
  {
    switch (type)
      {
      case CI8:
        return 127.0;
      case CI16:
        return 32767.0;
      case CI32:
        return 2147483647.0;
      case CF32:
      case CF64:
        return 1.0;
      default:
        return 0.0;
      }
  }

  /**
   * @brief Non-zero when @p type is a format doppler can send and decode.
   *
   * Derived from dp_format_size(): a format with no size is not a format.
   * Ask this rather than range-testing — the codes are two packed
   * characters, so "between the first and the last" means nothing.
   */
  static inline int
  dp_format_is_valid (dp_sample_type_t type)
  {
    return dp_format_size (type) != 0u;
  }

  /**
   * @brief The two characters BLUE writes for @p type (HCB bytes 52/53).
   *
   * Unpacks rather than translates — the enum value IS the code — so a wire
   * header and a file header cannot disagree about what they carry.
   *
   * @param type Sample format.
   * @param[out] out Two characters, mode then element type. Not
   *                 NUL-terminated.
   */
  static inline void
  dp_format_chars (dp_sample_type_t type, char out[2])
  {
    out[0] = (char)((uint16_t)type & 0xFFu);
    out[1] = (char)(((uint16_t)type >> 8) & 0xFFu);
  }

#ifdef __cplusplus
}
#endif

#endif /* DP_FORMAT_H */
