- **just-makeit pin 0.81.0 → 0.82.0.** Pure tooling: `jm apply` changes no
    generated file. It completes what the ring's generated binding needs
    ([just-makeit#1426](https://github.com/just-buildit/just-makeit/issues/1426),
    doppler-driven): a borrow names its release call (`releases`), an array
    input can be refused instead of coerced (`strict`), and a status message
    can carry `{n}` / `{capacity}`. The ring's tests run 71 of 72 against it.
