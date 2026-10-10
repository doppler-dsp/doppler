- **A validation finding is cited by its key, and numbered when the report
    renders** (#2059). `R.find(key, ...)` assigns the number by position
    and `R.ref(key)` resolves to it, so dropping a finding can no longer
    re-point a citation, which left five pointing at the wrong one in #2056.
    A typed number is refused in a report and, by
    `make lint-finding-citations`, everywhere else. Six reports renumbered
    findings that had been out of order, and conv's summary no longer cites
    a finding it never had.
