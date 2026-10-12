- **carrier_acq's window set, stated in the manifest and again in C, is held
    to agree.** The C refusal carries `SSOT: view=carrier_acq_window` and
    must bound the view exactly; named-args checks carrier_acq's slot against
    the view, and sees a direct call with a comment before its `(` (#2194,
    #2198).
