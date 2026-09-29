- **A coded frame is a description: `wfmgen --frame FILE`.** The six coding
    flags `--rs-depth`, `--randomise`/`--randomize`, `--asm`, `--conv`,
    `--interleave` and `--interleave-unit` are refused, each naming
    `--frame FILE`. The scene keys `rs_depth`, `randomise`, `asm`, `conv`,
    `interleave` and `interleave_unit` are refused, naming `"frame"`. The
    Python `Source` kwargs `rs_depth`, `randomise`, `attach_asm` and
    `convolutional` are gone; describe the frame with `FrameDesc` and pass
    its `bits()` as the payload. The file is what a scene's `"frame"` key
    holds, and `--record` carries it whole. Every coded case is
    byte-identical to the flag-spelled run it replaces (#853).
