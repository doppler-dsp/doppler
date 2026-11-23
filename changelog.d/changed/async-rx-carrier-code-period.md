- **`AsyncDsssReceiver` forms its carrier error over the code-period window.**
    The pre-despread carrier loop squares one coherent sum over the
    code-period window the code loop's look-back picks, instead of squaring
    each partial. A receiver (and `AsyncDsssPool`) therefore needs a symbol
    of at least a code period (`symbol_rate * len(code) <= chip_rate`), and
    refuses to build otherwise. At the CCSDS 415.1-B-1 floor this window
    slipped 10 of 90 receivers where the per-partial form slipped 41 of 89.
    `Dll` gains `dp_dll_get_window_sum()`.
