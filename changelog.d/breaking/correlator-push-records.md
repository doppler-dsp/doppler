- **`Correlator.push` returns a structured array of hit records**, one
    field per member, instead of 4-, 5- or 9-field tuples, and loses no
    input to a full list: the 1024-hit cap is gone
    ([design §5](docs/design/correlator-python.md)).
