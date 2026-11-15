- **`make lint` fails on a C identifier the Windows headers define away.**
    `<windows.h>` defines `near`, `far`, `IN`, `OUT`, `small` and a few
    other plain words as macros, so a variable with one of those names
    breaks only the Windows build. `make lint-windows-macro-names`
    (`scripts/check_windows_macro_names.py`) flags every use outside
    comments and strings; the five that predate it are a list that only
    shrinks.
