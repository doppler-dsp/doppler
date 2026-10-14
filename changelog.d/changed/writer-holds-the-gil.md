- **`Writer.write` and `.flush` hold the GIL again.** jm 0.105.0 released it
    for both, and a `close()` from another thread could then free the handle
    under a running write (#2294). This supersedes the v0.70.0 note on the ten
    methods that release the GIL: nine remain.
