- **`make spine-check` shows the lifecycle, object by object.** For each
    object a branch touches it prints the ten phases of
    [the lifecycle card](docs/dev/contributing/adding-algorithms.md) as
    present, missing or skipped, each answered by the gate that owns it. A
    new gap fails `make lint` unless a commit declares it with
    `Spine-skip:`; the 202 gaps of today are held in `scripts/.spine-gaps`,
    which only shrinks (#3140).
