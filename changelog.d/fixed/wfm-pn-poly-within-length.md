- **A `pn_poly` wider than `pn_length` is refused.** The generator masked it
    to the register, so `wfmgen --type pn --pn-length 5 --pn-poly 0x40` wrote
    a constant waveform -- the seed, then zeros -- at exit 0. Refused on
    every face (CLI, scene, `Composer`, `Synth`), and the CLI names both
    values (#1636).
- **A seed that empties the PN register is refused.** `dp_pn_create` and
    `dp_gold_create` checked `seed == 0` before masking, so a non-zero
    multiple of 2^length (`wfmgen --type pn --pn-length 7 --seed 128`) filled
    the register with zeros and generated a constant stream at exit 0. The
    masked seed is checked now; a wide seed that masks to non-zero (129 on
    7 bits) still builds (#1640).
