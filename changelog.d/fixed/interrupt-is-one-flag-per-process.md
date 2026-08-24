- **A stop requested in one extension module now reaches a wait in every
    other ([#976](https://github.com/doppler-dsp/doppler/issues/976)).**
    `dp_interrupt` is documented as *the one flag every blocking wait in
    doppler consults*. That was true in C, where everything links one
    archive, and false in Python: just-makeit links a component's core
    statically into each `.so` and CPython imports extensions `RTLD_LOCAL`,
    so `doppler.interrupt`, `doppler.buffer`, `doppler.stream`,
    `doppler.telemetry` and three `doppler.wfm` modules each held a
    **different** `volatile sig_atomic_t`. `Interrupt().interrupt()` set one
    of them; a ring wait in `doppler.buffer` read another and spun forever at
    100% CPU with no escape.

    Every test passed throughout, because the only Python setter and the only
    exercised wait happened to share a `.so`. doppler's own stream suite
    contained the proof and could not report it —
    `test_interrupt_latency_is_the_callers_to_set` blocked in `recv()`
    forever, so the run was killed rather than failed, and the assertion
    after that `recv()` had never once executed.

    Fixed with just-makeit's `process_global = true` (jm gh-1117), declared
    on `dp_interrupt_guard`: the owning module publishes a `PyCapsule` over
    the state and every other linking module adopts the pointer in its
    `PyInit_`. doppler writes the two accessors jm cannot
    (`native/src/dp_interrupt.c`) and, for the two `no_generate` modules jm
    emits no `PyInit_` for, the adopt call itself
    (`native/inc/dp_interrupt_pyadopt.h` — one definition, so the
    hand-written face cannot drift from the generated one).

    `dp_interrupt_state_t` and the `dp_interrupt_bind`/`dp_interrupt_state`
    pair leave the public header: the rendezvous hands the state across as an
    opaque `void *`, so nothing outside `dp_interrupt.c` needs its shape.

    Two gates, both sabotage-proven. `test_interrupt_is_process_wide.py`
    reproduces the issue end to end and, structurally, reads every built
    `.so` for the storage symbol and the capsule name — registration-free, so
    a NEW module that links the primitive and joins nothing fails the moment
    it exists. There is no waiver list; the previous five-module ratchet is
    gone rather than emptied. `test_dp_interrupt.c` pins at the C layer that
    adopting actually redirects the reads, so an adopt that silently did
    nothing could not pass as a successful import.
