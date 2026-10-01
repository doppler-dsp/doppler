- **The shared libraries export only the public API** (#1164).
    `libdoppler.so` exported 1658 symbols, including vendored cJSON and
    PFFFT, so a program with its own cJSON had doppler call its parser.
    Exports now come from the headers: 1556, and 89 for the stream library.
