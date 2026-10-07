- **`Farrow` is jm-generated** (doppler#1446). Its `delay` binding is
    re-rendered by jm and leaves the `-Wall -Wextra` exempt list; the GIL is
    still released across the kernel (`nogil`). `delay` returns a fresh array
    per call instead of one the object owns, so a result survives the next
    call. The stale `manual_stub` comment, which said `delay_max_out` was not
    generated (it has been since jm 0.99.0), is gone from the manifest.
