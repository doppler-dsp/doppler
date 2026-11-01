- **just-makeit pin 0.112.0 → 0.114.0: a destroyed object raises instead of
    reaching C with a NULL handle.** Every generated method now checks the
    handle first and raises `RuntimeError("destroyed")` after `close()`.
