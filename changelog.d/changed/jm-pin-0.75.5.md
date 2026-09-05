- **just-makeit pin 0.75.4 → 0.75.5.** Ships doppler-driven
    [jm#1277](https://github.com/just-buildit/just-makeit/issues/1277): a view
    may now use `<obj>_create` when the parent's `create_fn` points elsewhere.
    The check compared against the default-derived name while its message
    claimed to name the parent's, which blocked making a general constructor
    the base and the specialised one a view.
