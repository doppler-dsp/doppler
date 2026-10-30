# Corr2D batches: many maps in one call

`Corr2D` and `DelayDoppler` make one map per dump, and the only way two maps
combine is the coherent dwell. A batch takes H frames in one call and returns
every map, or one reduction of them, from one C loop that never touches the
dwell. It serves
[#2683](https://github.com/doppler-dsp/doppler/issues/2683) (batch maps and
reductions) and is the loop
[#2670](https://github.com/doppler-dsp/doppler/issues/2670)'s rate search
moves onto. The order a bank of hypotheses on one frame runs in is
[Corr2D hypothesis bank](corr2d-hypotheses.md); this page is the batch the
bank feeds.

______________________________________________________________________

## 1. Use cases

| caller                        | gives                                   | wants back                                                       | today                                                                                                       |
| ----------------------------- | --------------------------------------- | ---------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------- |
| a delay-Doppler sweep (#2683) | H frames, or H hypotheses of one frame  | every map, or the best cell over them and which map it came from | H `execute()` calls at dwell 1; at 64 x 512 that is about 186 us a map, and the cost is the map, not Python |
| acq's rate search (#2670)     | one frame under each rate hypothesis    | each map's peak and its peak-to-mean, to rank the hypotheses     | `acq_core.c` hand-rolls the per-map statistic and the argmax                                                |
| non-coherent integration      | frames whose phase wanders between them | the power summed across frames                                   | a coherent dwell cancels them; nothing sums power                                                           |

The peak-to-mean ranking is why `PEAKS` reports each map's own statistics:
the peak-to-mean of the pointwise maximum over maps is a different number,
and is the wrong one to rank hypotheses by.

## 2. What a batch promises

- **Map h is a fresh dwell-1 object's map for frame h**, bit for bit, on
    every path (general 2-D, the single-row fast path, `col_out`,
    delay-Doppler) and at every thread count. `execute()` and the batch run
    the same row, tile and frame helpers in `corr2d_core.c`, so this is one
    implementation, not two kept equal.
- **The dwell is untouched.** A batch never reads or writes `accum`, `count`
    or the state blob, so it may run in the middle of a dwell and needs no blob
    version.
- **Every reduction folds in h order**, so a float sum is the same at every
    thread count.
- **A refused batch writes nothing**: an empty batch, a length that is not a
    whole number of frames, or an output with room for less than the whole
    result. A batch never truncates the way `execute()` does, because a
    truncated reduction has no meaning.
- **No Python callback in the loop.** C callers get a reducer
    (`dp_corr2d_fold_t`, handed to `dp_corr2d_execute_fold`); Python gets one
    typed method per reduction, because the return shapes differ.

## 3. The operations

| C call / Python method | returns                                        | reduces                                                                          |
| ---------------------- | ---------------------------------------------- | -------------------------------------------------------------------------------- |
| `execute_all`          | H x n_out complex                              | nothing: map h at row h                                                          |
| `execute_coherent`     | n_out complex                                  | the frames' product spectra summed, inverted once: a fresh dwell-H object's dump |
| `execute_power_sum`    | n_out float                                    | `sum_h abs(map_h)**2`, cell by cell                                              |
| `execute_power_max`    | n_out float, n_out uint32                      | `max_h abs(map_h)**2` and that h; a tie keeps the lowest h                       |
| `execute_peaks`        | H records: peak power, row, column, mean power | each map to its own peak and mean                                                |

`execute_coherent` sums in the spectrum, as a dwell does, rather than summing
the maps: one inverse instead of H, and the sum of the maps only up to
rounding. Each call is a thin wrapper over the one loop with a built-in
reducer, and the header (`corr2d_core.h`) is the contract per call.

## 4. Threads

[Corr2D hypothesis bank §5](corr2d-hypotheses.md#5-cost) measured that
splitting one short frame across a pool costs more than it saves: up to
`nx = 256` an ordered frame is 1.5–8x slower on any pool than on one thread.
So:

- **two frames or more**: each worker makes whole maps on its own plans and
    scratch, and the maps are folded in h order;
- **one frame**: split as `execute()` splits it;
- **the coherent sum**: each worker owns a fixed range of rows through every
    frame, so a whole batch is two pool dispatches and each row still sums the
    frames in h order.

A row or a column tile is the same arithmetic on any worker's plans, which is
what keeps the result identical across thread counts. The scratch is one slot
per worker (`2 n + n_out` complex values, plus the row zero-pad when
`nx_out > nx`), made on the first batch.

## 5. Unknowns

- **What a batch saves over H `execute()` calls** at the request's geometry
    (64 x 512, H = 48). The per-map work is the same, so the saving is the
    per-call overhead and, on a pool, the dispatches. The benchmark row in
    `bench_corr2d_core.c` and `bench_corr2d.py` measures it.
- **Whether `ALL`'s copy out of the scratch map costs anything measurable**
    next to the transforms. It is one pass over n_out values a map.
- **Where whole maps lose to a split**: long codes with fewer frames than
    workers. Phase 0 measured a split gaining at `nx = 1023` (0.6–0.94x of one
    thread); a batch of two such frames on a large pool uses two workers. Not
    measured yet.

No Python prototype was needed: a batch changes no arithmetic, and every
claim above is checked in `test_corr2d_core.c` against the object's own
`execute()`.
