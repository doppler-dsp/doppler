- **The two-process streaming examples are now RUN, in pairs, instead of
    skipped.** `transmitter`/`receiver`, `replier`/`requester` and
    `pipeline_send`/`pipeline_recv` sat in `.examples-skip` as "needs a live
    peer" — true of a harness that runs one script at a time, and of nothing
    else: the example gate already runs every script as a subprocess, so
    running two is a second `Popen`. The excuse was costing coverage on
    exactly the demos a reader is most likely to copy. They move to a new
    `.examples-pairs` registry (`first.py + second.py: <evidence regex>`),
    and `test_example_pair_runs` starts both against a live broker, requires
    the pair's combined output to match the evidence pattern — a pair that
    exits 0 having exchanged nothing must not pass — and requires exit 0 from
    each half. The example gate goes from 76 passed / 7 skipped to 80 passed
    / 1 skipped; the one remaining skip is the draw-dependent Monte Carlo.

    Running them found a docs defect immediately: both pipeline docstrings
    said to start the workers first, and against a broker that has never
    carried the work-queue stream that fails deterministically with
    `dp_pull_create failed` — the Push side is what creates the stream. Once
    any sender has created it either order works, which is how the wrong
    instruction survived. Corrected, with the reason recorded (see #956).
