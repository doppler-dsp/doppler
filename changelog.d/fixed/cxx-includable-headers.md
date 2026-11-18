- **A C++ program can include doppler's public headers directly, and CI
    now checks it.** 152 of the 164 installed headers parse as C++11; 9
    remain, on a list `make cxx-headers-check` only lets shrink, and 3
    private or Python-only headers are declared not to be C++ headers at
    all. The complex types cross as they are; an inline body built on the
    C99 complex library is C-only (`dp_complex.h`, "From C++"), and
    `dp_cic_decimate()` is now also exported, so C++ has an entry point. No
    ABI change for a C caller (#3108).
