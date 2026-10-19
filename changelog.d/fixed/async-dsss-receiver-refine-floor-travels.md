- **An `AsyncDsssReceiver`'s refine floor survives a restore.**
    `set_refine_min_blocks()`'s value was not in the state blob, so a resumed
    receiver ran the default floor; it travels now (blob v9). The receiver
    and its cell flavor join the shared state round-trip matrix, which puts
    their setters under the mutator-state gate and retires 14 exemptions.
