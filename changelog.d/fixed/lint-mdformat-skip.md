- **`make lint` no longer skips mdformat because the tool failed to start.**
    The markdown formatter was skipped whenever `mdformat --version` failed, so
    a venv with a missing dependency made lint pass locally while CI's formatter
    rewrote the file. It now skips only on Python below 3.10, and a broken tool
    on newer Pythons fails.
