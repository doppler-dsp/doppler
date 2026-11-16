- **A C++ program can include `fir_core.h`, `lo_core.h`, `nco_core.h` and
    `util_core.h` directly, and CI now checks it.** 56 of the 164 installed
    headers did not parse as C++11; 46 remain, on a list
    `make cxx-headers-check` only lets shrink. The complex types cross as
    they are; an inline body built on the C99 complex library is C-only
    (`dp_complex.h`, "From C++"). No ABI change (#3108).
