# Hand-owned (jm includes this file and never writes it, gh-1351).
#
# wfm_synth_core.c is compiled with -ffp-contract=off (doppler#2805).
#
# The tree builds with -ffast-math, which implies -ffp-contract=fast. A
# compiler with baseline FMA (AArch64, or x86 with -mfma) then fuses the
# `a*c - b*d` of a complex multiply, and in a vectorised loop the body and the
# scalar tail fuse it differently, so a sample's last bit depended on where
# the caller's block ended: a Plan builds its cache in one run, a composer
# pulls 257 at a time, and they disagreed. The flag is the file's, not a
# function's: GCC documents `optimize` as for debugging, and clang's
# -ffast-math ignores the fp-contract pragma. Rounding every multiply-add of
# the synth separately is the same number on every compiler. The two-term
# products have nothing for -ffast-math to reassociate.
#
# It is not enough alone. Clang for ARMv8.3+ (Apple silicon) emits FCMLA for
# an interleaved complex multiply whatever -ffp-contract says, so the carrier
# mix also runs through one out-of-line, fixed-size block
# (wfm_mix_carrier, native/inc/doppler/wfm/wfm_private.h): one machine code
# body for every sample, vector body or tail.
#
# The flag comes AFTER -ffast-math, so clang says it is overriding an option;
# that is exactly the intent, and it is silenced for clang alone (turning
# fast-math off, as the validation harnesses do, costs the synth 4% to 44%).
dp_gnu_compile_options(wfm_synth_core PRIVATE -ffp-contract=off)
if(CMAKE_C_COMPILER_ID MATCHES "Clang")
  dp_gnu_compile_options(wfm_synth_core PRIVATE -Wno-overriding-option)
endif()
