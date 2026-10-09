- **Fixed-size frames from any chunking, with no loop of your own.** The ring
    gains a framed face (`dp_f32_framer_*`, plus `f64` and `i16`): feed any
    chunk, take overlapping frames, flush the one zero-padded row a stream's end
    owes. The frames are the same however the input was split, at the speed of
    the hand-written loop (0.355 vs 0.356 ns/sample). See the
    [ring design page](design/ring-buffer.md#9-the-framed-face-any-chunk-in-fixed-frames-out).
