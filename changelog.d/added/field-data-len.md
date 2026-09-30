- **A Field can be `data:LEN`: a frame's payload drawn from a data
    source** ([#1619](https://github.com/doppler-dsp/doppler/issues/1619)).
    It parses, prints and lays out, with at most one per frame. It has no
    bits of its own, so rendering it is refused, and until a source is
    connected every face refuses a frame that uses it, naming the data
    source. Design:
    [The Payload as a Data Source](docs/design/payload-data-source.md).
