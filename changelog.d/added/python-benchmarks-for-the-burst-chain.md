- **Five real Python benchmarks for the burst chain**, over one shared 64k
    stimulus so the composed object reads against its parts:
    `DsssBurstReceiver`, `BurstAcquisition`, `BurstDemod`,
    `BurstDespreader`, `PolynomialPhaseEstimator`. Each is two rows — noise
    against bursts — and each asserts what it claims, because a benchmark
    that quietly stopped decoding just looks faster. Two pairs come out
    *identical*, which is the useful part: acquisition's correlation and
    CFAR run the same whether a burst is present, and `ppe`'s coherent
    search has no data-dependent branch, so the composed object's
    idle-to-burst delta is attributable entirely to refine and demod.
