- **A `pn_poly` wider than `pn_length` is refused.** The generator masked it
    to the register, so `wfmgen --type pn --pn-length 5 --pn-poly 0x40` wrote
    a constant waveform -- the seed, then zeros -- at exit 0. Refused on
    every face (CLI, scene, `Composer`, `Synth`), and the CLI names both
    values (#1636).
- **A seed whose low `pn_length` bits are zero starts the register at 1.**
    `dp_pn_create` and `dp_gold_create` checked `seed == 0` before masking,
    so a non-zero multiple of 2^length emptied the register and generated a
    constant stream at exit 0 -- `wfmgen --type pn --pn-length 7 --seed 128`,
    and one emitter of the `plan_background` gallery scene (`1000 + k` hit
    1024 on 9 bits). A source's seed also seeds its noise, so it is not
    refused: it starts the register at 1, as seed 0 does. The constructors
    themselves now refuse a masked-zero seed (#1640).
