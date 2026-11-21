- **`Ofdm.add_run`'s `cyclic_prefix` default names its constant.** The
    signature reads `cyclic_prefix: int = ...` and the docstring
    `default OFDM_CP_DEFAULT` instead of the number 4294967295; the behaviour
    is unchanged (an omitted prefix keeps the class's).
