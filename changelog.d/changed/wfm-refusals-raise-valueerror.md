- **`field_bits()` and `Composer.from_file` refusals raise `ValueError`
    with the reason.** Malformed Field text raised
    `RuntimeError: dp_field_bits failed (returned 0)`; it now raises
    `ValueError` naming the fault (`POLY`, the Field bound, ...). A scene
    file the reader refuses raises `ValueError` rather than `OSError`; a
    file that cannot be read is still `OSError`.
