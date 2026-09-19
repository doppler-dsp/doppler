- **`make coverage` no longer rebuilds every validation report under
    instrumentation.** The `test_validation_limits.py` tests (now marked
    `validation_limits`, from their file name) build each object's whole
    report; instrumented, one took 823.7 s of an 854.5 s pass. Coverage now
    deselects them, as its C leg drops the `sweep` validators, and the plain
    suite still runs every limit on every push.
