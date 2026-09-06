- **The searcher fans its tiles across a persistent pool (design §2.3).**
    A tiled continuous `Acquisition` now runs the per-epoch tile loop and,
    at `D > 1`, the block-end column loop across workers created once with
    the engine and parked between pushes, the machine's online cores by
    default; `set_threads(n)` re-sizes it (0 = cores, 1 = serial) and
    `threads` reports it. Every tile owns its inverse plan and scratch, so
    the surface and the hits are byte-identical at any thread count. Burst
    and single-tile engines stay serial.
