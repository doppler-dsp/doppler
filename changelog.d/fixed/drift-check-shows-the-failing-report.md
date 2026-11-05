- **`make drift-check` shows the report of the run that failed.** On a
    failing `jm status`, `scripts/check_jm_status.py` used to discard that
    run's JSON and print a second, separate `jm status --check`. When the
    tree changed between the two, the reader saw "OK — up to date" over a
    gate that had failed, and the failure's only evidence was gone. It now
    renders the failing run's own report (every file that differs, with
    its state, and every other finding) and never runs jm twice.
