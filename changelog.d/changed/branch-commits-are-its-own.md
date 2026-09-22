- **`issue-link-check` refuses a branch carrying a base commit under a new
    hash.** A commit with the same author, author date and subject as one on
    the base is what `--amend` after a hook-blocked commit produces. The gate
    used to pass it on that commit's own `No-issue:`, and doppler#1472 was
    pushed that way.
