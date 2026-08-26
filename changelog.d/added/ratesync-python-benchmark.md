- **`RateSync` now has a Python benchmark that measures something**
    ([#1010](https://github.com/doppler-dsp/doppler/issues/1010)). It was a jm
    scaffold holding a fixture nothing called, so the timing loop every M-PSK
    receiver runs appeared in no Python snapshot row. Two rows over one 64k
    RRC-BPSK block, one per TED, each asserting its loop is still locked and
    its eye still open — every way this measurement breaks makes it look
    faster, and at 2 dB Es/N0 the count and the lock flag both still pass
    while the settled EVM does not.
