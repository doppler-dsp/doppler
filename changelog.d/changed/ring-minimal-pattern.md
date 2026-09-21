- **The ring's single-threaded examples teach the pattern, not the
    workaround.** `ring_chunking_demo` (part A), `wfm_stream_demo` and the
    ring docs polled `available` to make a blocking `wait()` safe and drained
    twice to make room for an all-or-nothing `write()` — idioms that predate
    `peek` / `write_some`. They are now the five-line loop: feed what fits,
    take every whole frame, repeat. It also handles a block larger than the
    ring, which the old shape could not.
