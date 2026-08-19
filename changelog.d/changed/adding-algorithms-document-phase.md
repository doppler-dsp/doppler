- **The algorithm lifecycle now names what a change owes a reader.** Phase 9 of
    [Adding an Algorithm](https://doppler-dsp.github.io/doppler/dev/adding-algorithms/)
    was one paragraph about carrying findings back; it is now five deliverables
    with the gate behind each — a `@code` example on every public function
    (executed by `make test-stubs`), a markdown guide when prose outgrows a
    docstring, C **and** Python benchmarks, a runnable example, and a gallery
    page when there is a figure worth seeing.

    Phase 3 also states the part that was implicit: a header's `@code` blocks
    **are tests**, flowed into the `.pyi` by jm and executed, so they must be
    pinned against a real run rather than reasoned about.
