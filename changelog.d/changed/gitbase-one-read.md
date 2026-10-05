- **Every ratchet reads its baseline through one `scripts/_gitbase.py`**
    (#1838). Five gate scripts each wrote their own merge-base read, and
    three still did. They now share `resolve_base` and `show_at`, while what
    each does with an unreadable ref (fail closed, or "nothing to compare")
    stays its own, pinned by `test_base_ref_reads.py`.
