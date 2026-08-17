- **`MpskReceiverR` is now a VIEW over `MpskReceiver`, not a second type — one
    object, three faces.** The Python surface is unchanged for every existing
    caller: same class name, same constructor signature and defaults, same
    `steps()`/`bits()` taking `float32`, same properties. What it GAINS is the
    four §8.1 read-backs the separate type had in C and never bound (`zeta`,
    `num_phases`, `lock_thresh`, `bn_agc_ratio`), which are shared from the
    parent verbatim. In C, `mpsk_receiver_r_*` is gone: the constructor is
    `mpsk_receiver_create_real()` and the block API is
    `mpsk_receiver_steps_real()` / `mpsk_receiver_bits_real()`, over one
    `mpsk_receiver_state_t` carrying `union { ddc_state_t *c; ddcr_state_t *r; }   fe` and an `int real`. `native/{inc,src}/mpsk_receiver_r/` and
    `objects/mpsk_receiver_r.toml` are deleted.

    The argument was never the duplication. `mpsk_receiver_r_core.c` was 372
    lines of which 16 functions were pure delegations, but the cost of the
    split was that their shared 784-line `mpsk_rx_loops.h` **had no test
    home** — so its claims were pinned only where one of the two tests
    happened to reach them, and the two did not overlap. `set_telemetry` was
    asserted seven times on the complex side and zero on the real one; **"the
    LO runs at half the input rate" was pinned by neither**, which is exactly
    where the gh-765 `freq_scale` defect lived. `test_mpsk_receiver_r_core.c`
    is folded into `test_mpsk_receiver_core.c` (§15-23) rather than deleted,
    and every row of that table now has one owner.

    §23 is the claim nothing asserted, in two halves, each proven by sabotage:
    the loop GAIN against `θ_ss = 2πr/wn²` under a **ramp** on both faces
    (`lo_sps = sps` on the real face reads 2.00× the law at both ramp rates),
    and the frequency READBACK against a known offset (dropping the 0.5 in
    `mpsk_rx_lo_to_input()` is off by exactly `df`, five times the tolerance).
    Each sabotage leaves the other half green. The stimulus for the first has
    to be a ramp: a type-2 loop nulls a frequency step regardless of gain,
    which is how gh-765 survived every test in the tree. The estimator is the
    **signed mean** of the discriminator output, not the mean of `|e|` — under
    a ramp the lag is a constant the loop holds, so the signed mean averages
    the jitter out while `mean|e|` carries a bias that read 44% high and failed
    a correct receiver.

    The state blob is unchanged on both faces and neither version moved, but
    the envelope MAGIC is keyed on the face (`MPSK`/`MPSR`) so a blob from one
    is refused by the other by name rather than reinterpreted. `MpskReceiverR`
    keeps its declared defaults verbatim, including the five it pins where the
    parent derives — three of which disagree with the derivation. Fixing that
    is a behaviour change and is gh-829; the collapse changed no behaviour, and
    `make validate-check` reports the report unchanged.

    Made possible by just-makeit#1012 (jm 0.62.0): a view method restating a
    parent's Python name may declare its own signature when it binds its own C
    symbol via `fn`. The type/flavor rule — a difference in constructor is a
    flavor, a difference in method signature is a separate type — is unchanged;
    what changed is that jm can now express the answer.
