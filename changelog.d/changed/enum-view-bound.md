- **A view's window set is declared once.** carrier_acq's C refusal carries
    `SSOT: view=carrier_acq_window` and must name the view's last window, so
    the C and Python sets cannot drift; named-args checks its slot against
    the view, and sees a call with a comment before its `(` (#2194, #2198).
