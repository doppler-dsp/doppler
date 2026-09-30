# The Payload as a Data Source — the measurement record

The dated record behind [the design page](payload-data-source.md), under
the same section numbers. The design page states what is; this page
records what was measured to get there, in order.

______________________________________________________________________

## 4. The rules the shape leaves open

### 4.0 The chunker, prototyped before any C (2026-09-30)

A throwaway Python chunker (about 180 lines, kept in a scratch directory
and not committed, per
[the lifecycle](../dev/contributing/adding-algorithms.md#start-with-why))
implemented §F.5's four rules over a finite source and a non-blocking
file descriptor. It asserted seven cases:

| case                                           | result                                                                                                     |
| ---------------------------------------------- | ---------------------------------------------------------------------------------------------------------- |
| 96 bits, `LEN` 32                              | 3 data frames, bits identical                                                                              |
| 90 bits, `LEN` 32, no fill                     | refused: "26 bits into a 32-bit frame; 6 bits short"                                                       |
| 90 bits, `LEN` 32, fill `01`                   | 3 frames; the last carries 6 fill bits, `010101`                                                           |
| 0 bits                                         | 0 frames (which led to §4.3: refuse it)                                                                    |
| a pipe of 24 bits, `LEN` 10                    | data, data, then the end padded with 6 bits; octets straddle frames and the bits come back in order (§4.2) |
| a paced pipe that pauses for 2.4 frame periods | `data, idle, idle, data` (§4.1)                                                                            |
| a paced pipe that underruns, no fill           | refused, but only at the underrun (which led to §4.4)                                                      |

What it changed:

- **§4.1:** the prototype held a partial residue through the idle frames
    and gave it to the next data frame. Writing that down made it a rule,
    rather than an accident of the loop.
- **§4.4:** the refusal for a stream without a fill arrived mid-run in
    the prototype, after frames had been emitted. That is why the design
    requires the fill up front.
- **§4.5:** the paced case's output depends on scheduling. The prototype
    had to sleep to make it happen, so the C tests must inject the
    underrun instead.
