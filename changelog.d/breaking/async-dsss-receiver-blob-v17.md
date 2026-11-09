- **`AsyncDsssReceiver` state blobs are v17 and `AsyncDsssPool`'s v7.**
    A receiver's blob now carries its refine hand-over ring after the
    carry, sized by the configuration, so a v16 receiver blob (or a v6
    pool blob) is refused
    ([#2974](https://github.com/doppler-dsp/doppler/issues/2974)).
