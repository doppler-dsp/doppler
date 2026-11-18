- **A CRC guide page and a C example.** `docs/guide/crc.md` lists the nine named
    models with their parameters, `refin` and check value (held to `dp_crc.h` by
    a test) and says the one thing a caller must get right, that a named
    instance does not carry `refin`; `crc_demo.c` computes all nine from octets
    and from a bit stream and shows a wrong `refin` giving a wrong CRC.
