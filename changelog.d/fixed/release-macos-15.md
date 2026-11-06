- **The release workflow builds macOS wheels on `macos-15`**.
    GitHub no longer serves the `macos-14` image ("The configuration
    'macos-14-arm64-us-default' is not supported"), so every macOS job of the
    v0.80.0 release run was cancelled before a runner was assigned and
    nothing was published. The wheels still pin min-os 11.0.
