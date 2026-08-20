- **`doppler.coding.ReedSolomon` — a caller can name their own block code, in
    both directions.** `rs_encode`, `rs_syndromes` and `rs_codeword_ok` were
    reachable from Python only through `FrameDesc`'s Reed-Solomon stage, which
    binds to `ccsds_tm_frame_ops` and carries an interleaving depth rather than
    a code — so Python could run exactly **one** Reed-Solomon code, CCSDS's,
    and only inside a frame. This is the third and last of the family slices
    [gh-900](https://github.com/doppler-dsp/doppler/issues/900) named, after
    `ConvEncoder` and `SyncFinder`, and it takes the same shape: `rs` owns the
    code, the object owns the binding of a code to its tables, and every method
    calls the matching kernel rather than reimplementing it.

    `encode` answers in whole systematic codewords because that is the unit
    every other method takes, and it may encode **in place** — the call a frame
    assembler makes, and the reason `rs_encode` stays exposed as the
    parity-only primitive. `decode` corrects the caller's own array and returns
    the count, with **-1** for "too far from every codeword to name one" and
    **-2** for a word that was not `n` symbols long, because those are
    different kinds of fact: one is the channel's answer and one is the
    caller's mistake.

    **Two of the five constructor arguments are validated rather than
    trusted**, and it has to be there or nowhere: a non-primitive `field_poly`
    and a `root_stride` sharing a factor with `n` each produce arithmetic that
    encodes and decodes against itself perfectly, so no round trip can ever
    catch them. `generator()` is exposed for the matching reason — standards
    publish those coefficients, so it is how a caller checks that they read the
    five numbers correctly against the *document* rather than against this
    implementation.

    Sixteen mutations of the object were each confirmed to turn the C suite
    red, and two of them found real gaps first: nothing had pinned that
    `first_root` reaches the arithmetic at all (a codec that silently used the
    textbook root set would produce a perfectly good RS(255,223) that no CCSDS
    receiver can decode), and nothing had pinned the `*_max_out` bounds — an
    off-by-one in `generator_max_out` is a heap overflow, because the binding
    allocates from it. Both now have sections of their own.

    The Python face is checked against `math.comb` arithmetic written out from
    the definition, not against the kernel: an oracle that shares its
    arithmetic with its subject proves only self-consistency, which every wrong
    Reed-Solomon also has.

- **A runnable example and a gallery page for the whole `doppler.coding`
    family.** `src/doppler/examples/coding_demo.py` runs an outer code, a
    marker, an inner code, a real AWGN channel, acquisition and both decoders,
    on a configuration that is nobody's standard — and it closes a gap the
    previous three slices each left: `ConvEncoder`, `Viterbi` and `SyncFinder`
    all shipped with no example exercising them, which
    [the lifecycle page](https://github.com/doppler-dsp/doppler/blob/main/docs/dev/contributing/adding-algorithms.md)
    owes unconditionally.

    It also measures something the tree stated and never showed: **past its
    radius a bounded-distance decoder can be silently wrong**, returning a
    positive count for a word that passes `codeword_ok` because it *is* a
    codeword — just not the one that was sent. The sweep runs on a deliberately
    weak RS(15,11), because the miscorrection probability is about
    `sum(C(n,i)(q-1)**i for i<=E) / q**(n-k)` — 2e-05 for RS(255,239) and 0.36
    for RS(15,11) — and a sweep of the strong code would have been reporting
    its own sample size. The measured rate converges onto that closed form,
    which the example asserts.
