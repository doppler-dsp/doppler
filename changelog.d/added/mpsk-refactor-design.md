- **`docs/design/mpsk-refactor.md` — the design for collapsing `MpskReceiver`
    and `MpskReceiverR` into one object with three faces.** Planned, not built.
    The argument is a measurement: the two differ only in a front-end pointer
    and one rate convention, and `mpsk_receiver_r_core.c` is 372 lines of which
    16 functions are pure delegations — but the cost of the split is not the
    duplication, it is that their shared 784-line `mpsk_rx_loops.h` **has no
    test home**, so its claims are pinned only where one twin's tests happen to
    reach them. "The LO runs at half the input rate" is pinned by neither, and
    that is where the gh-765 `freq_scale` bug lived.
