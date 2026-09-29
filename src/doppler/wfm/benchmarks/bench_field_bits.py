"""Benchmark for field_bits -- the Field text form, parsed and rendered.

A Field is read once per flag or scene key, so the parse is not a hot
loop; what is worth knowing is how the cost splits between reading the
text and producing the bits. The literal rows scale with the text (hex
expansion is the only work), the generated rows with the bits the
generator emits, and ``*REPS`` rows show that a repetition is a copy, not
a second generator run.

Run: pytest src/doppler/wfm/benchmarks/bench_field_bits.py --benchmark-only
"""

import pytest

from doppler.wfm import field_bits

#: (spec, bits it renders). Two literals, two generated kinds, and one
#: repeated generator whose length matches the unrepeated one beside it.
SPECS = [
    ("0x1ACFFC1D", 32),
    ("0x" + "a5" * 512, 4096),
    ("pn:4095:12", 4095),
    ("pn:1023:10*4", 4092),
    ("gold:4096:10:0x3a6:0x15e:0x237:0x49", 4096),
]


@pytest.mark.parametrize("spec,n", SPECS, ids=[s[:24] for s, _ in SPECS])
def test_bench_field_bits(benchmark, spec, n):
    out = benchmark(field_bits, spec)
    assert len(out) == n
    if benchmark.stats:
        benchmark.extra_info["Mbit_s"] = n / benchmark.stats["mean"] / 1e6
