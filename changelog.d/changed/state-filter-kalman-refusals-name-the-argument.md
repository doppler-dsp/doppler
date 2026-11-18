- **`KalmanStateFilter`'s refusals name the argument that failed.** One sentence
    used to cover ten different refusals; it is now `bn alone`, `t must be a   positive, finite update period`, `the bandwidth sigma_e and sigma_a give   is above the largest the filter accepts`, and so on, from the new C
    `dp_state_filter_kalman_args_why()`. `configure()` says what it refused.
