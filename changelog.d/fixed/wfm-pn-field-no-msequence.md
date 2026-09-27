- **A PN frame field with no m-sequence is refused.** A generated preamble,
    sync or payload with a 1-bit register and no explicit polynomial rendered
    the seed and then zeros, and reported success. It is now refused, the
    frame-field twin of the source fix in #1590 (#1602).
