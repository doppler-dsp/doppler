- **`Reader.annotations` returns a capture's SigMF annotations.** It gives
    one dict per row with the SigMF keys as the file has them
    (`core:sample_start`, `core:label`, ...), so a capture written with
    `Writer.annotate()` reads back the rows `Composer.to_sigmf()` gives. A
    raw/CSV capture reads them from its sidecar. Empty when there are none
    (#2565).
