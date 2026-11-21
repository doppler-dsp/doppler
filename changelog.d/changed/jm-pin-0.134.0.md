- **just-makeit pin 0.133.1 → 0.134.0.** `default_raw` is refused at load on
    a param table that does not read it (only an object's or a view's
    `init_params` do), and a composer source or segment field may declare
    `get_fn`, a Python read that goes through a C function of the struct and
    the rate while the stored value, `to_json` and `from_json` are unchanged
    ([just-makeit#2709](https://github.com/just-buildit/just-makeit/issues/2709),
    [just-makeit#2710](https://github.com/just-buildit/just-makeit/issues/2710)).
    The re-render changes no file.
