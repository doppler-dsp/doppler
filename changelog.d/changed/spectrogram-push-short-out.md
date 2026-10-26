- **`Spectrogram.push` takes a short `out=`.** A caller bounds memory with a
    buffer of any `(k, nfft)` shape: push writes the whole rows that fit, and
    `consumed` says where to resume (adopts jm 0.111.2's `short_out`, #2592).
