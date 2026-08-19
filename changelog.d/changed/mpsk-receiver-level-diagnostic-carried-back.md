- **The certification's level-diagnostic finding is now gated by `ctest` and by
    the examples suite, not only stated in a report.** `docs/dev/contributing/validation.md`
    step 9 — whatever a certification establishes goes back into the C test and
    into an example, because those are what keep it true and what put it in front
    of someone.

    **The C test's assertion is strictly stronger.** §11 checked that the AGC's
    gain *moved* by more than 10 dB per 4× level step. It now checks the **law**:
    each step moves it by `20·log10(4) = 12.0412 dB`, measured 12.0412 and
    12.0412. That is the difference between a trend and an absolute level
    estimate. Sabotage shows the coverage this adds — a 2% scale error on
    `agc_gain_db` passes the old `> 10 dB` check and fails the new one.

    **The example carries the blind spot**, because that is what a caller gets
    wrong: `lock` reads 0.936 / 0.948 / 0.950 across a 16× level change and
    **cannot see a level error at all** — it is the M-th-power carrier statistic
    and `carrier_nda_disc` divides out its own `|z|^M`. `mpsk_receiver_demo.py`
    now asserts both halves (the gain law, and that `lock` stays put), so a change
    making `lock` level-sensitive fails the examples gate instead of surprising
    someone in the field. The gallery page includes that region under
    "Diagnosing a level problem — read `agc_gain_db`, never `lock`".

    Only the robust half was carried. The report's §2.9 also shows the `A²`
    timing under-drive in `timing_rate`, and that one is **not** monotone in
    level — at 25 dB and amplitude 0.25 the un-levelled receiver reads better —
    so it is reported and deliberately not asserted anywhere.
