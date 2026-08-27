- **`coding.Interleaver` — a block interleaver, as an object.** Holds the
    geometry (`rows`, `cols`, `unit_bits`) so both ends of a link cannot
    disagree about the permutation, and de-interleaves `float32` soft values as
    well as hard bits — the path `DsssBurstReceiver.llrs` needs.
    [#1031](https://github.com/doppler-dsp/doppler/issues/1031)
