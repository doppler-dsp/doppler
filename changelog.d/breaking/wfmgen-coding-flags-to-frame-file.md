- **A coded frame is a description: `wfmgen --frame FILE`.** The coding
    flags `--rs-depth`, `--randomise`, `--asm`, `--conv`, `--interleave`
    and `--interleave-unit`, and their scene keys, are refused, naming
    `--frame FILE` / `"frame"`; the file is what a scene's `"frame"` holds
    and `--record` carries it whole. The `Source` kwargs `rs_depth`,
    `randomise`, `attach_asm` and `convolutional` are gone: from Python,
    compose a coded source from a scene with a `"frame"` until #1617.
    Byte-identical to the flag-spelled runs (#853).
