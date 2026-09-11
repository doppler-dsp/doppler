- **`Corr2D` takes a `col_out` lag**, emitting one value per row instead of
    the whole correlation map. A caller that already knows its lag — refine
    knows the code phase modulo the epoch — gets the plain time-domain sum
    with no transform in either direction, `O(nx)` per row against the map's
    `O(nx log nx)`. This is what lets `BurstCapture` correlate through the
    shared kernel rather than hand-rolling the sum beside it.
