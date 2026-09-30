- **`dp_hash64`: FNV-1a 64, the one content hash**
    ([#1619](https://github.com/doppler-dsp/doppler/issues/1619)). It is
    header-only and incremental, so a file hashed read by read gives the
    hash of the whole. It is how a record will identify a
    `--data-from-file` source. Pinned against the published FNV vectors.
