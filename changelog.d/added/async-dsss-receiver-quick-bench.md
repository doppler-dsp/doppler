- **`make bench-rx` times the `AsyncDsssReceiver` alone, in seconds.** It
    runs the receiver's C bench: the held receiver at three operating
    points, now including the SPEC point (a 1,023-chip code at 3.069 Mcps,
    2 samples a chip), and, new, the SEARCHING receiver's own search over
    plus or minus one code-period frequency on noise. That is the per-sample
    cost the parallel design multiplies. Each row is recorded only if the
    receiver is still holding the code, or still searching, after the timed
    rounds.
