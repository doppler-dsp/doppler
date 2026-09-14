- **`ring_chunking_demo.py`** shows what `doppler.buffer` is actually for —
    decoupling the producer's block size from the consumer's, in both
    directions: irregular multi-thousand-sample blocks re-blocked into exact
    1024-sample FFT frames, and a drip of 7–100-sample writes batched into
    2048-sample drains. Both run over many wraps, which is the case the
    double-mapping exists for. The ring guide gains the same two patterns and
    the three things that bite — `write` is all-or-nothing and never blocks,
    `dropped` counts rejected *calls* rather than lost samples, and `wait(n)`
    above `capacity` hangs
    ([#1335](https://github.com/doppler-dsp/doppler/issues/1335)).
