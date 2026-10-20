- **`make warnings-check` no longer overwrites the checkout's Python
    extensions and `wfmgen` with its clang build.** Its throwaway trees stage
    them inside themselves, and a gate fails the target if any compiled
    artifact under `src/` changed (#2518).
