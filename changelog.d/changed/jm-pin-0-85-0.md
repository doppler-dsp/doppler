- **just-makeit pin 0.84.0 → 0.85.0.** No generated code changes. It
    adds a `CMakePresets.json`, so Visual Studio and VS Code's CMake Tools
    configure doppler through `cmake --preset` into `out/build/`. doppler's
    copy also turns on `BUILD_PYTHON`, because doppler's CMake defaults it
    off and an IDE build would otherwise have no Python extensions.
