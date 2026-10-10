# The Spectrogram — chunks in, rows out

A stream arrives in whatever pieces the transport made of it; a spectrogram
wants rows of `nfft` bins, one every `hop` samples, **every one of them**, and
the same rows however the stream was cut. Nothing in the library does that
today. This page is the design of the program that makes it one object: what
it is for, what it must guarantee, what is still unknown, and which existing
primitives it is built from rather than re-implements.

The dated record behind it — what each step measured, and the guesses that
were wrong — is [the measurement record](spectrogram-measurements.md).
Section numbers are shared between the two, so a `§5.1` in an issue or a code
comment is the same entry on both.

______________________________________________________________________

## 1. The use cases

| who calls it                                                              | with what                                                  | and does what with the answer                                                          |
| ------------------------------------------------------------------------- | ---------------------------------------------------------- | -------------------------------------------------------------------------------------- |
| a live display (the spectrum analyzer, a waterfall)                       | chunks of whatever size a socket or a pull source delivers | appends **every** row to a scrolling image                                             |
| a capture analyst reading a file                                          | a `Reader`'s blocks, or a whole array                      | plots the whole capture, and wants the last partial row, once, at the end              |
| a detector looking for bursts in time and frequency                       | the same rows                                              | thresholds them; the object stops at the row, the detection is the caller's            |
| an averaging consumer (video-averaged waterfall, max-hold over many rows) | rows                                                       | folds them with `AccTrace`; the spectrogram does not average, so this composes cleanly |
| a resumable pipeline (a worker moved to another thread, process or pod)   | its state blob                                             | resumes **bit for bit** mid-frame                                                      |

What those callers do today is the evidence the object is missing, not a list
of things to port:

- the only spectrograms in the tree are whole-array loops in two examples,
    one of which drops its last frame;
- the spectrum analyzer carries two copies of the same re-blocking loop in
    its sources, and its engine deliberately emits only the **newest** window,
    which is right for a display that cannot keep up and exactly wrong for a
    spectrogram;
- `PSD` drops a trailing partial frame, has no hop, and returns an average
    rather than rows;
- chunk invariance is tested only object by object, in private copies
    (`doppler_channel`, `wfm_synth`, the resampler), each a page of loop bounds
    and none a property another object could join; so a streaming object that
    has no such copy has no such test.

## 2. Design goals, and what is not yet known

The goals, each of which is a thing a test can fail:

| #   | goal                                                                                                                                                                  |
| --- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| G1  | **Chunk invariance.** The rows are a function of the input stream, not of how it was split into calls — including splits of 1, `nfft`-1, `nfft`, `nfft`+1 and random. |
| G2  | **Every row, none twice.** Row *k* covers samples `[k·hop, k·hop + nfft)`. The one partial row at the end is emitted only by an explicit `flush`, and on the grid.    |
| G3  | **One spectrum kernel.** A row equals the PSD of that frame, bit for bit — same window, same zero-pad, same FFT, same coherent-gain dBFS reference.                   |
| G4  | **A bounded, serializable carry.** Fewer than `nfft` samples are held between calls, so the state blob has a size that depends on the shape alone.                    |
| G5  | **Transport independence.** It consumes arrays. It has no idea whether they came from a file, a socket or a ring.                                                     |
| G6  | **No cost the hand loop did not have.** Owning the loop must not make the stream slower.                                                                              |
| G7  | **Backpressure loses nothing.** A short output buffer slows the stream down; it never drops input.                                                                    |

What is **not known**, and is therefore what the characterization measures
rather than confirms (U5 and U6 are answered; U1–U4 have no number yet):

- **U1 — the carry's copy against a bypass.** Every sample goes through the
    ring (about 8 bytes copied per complex sample). Whether feeding the FFT
    straight from the caller's buffer when the carry is empty is worth a second
    path is a question about `nfft` from 256 to 65536 and `hop` from `nfft`/4 to
    `nfft`, and is decided only if a bench shows the copy matters.
- **U2 — row latency.** The time from a sample arriving to its row being
    available, against `hop` and against the chunk size, with attention to the
    two extremes (chunks of 1, and chunks far larger than `nfft`).
- **U3 — where a row's time goes.** Window, FFT, power, and the `log10` of the
    dB conversion. If the logarithm dominates, a cheaper approximation changes
    what the default output mode should be.
- **U4 — rows per second one core sustains** at `nfft` 1024 and `hop` 256, and
    what that leaves a display.
- **U5 — the dB floor. ANSWERED (§5.4).** A dB row reads no lower than
    −200 dB per bin: a tone under that, and an all-zero frame, give the same
    row. Wideband noise reaches it about `10·log10(nfft)` sooner, and float
    sources in the tree can produce such samples. That is the dB face by
    design; a caller who must tell zero from tiny reads the samples, or the
    linear row (#1968), whose only floor is float32's.
- **U6 — what callers want at the end of a stream. ANSWERED (§5.5).** `flush`
    stays explicit: only the caller knows where its own stream ends, and the
    transports' end-of-stream marker can be dropped (PUB/SUB) or repeated
    (PUSH/PULL), so a flush implied by it could lose the last row or emit one
    mid-stream.

## 3. The prototype

A throwaway numpy implementation, in a scratch directory and **not committed**,
asked the three questions the design rests on before any C existed:

1. Does a carry of fewer than `nfft` samples reproduce the one-shot rows for
    every split?
1. Is the flushed row the row a one-shot run would produce over the input
    zero-padded to that row's end — so that it sits on the hop grid and exists
    only when it holds an uncovered sample?
1. How far apart are a spectrogram built from `fft` plus `10·log10|X|²` and
    the PSD's convention (divide by the window's coherent gain squared)?

The answers are in [§5.3](spectrogram-measurements.md#53-the-prototype-2026-10-08):
yes, yes, and by **20·log10(Σw)** — 54.18 dB for a 1024-point Hann window — so
a second kernel is not a small inconsistency but a wrong level. That is why
the design promotes PSD's kernel instead of writing one. The prototype also
mis-reported its own contract on the first run, through a comparison bug of its
own; the record keeps that.

## 4. The shape

Three existing things, composed. The object adds almost nothing of its own,
which is the design.

### 4.1 The ring owns the carry

The ring already does "any chunk in, a contiguous frame out, with a hop"
(`write_some`, `peek`, `consume`), but every consumer writes that loop itself.
The **framer** is that loop once, a face of the ring beside its element-typed
face:

| call                 | what it promises                                                                                                                |
| -------------------- | ------------------------------------------------------------------------------------------------------------------------------- |
| `framer_feed`        | the only write path; admits input only as far as the frames it yields fit the caller's room, so a drained framer holds `< nfft` |
| `framer_next`        | the next frame, zero-copy; the pointer holds until the next framer call                                                         |
| `framer_flush`       | ends the stream: the one zero-padded row it owes, on the hop grid, or none; **refused** while whole frames are still buffered   |
| the snapshot triplet | the carry as a standard state blob, fixed-size for a given shape, naming its hop, position-free                                 |

The framer owns its ring exclusively, which is what makes `framer_feed` the
only way in and the bound a fact rather than a hope. Its own section belongs on
[the ring page](ring-buffer.md), because it is a face of the ring.

### 4.2 PSD's kernel is the spectrum

A spectrogram is **PSD per frame, no averaging**. `PSD` already owns every
step that makes a frame a spectrum — the window (Hann, Kaiser, Blackman-Harris,
and rectangular), the zero-pad, the FFT, the DC-centred power, and the dBFS
reference that makes a full-scale tone read 0 dB. Those steps are one internal
transform that `accumulate` and the per-frame calls
(`dp_psd_frame_power`, `dp_psd_frame_db`) share, and one dB conversion that the
averaged readouts and the single-frame one share.

The alternative, `fft` plus `dp_magnitude_db_cf32`, is rejected on the
prototype's number (§3), not on taste.

### 4.3 The object

```text
dp_spectrogram_create (nfft, hop, window, beta, mode);
size_t dp_spectrogram_push (s, const cf32 *in, size_t n_in,
                            float *out, size_t max_out);  /* floats written */
size_t dp_spectrogram_push_max_out (s, size_t n_in);  /* rows_for * nfft  */
size_t dp_spectrogram_consumed (s);        /* input the last push took    */
size_t dp_spectrogram_rows_for (s, size_t n_in);  /* exact, in rows       */
size_t dp_spectrogram_flush (s, float *row);      /* 0 or nfft floats     */
size_t dp_spectrogram_pending (s);
/* + reset, destroy, and the state triplet */
```

It owns a ring, a framer, and the PSD estimator whose kernel it calls. `push`
loops `framer_feed` → `framer_next` → the PSD kernel, so any chunk size works
whatever the ring's capacity. **A sample is taken unless it would complete a
row `out` has no room for** — the framer's own `feed` contract, applied to
rows. So a short `out` stops at a whole row and `consumed()` says where to
resume, input that completes no row is always taken (even with `max_out` 0),
and `push_max_out(n)` is exactly the room that makes a push take all of `n`.

`nfft` is a power of two and `1 <= hop <= nfft`: frame length and row width
are then the same number, which a frame PSD zero-pads to more bins than
samples would not give (separating the two is
[#1966](https://github.com/doppler-dsp/doppler/issues/1966)). Rows are dBFS;
`mode = power` is reserved and refused until PSD's normalised per-frame
power is on main, so the two modes share one reference
([#1968](https://github.com/doppler-dsp/doppler/issues/1968)). Every row is
DC-centred exactly as PSD's kernel emits it, bin *k* at index `nfft/2 + k`:
bin order has one home, and an FFT-order option, if one is ever wanted,
belongs to that kernel
([#1988](https://github.com/doppler-dsp/doppler/issues/1988)). The state is
the spectrogram's envelope around the framer's snapshot and nothing else,
because the window, the plan and the scratch are configuration.

The Python face is **declarative or absent**. A `push` that returns
`(rows, nfft)` needs jm to express a two-dimensional result
(just-buildit/just-makeit#2115, implemented by #2152's `out_cols`), and that
is its only jm dependency: jm already sizes a `variable_output` method from
its input through a two-argument `_max_out (state, n_in)`, which is
`push_max_out`. Until that release, the Spectrogram is a hand-owned C
component (`[project].c_deps`); no hand-written binding stands in.

### 4.4 What it composes, and what it does not re-implement

| need                                       | owner                                                       |
| ------------------------------------------ | ----------------------------------------------------------- |
| any chunk in, fixed frames out, hop, flush | the ring's framer                                           |
| window, zero-pad, FFT, power, dBFS         | `PSD`'s per-frame kernel (and through it `FFT` and windows) |
| the state envelope and cursors             | `dp_state.h`                                                |
| averaging rows, if a caller wants it       | `AccTrace`, composed by the caller                          |

## 5. What is measured

Only what has a number today. Each item is a claim; the dated entry, its
method and its caveats are in the record under the same number.

- **§5.1** — Promoting PSD's kernel left every readout **byte-identical**
    to the version before it, over 72 configurations (3 windows × 3 lengths,
    one not a power of two, × 2 pads × 4 averaging modes, complex and real
    input).
- **§5.2** — `accumulate` is **bimodal** on this machine, about 1.79 or 2.71
    ns/sample, with the same medians before and after the promotion: a
    memory-layout effect that the unchanged code shows too.
- **§5.3** — The prototype: the contract holds on 392 cases, and a second
    kernel would be wrong by `20·log10(Σw)`.
- **§5.4** — The dB floor: a row reads no lower than −200 dB, so a tone
    under it and an all-zero frame give the same row bit for bit. Noise
    vanishes about `10·log10(nfft)` sooner: at `nfft` 1024 a total of
    −180 dBFS leaves on average 1 to 6 bins of 1024 above the floor,
    depending on the window, and the exponential-bin model predicts every
    count to within 2.3 bins.
- **§5.5** — End of stream: the only caller flushes explicitly, and the
    transports' marker is not reliable enough to imply a flush.

The framer's cost against the hand-written loop is a property of the ring, and
is recorded with it in [the ring's measurements](ring-buffer-measurements.md).

## 6. The order of work

One slice at a time, each its own change, each with its own review. The live
state — which of these is merged — is the tracking issue
[#1894](https://github.com/doppler-dsp/doppler/issues/1894), not this page.

| slice | what                                                                                                        | proven by                                                  |
| ----- | ----------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------- |
| 1     | the ring's framer, and the chunk-invariance harness with its ratchet                                        | sabotage of each claim; the harness's own negative control |
| 2     | PSD's per-frame kernel, and a rectangular window                                                            | byte-identity against the previous library (§5.1)          |
| 3a    | the C `Spectrogram`: core, header, C tests, state round trip, a C twin example                              | the lifecycle's phases 2–4, 6–10                           |
| 3b    | the Python face, declaratively                                                                              | the jm release that carries #2115                          |
| 4     | what is left: the ring's other consumers onto the framer, `PSD`'s own hop, the example and re-blocker ports | each its own issue                                         |

## 7. What this is not

- **Not an averager.** Rows out; folding them is `AccTrace`'s, composed by the
    caller.
- **Not a decimator or a front end.** It consumes baseband at the rate it
    runs; the DDC upstream is the caller's.
- **Not a display.** No colour map, no scrolling, no dB range; those are the
    consumer's.
- **Not a detector.** It stops at the row.
- **Not a transport.** It never learns where its samples came from.
- **Not real-input or double-precision yet.** Complex float32 first; the rest
    is a follow-up with its own measurements.

## See also

- [Streaming Spectrograms](../guide/spectrogram.md) — the user guide, and the
    tested example
- [The Ring Buffer](ring-buffer.md) — the framer's home
- [The measurement record](spectrogram-measurements.md)
- [Spectral analysis with PSD](../guide/spectral-psd.md) — the estimator whose
    kernel this reuses
- [Spectrum Analyzer](specan.md) — the display this serves, and why its engine
    keeps only the newest window
- [State Serialization](state-serialization.md) — the envelope the carry uses
