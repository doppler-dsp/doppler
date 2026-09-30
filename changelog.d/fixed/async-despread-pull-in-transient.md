- **The async despreader demo shows the code loop's pull-in transient, and
    gates on it.** Its lock statistic dips about 32% while the loop absorbs
    the truncating NCO's frequency step. The dip lasts `1/(ζωₙ)` ≈ 375 epochs
    at `bn = 0.002`. The run now lasts 3τ plus a tail, asserts that `R`
    recovers, and the page states the dip instead of "no residual ring"
    (#1670).
