- **The `m_out` rule has one implementation again.** gh-644 gave it a home in
    `mpsk_rx_derive_m_out()`; `dsss_receiver_core.c` was never migrated, so
    the tree carried two implementations of one rule — the thing CLAUDE.md's
    *never reimplement existing logic* exists to prevent, and which the
    retired copy's own comment was a monument to having gone wrong once
    already.

    They disagreed below `sps = 2`: the shared rule refuses with `0`, the
    local one floored at `2`. Neither value builds a receiver at `sps = 1`,
    so this moves *which* rule refuses rather than whether one does —
    recorded because a silent value difference between two copies of a single
    rule is how the first drift happened.
