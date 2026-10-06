- **`dp_pn_create_mls()`: the `poly = 0` default, as a constructor**
    (doppler#1446). `dp_pn_create()` takes `poly` verbatim, so a zero is a
    register with no feedback, and each caller that lets a user say "default"
    resolved it itself. `dp_pn_create_mls()` is that resolution in front of it
    (`poly` if non-zero, else the maximal-length polynomial for `length`) and
    is what `PN` now calls, so the rule lives in C once instead of in a
    hand-patch to the binding. `dp_pn_create()` is unchanged and still pinned.
    `PN`'s behaviour is unchanged, including a width-1 register being built;
    its binding is now jm-generated (see `native/inc/doppler/pn/pn_core.h`).
