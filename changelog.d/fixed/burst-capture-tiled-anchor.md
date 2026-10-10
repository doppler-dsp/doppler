- **A tiled `BurstCapture` anchors each detection at its own frame.** With
    `doppler_uncertainty` past the native span, each detection was anchored
    (W − 1) code periods early. A burst came back at the wrong start whenever
    reps + m < W − 1 (m: whole periods its deciding frame sat past the
    start), so every burst did once 2·reps < W − 1. A burst in the first
    (W − 1) periods came back with a garbage start, and one that closely
    followed a window was silently dropped (#2090). State blobs stay at
    version 5: a tiled checkpoint taken before the fix refines its queued
    anchors wrong once, which is acceptable while the format is unreleased.
