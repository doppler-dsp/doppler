- **just-makeit pin 0.126.1 → 0.133.0.** A portable x86-64 build now
    runs the SIMD macros on four float lanes (an SSE2 tier in `jm_simd.h`)
    and the dot product on four accumulators, so a result can differ from
    the previous release in the last place. A package's `__all__` no longer
    lists underscore names, and the choice tables carry docstrings in the
    stubs.
