- **A wide `Acquisition` dwell decides about a third faster.** The pick's
    masked argmax over the surface was a scalar loop, 37% of a ±50 kHz
    `AsyncDsssPool` push. It now finds each block's largest candidate in a
    vectorizable pass and runs the loop only where a block can move the
    pick, giving the same cell. On the pinned pool bench the full-span rows
    went from 241.8 to 174.4 ns a sample, and the native rows from 30.3 to
    27.2.
