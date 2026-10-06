- **Twenty-three more binding fragments are jm's, and five `track` objects
    gain the guards the hand-owned copies never received** (#1446). The
    `steps` wrappers of the `cvt` converters, `agc`, `boxcar`, `lockdet`,
    `loop_filter` and the `track` objects are now re-rendered by jm, and they
    leave the `-Wall -Wextra` exempt list. `Costas`, `CarrierMpsk`,
    `CarrierNda`, `RateSync` and `SymbolSync` now refuse a C core that wrote
    past the output buffer, an output too large for numpy, and a failed
    `PyArray_SetBaseObject`, each with an exception rather than silently.
