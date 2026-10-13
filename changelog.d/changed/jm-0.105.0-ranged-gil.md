- **A composer's integer ranged bound must be an `int`; ten methods now
    release the GIL while they run.** A float bound is a `TypeError`, as in
    `Segment("tone", off_samples=(64.0, 128.0))`, and a negative one an
    `OverflowError` (jm gh-2224); `PSD.accumulate`, `BerMeter.score`,
    `Writer.write` and seven more run without the GIL (gh-2259, #2289).
