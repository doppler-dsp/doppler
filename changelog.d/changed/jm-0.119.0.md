- **just-makeit pin 0.117.0 → 0.119.0: a status code can name its own
    exception.** A `status_return` method's `status_errors` and a
    serializable object's `set_state_status_errors` map a C status to a
    Python exception (just-makeit#2470), which the next entry uses for
    `MemoryError`. 0.118.1 also refuses a manifest parameter order that
    does not match a sacred `_core.c` definition (just-makeit#2465). With
    no table, the bindings are byte-identical.
