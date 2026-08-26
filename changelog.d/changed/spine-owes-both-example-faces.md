- **The lifecycle spine owes a runnable example in C *and* Python, not
    either** (`docs/dev/contributing/adding-algorithms.md`). One is not
    enough because the two faces fail differently: the C example is where a
    caller sees the lifecycle it must actually manage — create, feed, drain,
    destroy, and that the output buffer is the caller's — none of which the
    binding exposes because it does that work for you; the Python example is
    where the result is legible. The same page's claim that C examples are
    *registered* while Python ones are *discovered* went too: both are
    globbed since gh-863, and opting either out costs an entry with a
    mandatory reason.
