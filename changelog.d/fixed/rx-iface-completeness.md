- **An incomplete receiver adapter now fails by name instead of segfaulting.**
    Every `dp_rx_iface_t` entry is mandatory — the instrument calls all twelve
    unconditionally — so a positional initializer that stops one short is a
    NULL call at the first operating point, before a single line is printed.
    `dp_rx_run()` now checks the adapter first and fails the record with
    `adapter entry '<name>' is NULL`, which `--check` turns red.

    It is a *failure*, not a refusal: a refusal is the instrument declining a
    number it cannot defend, and this is the instrument unable to run at all.

    Found the way the class is designed to be found — `ContinuousMpskReceiver`'s
    adapter was written against an eleven-entry interface and `zeta` was
    appended as the twelfth on another branch, so **both branches were green**
    and only their rebase was not. The guard is proven by sabotage: dropping
    `rx_mpsk_zeta` from `RX_CONT` reddens `validate_rx_battery --check` with the
    entry named, where before it exited 139 with no output.
