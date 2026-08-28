- **`DsssBurstReceiver`** (new, C) — the burst chain composed in one object,
    the way `DsssReceiver` already composes the continuous one. Samples in,
    one burst's payload bits out with a CRC verdict and an event describing
    it, through **search → refine → demod** behind one `push()`. Every part
    was already certified and nothing composed them, so the hand-off was
    arithmetic each caller redid in Python. The stage that earns its keep is
    **refine**: acquisition reports an *end* anchor and a code phase modulo
    one period, and neither is a burst start. Certified at 29 limits / 10
    findings / 0 open ([#1001](https://github.com/doppler-dsp/doppler/issues/1001));
    design in `docs/design/dsss-burst-receiver.md`.
