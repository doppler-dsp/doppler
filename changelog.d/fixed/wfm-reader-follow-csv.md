- **`Reader.read_follow` on a CSV capture returns its samples, not its text
    as floats.** The follow read skipped the file-type check every other
    entry point makes and decoded a CSV's bytes through the binary path. It
    now follows a CSV by complete lines, with the same parser `read()` uses,
    leaving a line the writer has not finished for the next call, and
    returns exactly what `read()` returns (#2658).
