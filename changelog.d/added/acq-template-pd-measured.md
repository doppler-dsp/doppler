- **A template burst engine's `pd_predicted` is measured, not only
    modelled.** `native/validation/acq_template_pd.c` runs acq report §2.6's
    Monte-Carlo on four kinds of preamble at three design points each, with
    the code as a control that reproduces §2.6 (0.729 against 0.740 ± 0.025).
    QPSK sits on the model; Zadoff-Chu and chirp are conservative (a chirp's
    peak slides along its ridge instead of shrinking). A shaped template is
    optimistic by 0.02–0.03 at 3σ
    ([#1483](https://github.com/doppler-dsp/doppler/issues/1483)).
    [`dsss-acquisition.md` §3.1](docs/design/dsss-acquisition.md) records the
    design and these numbers.
