- **The stream tests' work-queue cleanup is idempotent** — the teardown added
    with the queue-residue fix deleted unconditionally, and a broker answers
    `Not Found` for a stream already gone, so a *passing* test became a CI
    ERROR against whichever test used the fixture last: **1 error beside 3369
    passes**, on one interpreter of six, green on re-run ([#1147][gh1147]).
    Cleanup now tolerates the already-absent case only — a stream that exists
    and cannot be deleted still fails the run, so the 40 GB regression
    [#1136][gh1136] gated for would still be caught.

[gh1136]: https://github.com/doppler-dsp/doppler/issues/1136
[gh1147]: https://github.com/doppler-dsp/doppler/issues/1147
