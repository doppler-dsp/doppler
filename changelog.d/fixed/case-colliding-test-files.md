- **A Windows or macOS checkout no longer shows a phantom edit in the
    resample tests.** `test_Resampler.py` (a jm scaffold) and
    `test_resampler.py` (the real suite) were one path on case-insensitive
    filesystems. The real suite now has jm's name, and the tracked-paths gate
    fails on any two paths that differ only in case.
