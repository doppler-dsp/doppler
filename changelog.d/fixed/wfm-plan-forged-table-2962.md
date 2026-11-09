- **`dp_wfm_plan_restore` no longer crashes on a forged signal table**
    (#2962). A blob that kept its DSP fingerprint but claimed more signals
    than could be allocated left the table NULL and still marked intact, so
    the build dereferenced NULL. A count or length the blob cannot hold now
    rebuilds the Plan from its embedded spec, and a spec length that wraps
    is refused.
