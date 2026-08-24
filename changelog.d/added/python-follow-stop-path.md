- **A follow read is stoppable from Python.** `wfm_reader_read_follow()` takes
    an injected stop predicate rather than calling `dp_interrupted()` itself,
    so that the core does not drag `dp_interrupt.c` onto the link line of
    every C consumer of `wfm_reader_core`. Nothing was injecting one on the
    Python side, so `Reader.read_follow()` had **no escape at all** — both
    budgets default to "forever" on purpose, which made "no stop predicate"
    mean "waits until the writer closes, whatever happens".

    The binding now installs `dp_interrupted` at construction, which is what
    `wfm_reader_core.h`'s own example says doppler passes. This is only true
    across modules because `dp_interrupt_guard` is `process_global`
    (doppler#976): before that fix the flag `Interrupt()` sets in
    `doppler.interrupt` was a different variable from the one `doppler.wfm`
    reads, and this line would have looked like it worked.

    A stop still does not, by itself, end the read, and that is the design:
    `follow_grace_ms` defaults to 0 = forever, so a stopped reader waits for
    the writer's marker indefinitely — the shutdown propagates *through* the
    file and the reader joins it rather than racing it. A caller that wants a
    bounded stop sets a grace.

    `test_wfm_reader_follow.py` is the first Python coverage this path has
    had: the stop, the drain that outranks it, the marker as the normal
    ending, and a bounded budget. Every blocking wait runs in a thread with a
    join deadline, so a regression fails the suite instead of wedging it —
    which is what it would have done before doppler#976.
