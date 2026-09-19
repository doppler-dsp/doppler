- **A vcpkg port for the Windows C library, with a clang-cl triplet.**
    `vcpkg install doppler:x64-windows-clangcl` from the in-repo overlay
    under `packaging/vcpkg/`; CI installs and consumes it on every PR
    (`make vcpkg-smoke`). Not upstream vcpkg, whose `cl.exe` triplets cannot
    build doppler. See [Install → Windows](docs/install/c.md#with-vcpkg).
