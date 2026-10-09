- **`telemetry.Telemetry`, `MemoryCapture` and `Capture` are jm-generated**
    (doppler#1886). The two `read_dict()` methods stay hand-written, in
    each object's `_extra.c`, and are declared as `extra_methods`; their
    stubs and docstrings are unchanged. jm's render adds the guards the
    hand-kept bindings lacked: `read()` and `records()` refuse a kernel
    count past the buffer and an output size past `NPY_MAX_INTP`. And
    `Telemetry.stats()` now refuses arguments, as its stub always said; it
    silently ignored them. The three fragments leave the `-Wall -Wextra`
    exempt list.
