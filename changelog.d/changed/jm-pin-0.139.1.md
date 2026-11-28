- **just-makeit pin 0.137.0 → 0.139.1.** `HalfbandDecimatorQ15.execute()`
    refuses an interleaved `int16` input whose length is odd with a
    `ValueError`, where it dropped the last element
    ([just-makeit#1999](https://github.com/just-buildit/just-makeit/issues/1999));
    a refused assignment to a `Synth` or `Segment` field leaves the source
    as it was, since every composer setter writes a copy and commits it
    only on success
    ([just-makeit#2878](https://github.com/just-buildit/just-makeit/issues/2878)).
