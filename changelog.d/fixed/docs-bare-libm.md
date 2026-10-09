- **The contributor guide no longer teaches a Windows link failure.**
    `adding-a-module.md` registered benchmarks against a bare `m`, the libm
    spelling the `bare-libm` gate refuses in real CMake. The snippet now links
    `${DP_MATH_LIBRARY}`, and the gate reads the `docs/dev/` cmake fences too.
