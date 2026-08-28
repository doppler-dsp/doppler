- **The Python benchmark ratchet is now checked for stale entries.** Rule 5
    skipped every `PY_HOLLOW_ALLOW` path and nothing looked at them again, so
    a file that started recording could keep its waiver and the set could stop
    being a ratchet with no run going red — the same gap the C ratchets closed
    one release earlier. An entry whose file records, or whose path no longer
    exists, now fails the gate, and what is left on each ratchet is printed on
    every green run instead of tallied in a comment.
