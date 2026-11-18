- **A CRC guide page and a C example.** `docs/guide/crc.md` lists the nine named
    models with their parameters, `refin` and check value (held to `dp_crc.h` by
    a test) and says the one thing a caller must get right, that `refin` is part of
    the model, so a flipped one is another CRC with no error; `crc_demo.c`
    computes all nine from octets and from a bit stream and shows exactly
    that.
