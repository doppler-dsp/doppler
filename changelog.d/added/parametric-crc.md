- **One parametric CRC.** `dp_crc.h` computes any CRC from its model (width,
    polynomial, initial register, input reflection `refin`, output reflection,
    output XOR: six fields, set by designated initializers) over the
    library's unpacked bit streams or over octets, with nine named catalogue
    instances (CRC-32/ISO-HDLC, CRC-32C, CRC-16/IBM-3740, CRC-8/SMBUS, and
    others), each pinned to its catalogue check value and confirmed against
    zlib and `binascii` where they have it. `dp_crc16_ccitt` is now one row of
    it, with its output unchanged. Header-only; no protocol is claimed to use
    any model.
