- **A tiled `BurstCapture` anchors each detection at its own frame.** With
    `doppler_uncertainty` past the native span, acquisition tiles Doppler
    over one code period, but every detection was anchored as if its frame
    were all W tiles long: (W − 1) periods early. With `reps` below W − 1
    every burst came back at the wrong start. A burst in the stream's first
    (W − 1) periods came back with a garbage start, and a live checkpoint
    holding one was refused (#2090).
