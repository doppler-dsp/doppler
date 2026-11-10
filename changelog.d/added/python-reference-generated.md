- **The Python API reference is generated from the source.** Each public
    package gets a page at `reference/doppler/<module>/`, rendering its
    `__all__` with a summary table up top and, on each entry, the pages
    that link to it. A new module, class or function appears by existing.
    `make docs-check` now fails when a public name has no entry in the
    built site (#3000).
