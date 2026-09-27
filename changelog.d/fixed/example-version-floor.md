- **The example projects refuse a stale doppler install at configure time.**
    Each `example-projects/*/CMakeLists.txt` now asks for
    `find_package(doppler <release> REQUIRED)`, stamped by `make docs-relink`
    at each release, so an old install is refused by CMake rather than
    failing later as a missing header (#1583).
