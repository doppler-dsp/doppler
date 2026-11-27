- **A DDC mixes a sample the same way on every path.** Compiled with fused
    multiply-adds (ARM, or x86 built for the host), the block form, the
    one-sample push form and the frequency port could mix the same sample
    to different last bits; a 16-bit CIC used to round that away. The mix
    is now computed without contraction, and unvectorised under clang
    ([why](native/src/ddc/ddc_extra.cmake)).
