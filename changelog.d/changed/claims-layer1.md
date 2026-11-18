- **C tests can state a claim and its negative control.** `dp_test.h` gains
    `DP_CLAIM`, `DP_CONTROL` and `DP_REFUSAL` (a refusal with its minimal
    pair); `make claims-check` requires every claim a test opens to have a
    control and every test either to open claims or to be on a shrink-only
    list (#847, layer 1). `KalmanStateFilter`'s test is the first adopter, and
    its report's control column reads the claims it recorded.
