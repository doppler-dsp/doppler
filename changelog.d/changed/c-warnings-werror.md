- **doppler's own C builds with `-Wall -Wextra`, and doppler's build turns
    them into errors.** Before this, none of the 691 compile lines carried a
    warning flag, so an unhandled enum value (#1642) was visible only on clang.
    `cmake/warnings.cmake` covers every doppler target and leaves `vendor/`
    alone. `-Werror` is on only in doppler's own builds (`make`, CI):
    `DOPPLER_WERROR` defaults OFF, so consumers building from source (sdist,
    vcpkg, packages, `add_subdirectory`) get warnings, not errors. Getting to
    zero found a validation claim that was printed as PASS without being
    checked (#1658, #1657).
