- **`wfm.Reader` is jm-generated** (doppler#1446). Its binding is
    re-rendered by jm and leaves the `-Wall -Wextra` exempt list. The two things
    that kept it hand-owned are now declared: the `sample_type` hint is a
    project `[[enum]]` (`reader_stype`) whose `enumerators` bind `"auto"` to
    `WFM_READER_STYPE_AUTO` (-1) beside the ten wire types, and Ctrl-C ending a
    `read_follow()` is a C creator, `dp_wfm_reader_create_interruptible`, set as
    the object's `create_fn`. `dp_wfm_reader_create` is unchanged and still
    installs no stop predicate. `WFM_READER_STYPE_AUTO` is now an enumerator of
    `wfm_reader_stype_t`, alongside `WFM_READER_STYPE_CF32` … `_I8`. The `.pyi`
    `sample_type` annotation now lists all eleven names it always accepted.
