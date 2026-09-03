- **`AsyncDsssReceiver` allocates nothing per `steps()` while tracking.** The
    track chain used to `malloc`/`free` two scratch buffers on every call; it
    now runs one code period at a time through scratch sized once per chain
    build, the refine stage's own shape. Output is bit-exact before and after
    at every block size tried; warm cost unchanged (44.5 ns/sample at the 5
    Mcps operating point). Closes #1192.
