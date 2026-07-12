# Examples

The runnable scripts under
[`src/doppler/examples/`](https://github.com/doppler-dsp/doppler/tree/main/src/doppler/examples)
that don't get a rendered walkthrough in the [Gallery](../gallery/index.md) —
raw building blocks, C programs, and the streaming demo.

- [LO](python-lo.md) — complex phasor generator with FM control and phase continuity.
- [NCO](python-nco.md) — raw uint32 phase accumulator with overflow carry and fixed-point scaling.
- [FFT](python-fft.md) — per-instance 1-D and 2-D FFT with plan reuse.
- [Ring Buffers](python-buffers.md) — double-mapped lock-free ring buffers for producer/consumer pipelines.
- [C examples](c.md) — minimal C99 programs calling the library directly.
- [Streaming](streaming.md) — NATS-based signal streaming end-to-end.
