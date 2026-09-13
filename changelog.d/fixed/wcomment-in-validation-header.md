- **`-Wcomment` on every build of the validation harnesses.**
    `dp_rx_mpsk.h`'s prose contained `native/validation/*.c`, and the `/*` in
    that glob opens a nested comment as far as the compiler is concerned. The
    sibling instance in `wfm_compose_ext.c` is jm-generated and is fixed
    upstream instead — `jm apply` reverts an edit there.
