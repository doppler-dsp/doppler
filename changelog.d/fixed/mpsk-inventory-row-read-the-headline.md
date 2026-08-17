- **A claim-inventory row was read from a test's headline instead of its
    assertions, and understated the coverage.** The `MpskReceiver` report's §1.1
    recorded C6 — the two-way handover — as *"the flip is pinned; the DROP-BACK
    is absent"*. `test_mpsk_receiver_core.c` §4 in fact pins the flip, the
    drop-back **and** the re-declare; the row was written from the section's
    comment rather than from what it asserts.

    What §4 genuinely cannot test is the part the header argues — that the loop
    filter *carries the frequency estimate across* rather than re-acquiring —
    because §4 re-seeds the carrier by hand over the outage, so it would pass
    against a receiver that cleared it. That is what the new §12 covers, and the
    row now says so.

    Recorded because it is the campaign's own trap arriving from the other
    direction: **pinned-only-at-literals**, committed by the auditor rather than
    the author. An inventory is evidence about tests, so it has to be read the
    way the tests are — assertion by assertion.
