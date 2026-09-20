- **A hand-owned `.pyi` can no longer lag its extension silently.** A
    registration-free gate compares every class's runtime members to its
    stub (122 classes); `buffer.pyi` had been missed twice. One existing gap,
    `Push.send_eos`, is ratcheted
    ([#1431](https://github.com/doppler-dsp/doppler/issues/1431)).
