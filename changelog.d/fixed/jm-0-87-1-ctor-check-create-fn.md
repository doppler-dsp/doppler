- **just-makeit pin 0.87.0 → 0.87.1: the drift gate's constructor check
    reads the function an object actually binds** (jm#1498). It compared
    `<obj>_create` even when `create_fn` named another constructor, so an
    object keeping a public `<obj>_create` of a different shape beside its
    bound one reported CTOR drift that was not drift. `burst_acq` is that
    shape. No generated file changes on doppler's tree.
