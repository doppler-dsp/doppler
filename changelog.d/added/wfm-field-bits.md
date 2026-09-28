- **`doppler.wfm.field_bits(text)`** turns a Field's text form into bits:
    `field_bits("0x1ACFFC1D")`, `field_bits("pn:1023:10")`,
    `field_bits("dotted:8*2")`. It is the one door from text to the bits a
    frame takes, over the C parser, and malformed text raises rather than
    returning an empty array (#853).
