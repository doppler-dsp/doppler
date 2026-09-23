- **`burst_capture_create_template()`: capture a burst behind any repeated
    complex preamble** (#1470 phase 5), with a file-backed twin. One chip is
    one sample. Refine now correlates against the acquisition engine's own
    reference row rather than a second expansion of the chips. A code's
    capture is unchanged, and a code handed over as samples captures the
    same window at the same start. The Python face follows.
