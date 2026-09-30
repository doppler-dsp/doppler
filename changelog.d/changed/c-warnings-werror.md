- **doppler's own C builds with `-Wall -Wextra -Werror`.** Before this, none
    of the 691 compile lines carried a warning flag, so an unhandled enum value
    (#1642) was visible only on clang. `cmake/warnings.cmake` applies the flags
    to every doppler target, including the jm-generated ones, and leaves
    `vendor/` alone. Flow-sensitive gcc classes stay warnings. Getting to zero
    found a validation claim, "jitter proportional to bn", that
    `validate_carrier_mpsk_jitter` printed as PASS without ever checking
    (#1658, #1657).
