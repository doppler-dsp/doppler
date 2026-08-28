- **`wfm.Frame.deframe()` — the receive counterpart of building a frame.**
    Undo a description's stages in place order (randomiser XORed back, outer
    code repairs APPLIED, CRC checked) and hand back the corrected bits, with
    the verdict in `rx_ok` / `rx_units` / `rx_checked` / `rx_symbols`. The
    payload is then a slice at `field_off()`, because a description does not
    privilege one field over another. `rx_checked == 0` says the frame carries
    no reversible stage — a different fact from a check that failed.
