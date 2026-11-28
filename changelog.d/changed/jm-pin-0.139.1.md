- **just-makeit pin 0.137.0 → 0.139.1.** `HalfbandDecimatorQ15.execute()`
    refuses an interleaved `int16` input whose length is odd with a
    `ValueError`, where it dropped the last element
    ([just-makeit#1999](https://github.com/just-buildit/just-makeit/issues/1999));
    a `Synth` or `Segment` re-`__init__` that refuses one argument leaves
    every field as it was, where it kept the arguments parsed before the
    bad one: the constructor parses into a copy and commits it whole
    ([just-makeit#2878](https://github.com/just-buildit/just-makeit/issues/2878)).
