- **A JSON scene that omits a key gets the same default as the flags and
    Python.** The reader typed its own: `seed`/`sps`/`pn_length` were 1/8/7
    against 0/1/15, and an omitted `num_samples` made an empty segment. It now
    reads the generated manifest defaults (#1596).
