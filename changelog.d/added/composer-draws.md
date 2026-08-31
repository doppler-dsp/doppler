- **`draws(scene)` — the ground truth of a ranged scene** (#1112). A ranged
    field is re-picked per instance, so the scene declares a span while each
    burst flies one value out of it; scoring a receiver needs *which* value,
    and the span cannot say. The drawn rows were reachable from C and from a
    capture's SigMF metadata but not from Python, so an in-process
    `compose()` could only recover its own truth by emitting metadata as a
    side effect. `draws()` returns one record per source per instance —
    placement plus the drawn `freq`/`f_end`/`snr`/`level`/`doppler` — read
    through the same `wfm_compose_draws()` the metadata uses, and a test
    pins the two to each other.
