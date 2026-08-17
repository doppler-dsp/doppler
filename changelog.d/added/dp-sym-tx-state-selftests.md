- **`dp_sym_test.h`, `dp_tx_test.h` and `dp_state_test.h` have self-tests, and
    the state macro gains the FIDELITY half it was missing.**

    **`DP_STATE_ROUNDTRIP_TEST` is 12 lines and 31 test files call it** — the
    only evidence most serializable objects have that their state interface
    works. It asserted what `set_state` **returns** (`DP_OK` on a good blob,
    `DP_ERR_INVALID` on a clobbered one) and never that the restored object
    **carries** the state, so a `set_state` that validated the envelope,
    returned `DP_OK` and restored nothing passed at all 31 sites. The
    project's claim is bit-exact resume; every object meeting it did so in its
    own test, by hand, while the shared macro a new object reaches for first
    proved only the envelope.

    The generic fix is three lines and needs no knowledge of the object:
    restore into `b`, re-serialize `b`, compare to `a`'s blob. The state
    standard is what makes it well defined — a blob carries only RUNNING
    fields, config is restored by `create()`, and `b` must be a fresh object
    of the same config. **No object was cheating**: everything passes with it
    in, and all 32 call sites pass distinct `a`/`b`, so it is exercised rather
    than trivially satisfied.

    Finding it needed a fake object, because the macro pastes a PREFIX and so
    cannot be exercised against a real one without also testing that object.
    `test_dp_state.c` builds one over the real `dp_state.h` envelope whose
    `set_state` switches between correct and three broken implementations.

    **`dp_sym_test.h`** is thin, so what is load-bearing is the numbers its
    docstrings state — other files write fixed thresholds against them. Those
    are pinned at the source, and the half a closed form cannot establish is
    measured: a stream at uniformly random phase lands on the scatter floor
    and **passes** the `< -12.0 dB` assertion the header records as live in
    `test_mpsk_receiver_r_core.c` until 2026-07-27, while the identical stream
    fails it at BPSK. It also found the short-stream floor is **39, not the 20
    the guard names** — both back-half forms score `ceil(n_syms/2)`, which the
    layer beneath rejects below 20. Docstrings corrected.

    **`dp_tx_test.h`** is the file `check_stimulus_sources.py` structurally
    cannot police (§5.4): it *is* the test layer's stimulus, so the gate has
    nowhere to point. The conventions it would have checked are asserted
    instead. `DP_TX_RC` makes the central one exact — a full raised cosine is
    Nyquist, so at symbol centres the sample **is** `amp * symbol`, and "amp
    is the SYMBOL amplitude, never a peak" stops being a comment. Proven by
    sabotage: reading `tau` as samples, and dropping the `rate` scaling from
    the lead-in, each take exactly the one assertion written for them red.
