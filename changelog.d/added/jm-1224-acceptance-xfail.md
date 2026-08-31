- **The `Segment(frame=…)` sugar now has an acceptance test that fires by
    itself.** It waits on
    [just-makeit#1224](https://github.com/just-buildit/just-makeit/issues/1224)
    (an `init_param` cannot take another generated object), so it is a strict
    xfail: when jm ships the feature and the manifest declares the field, it
    XPASSes and reddens CI until the marker goes. The bar it asserts is
    byte-identity with the JSON route, not merely that the call succeeds.
