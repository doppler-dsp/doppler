# Hand-owned (jm includes this file and never writes it, gh-1351).
#
# ddc_core.c is compiled with -ffp-contract=off, for the reason
# wfm_synth_extra.cmake gives (doppler#2805): the tree builds with
# -ffast-math, which implies -ffp-contract=fast, and on a compiler with
# baseline FMA (AArch64, or x86 built for the host) the `a*c - b*d` of the
# carrier mix's complex multiply is fused differently at different code
# sites. The block form, the one-sample push form, and two DDCs tuned by the
# LO or by the frequency port then mix the same sample to different last
# bits. A 16-bit CIC rounded that away; a CIC with a 64-bit word does not
# (#3375), and test_ddc_core.c's bit-for-bit checks went red on ARM.
# Measured on x86 with -march=native: red with contraction, green without.
#
# Clang for ARMv8.3+ (Apple silicon) emits FCMLA for a vectorised complex
# multiply whatever -ffp-contract says, so the mix loops also carry a
# clang-only no-vectorize pragma in ddc_core.c: every sample is then
# multiplied by the same scalar code in every path.
dp_gnu_compile_options(ddc_core PRIVATE -ffp-contract=off)
if(CMAKE_C_COMPILER_ID MATCHES "Clang")
  dp_gnu_compile_options(ddc_core PRIVATE -Wno-overriding-option)
endif()
