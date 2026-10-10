- **`BurstCapture` never refuses input, and every epoch is the burst's
    stream position.** At 8192-sample blocks with `release()` on every
    window, one refused history write was permanent: 73728 samples dropped,
    4 of 15 bursts captured, and every later `preamble_start` early by what
    was refused. `push()` now writes what fits and loops, a detection whose
    history is gone is swept and counted in `dropped`, and a checkpoint
    carries the whole ring, so a resume emits what the live capture would.
    State blobs move to version 5 (#2015, #2028).
