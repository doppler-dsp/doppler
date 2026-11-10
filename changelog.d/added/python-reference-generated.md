- **The Python API reference is generated from the source.** Each public
    package gets a page at `reference/doppler/<module>/`, rendering its
    `__all__` with a summary table up top and, on each entry, the pages
    that link to it. A new module, class or function appears by existing.
    `make docs-check` fails when a public name has no entry in the built
    site, and `make lint` when a package declares no `__all__` or a
    submodule exports a name its package does not. So `doppler.dsss` now
    exports `Acquirer`, `CoarseChannel` and `Detection`, and
    `doppler.resample` the five CIC design helpers (#3000).
