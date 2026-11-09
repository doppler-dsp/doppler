- **`DsssBurstReceiver` no longer overruns its preamble buffer on a huge
    `spc`.** The preamble's byte size, `acq_code_len * spc` samples, wrapped
    to a short allocation that construction then wrote past; an `spc` whose
    preamble cannot be held is now refused, with a `why` naming it.
