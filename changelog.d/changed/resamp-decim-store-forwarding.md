- **The resampler's decimating path takes up to 27% less time, bit for
    bit.** Its dump read the integrate-and-dump registers at an offset the
    accumulate loop's vector stores could not forward to; the loop now
    starts its vectors where the dump reads them. `resamp::execute` at
    rate 0.9999 went from 10.7 to 7.8 ns a sample and the pool's
    one-tile push from 9.25 to 8.52 ms (x86-64-v2, gcc 16); the output
    and the state blob are byte-identical.
