- **`ofdm`: `Ofdm.modulate()` raises `ValueError`, naming the cause, for a
    refused call, where it returned an empty array.** Code that tested
    `modulate(...).size == 0` for a refusal must catch it; a render of nothing
    still returns an empty array. C gains `dp_ofdm_modulate_why`. See the [OFDM
    guide](docs/guide/ofdm.md).
