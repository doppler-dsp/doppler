- **`BurstAcquisition` is certified as what it is — a forwarder** — 17 limits
    on every push. The report deliberately does **not** re-measure detection:
    that would certify `acq`'s engine twice and present the second run as
    independent evidence. What a forwarder can get wrong is delivery, so each
    of the seven constructor arguments is varied alone and must move
    something no other argument moves — the type system cannot see a
    transposition when `pfa` and `pd` are both doubles in (0,1). Evidence:
    `src/doppler/dsss/tests/validation/burst_acq/results.md`.
