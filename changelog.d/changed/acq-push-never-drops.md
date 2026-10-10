- **A full result buffer stops `dp_acq_push()` where the input can
    resume** (`dp_acq_consumed()`, and `dp_burst_acq_consumed()` for
    `BurstAcquisition`): it takes nothing more, keeping only a rest that
    completes no frame, counting the carry, as the carry. A frame that ends
    a dwell is taken only while its whole list fits; under `max_peaks` the
    list is cut to its strongest picks. `dp_acq_run()` stops and resumes the
    same way. Python's `push()` has room for 1024 results (was 64). State
    blob v5 (#1895, #1992).
