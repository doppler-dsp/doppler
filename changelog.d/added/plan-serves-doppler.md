- **`Plan` serves a Doppler source, bit-identical to `compose()`**
    (doppler#1109). Both lifetimes, a ranged `doppler`/`doppler_rate` (a seed
    redraws it), gap ring-out, a leading delay, and a bundled source's noise,
    which sits inside the channel. The cache holds the signal before the
    channel and `render()` runs the channel over it. A Doppler render is no
    longer a pure re-weight. A `background=True` source with Doppler is still
    refused.
