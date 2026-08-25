- **`BurstAcquisition` is certified as what it is — a forwarder** —
    `src/doppler/dsss/tests/validation/burst_acq/results.md`, 17 limits on
    every push.

    **The report deliberately does not measure detection performance.**
    `burst_acq_core.c` forwards every call into `acq_core.c`'s shared
    engine, so re-running the statistics through the wrapper would certify
    the same code twice and present the second run as independent evidence.
    The physics is certified in `acq`'s report — which measures through this
    very front door.

    **What a forwarder can get wrong is delivery, and that is now
    certified.** The C test built one object and detected with it, which
    proves nothing about whether each argument reached its own destination:
    a wrapper that transposed two same-typed parameters would still
    construct and still detect a strong signal. The characteristic defect is
    invisible to the type system — `pfa` and `pd` are both doubles in (0,1),
    `reps` and `spc` are both `size_t`, `noise_mode` and `nthreads` are both
    `int`.

    So each of the seven constructor arguments is now varied alone, and two
    things are required: every argument must move **something** (it was not
    dropped or defaulted), and no two may move the **same set** of derived
    quantities (a transposition would be invisible if they did). Both hold.
    `pfa` and `pd` separate only because `pfa` moves the threshold and the
    per-cell rate while `pd` moves only the look count.

    Backed by six documented identities across three geometries — `sf` is
    the code length, `fs` is `chip_rate · spc`, `code_bins` is `sf · spc`,
    the native span is `chip_rate/(2·sf)`, the resolution is
    `chip_rate/(sf · doppler_bins)`, and the coherent depth stays within
    `[1, reps]`. An argument that landed in the wrong slot breaks several at
    once.

    **The under-powered warning was declared and never tested.** It is a
    manifest-driven post-construction diagnostic gated on the engine's own
    `underpowered` field, and a diagnostic nobody exercises is
    indistinguishable from one that was removed. Now checked in **both**
    directions: it fires on a configuration that cannot meet the requested
    `pd`, and stays silent on one that can — a warning that always fires is
    as useless as one that never does, and only the pair separates them.

    Every forwarded method is checked by evidence that the engine *acted*
    rather than that the call returned: a burst rolled by 17 samples
    reporting code phase 17, a half-frame residue really dropped, a
    mid-stream blob really resuming.
