---
description: "Compute any CRC from its model, over octets or over the library's bit streams, with nine named catalogue instances, and why the input bit order is the one thing you must get right."
---

# CRC (`dp_crc.h`)

`dp_crc.h` computes a CRC from its **model**, so a frame's check sequence is
a row of data and not another copy of a loop. It is a C header with no
library to link, for C callers; there is no Python class. The frame builders
use it (the 802.11 FCS in [`wlan`](wlan.md) is CRC-32/ISO-HDLC over the MPDU's
octets), and a caller with a field that carries a CRC states the model and
calls the same function.

A CRC model is six numbers in `dp_crc_t`: the register `width` in bits, the
generator polynomial `poly` (normal form, without the top term), the initial
register `init`, whether the final register is bit-reversed (`refout`), a
value XORed into the result (`xorout`), and the catalogue's `refin`, whether
each input octet's bits are presented least significant first.

**Two entry points, one loop.** `dp_crc_bytes(&model, octets, n)` takes
octets and presents each one's bits in the order the model's `refin` asks for;
`dp_crc_bits(&model, bits, n)` takes the library's unpacked streams, one bit
per byte, shifted in first bit first, and never reads `refin`: a stream
already in bit order has nothing left to reflect, so the order is whatever
the caller built.

## Named models

Nine catalogue instances are macros, each pinned in the unit test to the
catalogue's check value (the CRC of the nine characters `123456789`):

| macro                | width | poly                 | init                 | refin | refout | xorout               | check                |
| -------------------- | ----- | -------------------- | -------------------- | ----- | ------ | -------------------- | -------------------- |
| `DP_CRC_32_ISO_HDLC` | 32    | `0x04C11DB7`         | `0xFFFFFFFF`         | true  | true   | `0xFFFFFFFF`         | `0xCBF43926`         |
| `DP_CRC_32_BZIP2`    | 32    | `0x04C11DB7`         | `0xFFFFFFFF`         | false | false  | `0xFFFFFFFF`         | `0xFC891918`         |
| `DP_CRC_32C`         | 32    | `0x1EDC6F41`         | `0xFFFFFFFF`         | true  | true   | `0xFFFFFFFF`         | `0xE3069283`         |
| `DP_CRC_16_IBM_3740` | 16    | `0x1021`             | `0xFFFF`             | false | false  | `0x0000`             | `0x29B1`             |
| `DP_CRC_16_XMODEM`   | 16    | `0x1021`             | `0x0000`             | false | false  | `0x0000`             | `0x31C3`             |
| `DP_CRC_16_KERMIT`   | 16    | `0x1021`             | `0x0000`             | true  | true   | `0x0000`             | `0x2189`             |
| `DP_CRC_8_SMBUS`     | 8     | `0x07`               | `0x00`               | false | false  | `0x00`               | `0xF4`               |
| `DP_CRC_8_ROHC`      | 8     | `0x07`               | `0xFF`               | true  | true   | `0x00`               | `0xD0`               |
| `DP_CRC_64_ECMA_182` | 64    | `0x42F0E1EBA9EA3693` | `0x0000000000000000` | false | false  | `0x0000000000000000` | `0x6C40DF5F0B497347` |

`test_crc_guide_table.py` holds this table to the header: a value changed in
one place and not the other fails the build. **A named instance is not a
claim that a standard uses it.** Which model a protocol field takes, and in
what bit order the value is transmitted, are facts in that protocol's text.

## The one thing to get right: `refin` is part of the model

The named instances carry their `refin`, so `dp_crc_bytes(&model, ...)` is
right without a second argument. Change it and you have a different CRC, with
no error: CRC-32/ISO-HDLC over `123456789` is `0xCBF43926` with `refin = 1`
and `0x1898913F` with `refin = 0`. The struct holds a catalogue model whose
`refin` and `refout` differ (CRC-12/UMTS is one); every named instance here
has them equal.

Write a model of your own with **designated initializers** (`.width = 8u, ...`), never by position: `refin` is the struct's last field, so a literal
written before it existed still compiles, and means `refin = 0`.

The C example computes all nine from the octets and from the bit stream,
shows what flipping `refin` does, and exits non-zero if a value is not the
catalogue's:

```c
--8<-- "native/examples/crc_demo.c"
```

A model of your own is a `dp_crc_t`; `dp_crc_valid()` says whether the header
can compute it (`width` 1 to 64, and `poly`, `init` and `xorout` fitting in
`width` bits).
