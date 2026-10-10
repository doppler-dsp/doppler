- **`AWGN`'s amplitude travels in its state blob.** `set_amplitude` (and
    the `amplitude` property) can change it after `create`, but the blob
    carried only the RNG state, so a restored generator kept whatever
    amplitude it was built with. The blob is version 2 and carries it
    (#2084).
