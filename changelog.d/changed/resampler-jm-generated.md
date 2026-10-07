- **`Resampler` is jm-generated** (doppler#1446). Its binding is re-rendered by
    jm and leaves the `-Wall -Wextra` exempt list. The custom `bank=` constructor
    is declared in the manifest, and `execute_ctrl` raises `ValueError` for a
    `ctrl` shorter than `x` from the C core's refusal rather than a check in the
    binding. The output buffer is now per call, as for the other adopted
    objects.
