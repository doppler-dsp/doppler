- **A mutator's value is state, and a gate holds every serializable object to
    it (#2022).** A value a setter, writable property or
    `configure`/`retune`/`reseed` can change must travel in the state blob.
    Config that sizes the blob is a reject key. `test_mutator_state.py`
    finds every public member of each class Python gets, and every C setter
    it doesn't bind, and probes each by restoring into other-valued targets.
    The 119 that don't yet pass are
    listed by exact verdict in `scripts/.mutator-state-exempt`, which only
    shrinks. The rule is in `docs/design/state-serialization.md`; the
    known violations are filed as #2079–#2084 and #2087.
