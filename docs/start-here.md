# Start Here

A one-line map from "what you're trying to do" to the doc family that
answers it.

| You want to...                     | Go to                                                                                                           |
| ---------------------------------- | --------------------------------------------------------------------------------------------------------------- |
| Call a specific function or class  | [API Reference](api/index.md) — Python surface, one page per module                                             |
| See it running, with a plot        | [Gallery](gallery/index.md) — dozens of worked examples grouped by DSP domain                                   |
| Accomplish a task end-to-end       | [Guides](guide/index.md) — task-oriented walkthroughs (lock detection, DSSS acquisition, PSD, real-time pacing) |
| Understand why it's built this way | [Design](design/index.md) — architecture decisions and algorithm rationale                                      |
| Add a module or contribute         | [Contributing](dev/index.md) — repo layout, module conventions, release process                                 |
| Look up a raw C signature          | [C API](c-api/index.md) — machine-generated from the C headers                                                  |
| Compose and stream a waveform      | [Waveform Generator](guide/wfmgen/index.md) — wfmgen's own top-level nav section                                |

Not sure which page has the piece you need? Every family answers a
different question about the *same* underlying object — for example,
`SymbolSync` has a [Python API page](api/python-track.md), a
[gallery walkthrough](gallery/symsync.md), a
[design doc](design/timing_lock_detector.md) for its lock detector, and a
[guide](guide/lock-detection.md) for wiring it into a receiver. Start
wherever matches your question above; the pages already link out to each
other from there.
