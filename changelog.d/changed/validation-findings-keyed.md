- **A validation finding is cited by its key and numbered when the report
    renders** ([#2059](https://github.com/doppler-dsp/doppler/issues/2059)).
    `R.find(key, ...)` assigns the number and `R.ref(key)` resolves to it, so
    dropping a finding cannot re-point a citation; #2056 left five that way.
    `make lint-finding-citations` refuses a typed number outside a report and
    a cited key no report records. Six reports renumber findings that were
    out of order.
