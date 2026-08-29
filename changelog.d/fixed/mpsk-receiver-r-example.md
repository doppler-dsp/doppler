- **`MpskReceiverR` has a runnable example of its own.** It was the one
    public class whose stub carried none — it had looked documented only by
    inheriting its parent's docstring, which just-makeit 0.70.1 correctly
    stopped a view doing. jm synthesises a "Create with defaults" doctest for
    a constructor with no example block and renders the block when there is
    one, so the C example on `mpsk_receiver_create_real()` was not an extra —
    it *replaced* the Python one. That C usage now sits in the header's file
    comment beside the parent's, where doxygen still shows it and jm never
    looks; docstring coverage goes 1670 → 1671 and the C API loses nothing.
