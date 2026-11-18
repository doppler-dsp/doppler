- **`StateFilter`'s refusals name the argument that failed.** `StateFilter(...)`
    said one sentence for six different failures (and cited a repo path);
    it now says `t must be a positive, finite update period`, `bn must be >= 0`,
    `bn*t is above the largest the filter accepts`, and so on, from the new C
    `dp_state_filter_params_why()`. `configure()` says what it refused
    instead of `configure failed (rc=-4)`.
