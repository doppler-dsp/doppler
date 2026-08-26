- **A runnable example for `DsssBurstReceiver`**
    (`src/doppler/examples/dsss_burst_receiver_demo.py`) — the last phase-9
    deliverable the object was missing. Where
    `dsss_burst_pipeline_demo.py` drives the three stages separately, this
    is the same job through the composed object, asserting rather than
    claiming: identical decodes at every block size from 99k down to 333
    samples (32x shorter than one burst, `dropped == 0`); a burst split
    across two `push()` calls held — `pending` says so — and returned
    whole; and bursts packed inside `refine_span` coalescing, which is what
    makes that span a minimum rather than a suggestion.
