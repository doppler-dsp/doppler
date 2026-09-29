- **A coded frame is a description: `wfmgen --frame FILE`.** The six coding
    flags `--rs-depth`, `--randomise`/`--randomize`, `--asm`, `--conv`,
    `--interleave` and `--interleave-unit` are refused, each naming
    `--frame FILE`. The scene keys `rs_depth`, `randomise`, `asm`, `conv`,
    `interleave` and `interleave_unit` are refused, naming `"frame"`. The
    file is what a scene's `"frame"` key holds, and `--record` carries it
    whole. From Python, compose a coded source from a scene with a
    `"frame"` (`Composer.from_json`); the `Source` kwargs `rs_depth`,
    `randomise`, `attach_asm` and `convolutional` are gone, and a `Source`
    takes no description until #1617. A `FrameDesc`'s `bits()` handed over
    as the payload gives the same samples for `type="bits"` and for a DSSS
    burst with `crc="none"`, but its record holds the bits, not the stages.
    Every coded case is byte-identical to the flag-spelled run it
    replaces (#853).
