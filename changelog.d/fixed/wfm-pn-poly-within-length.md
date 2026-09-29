- **A `pn_poly` wider than `pn_length` is refused.** The generator masked it
    to the register, so `wfmgen --type pn --pn-length 5 --pn-poly 0x40` wrote
    a constant waveform -- the seed, then zeros -- at exit 0. Refused on
    every face (CLI, scene, `Composer`, `Synth`), and the CLI names both
    values (#1636).
