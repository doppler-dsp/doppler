- **A scene refuses `"payload"` beside `"frame"`**, as the CLI refuses
    `--bits` beside `--frame`. It used to exit 0 and drop the payload
    (#1683). A carried frame description is the whole frame, payload
    included. The pair is one declaration in the field table, and the CLI,
    a scene, a `Synth`/`Segment`, `--help` and the schema all refuse from
    it. A refused `Synth` now raises `ValueError` with the source's reason
    rather than `RuntimeError: ... returned NULL`. A framed `bits` source
    with no flat payload now builds.
