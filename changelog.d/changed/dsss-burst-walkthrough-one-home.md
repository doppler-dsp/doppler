- **The DSSS burst walkthrough is one page, sent from a description.**
    `dsss_burst_receiver_demo.py` now transmits its train from a
    `FrameDesc` on all three wfmgen faces (`--frame`, a scene's `"frame"`,
    `Segment(frame=)`), asserts them byte-identical, and checks the decoded
    frames with `FrameDesc.check`/`deframe` on the same object. Its codes are
    Field text. The duplicate `dsss-burst-pipeline` page, script, figure and
    test are removed (#1682).
