# Clock Doppler — a source that is moving

A carrier offset moves the carrier. **Doppler moves the clock.**

That distinction is the whole page. `--freq 12000` puts a signal 12 kHz up
the band and leaves its symbol rate exactly where it was. A real emitter
closing at 7.5 km/s does something else: it rescales the *received time base*,
so the carrier, the symbol rate and the chip rate all move together, by the
same fraction. A receiver's timing loop sees an error that a carrier-only
offset never produces — which is precisely the error you wanted to test.

`--doppler` is that second thing. It is given in **ppm** — parts per million
of time-base scale — because that one number is what moves every clock at
once:

$$
f_\text{rx} = f_\text{tx}\,(1 + d\times10^{-6}),
\qquad
R_\text{sym,rx} = R_\text{sym,tx}\,(1 + d\times10^{-6})
$$

______________________________________________________________________

## The four flags

| Flag                           | Unit  | What it sets                                                                                                     |
| ------------------------------ | ----- | ---------------------------------------------------------------------------------------------------------------- |
| `--doppler PPM[:PPM]`          | ppm   | Time-base scale. `0` (default) builds no channel at all.                                                         |
| `--doppler-rate PPM_S[:PPM_S]` | ppm/s | Linear ramp on the above — an accelerating pass.                                                                 |
| `--carrier-hz HZ`              | Hz    | The RF carrier the ppm is *referred to*, which is what turns a time-base scale into a coherent carrier rotation. |
| `--doppler-lifetime L`         | —     | `per_instance` (default) or `persist`. See [below](#the-lifetime-is-a-choice).                                   |

A LEO pass at S-band is a couple of tens of ppm:

```sh
# 25 ppm closing on a 2.4 GHz carrier ≈ 60 kHz of carrier shift, and a
# symbol rate 25 ppm fast along with it.
wfmgen --type qpsk --fs 1e6 --sps 4 --snr 12 --count 20000 \
       --doppler 25 --carrier-hz 2.4e9 -o pass.cf32
ls -l pass.cf32
```

`--carrier-hz` is **independent** of the two ppm figures, and `0` is a real
answer rather than an unset field: it warps the clock and applies no carrier
rotation, which is the right model for a baseband recording whose carrier was
already stripped. Set it when you want the rotation that accompanies the warp.

`--doppler-rate` ramps the offset as the pass progresses:

```sh
# A pass that sweeps through zero: -20 ppm, closing at 4 ppm/s.
wfmgen --type bpsk --fs 1e6 --sps 8 --count 20000 \
       --doppler -20 --doppler-rate 4 --carrier-hz 2.2e9 -o accel.cf32
ls -l accel.cf32
```

!!! note "The rate is per second of *stream*, not per second of on-time"

    The channel keeps running through a segment's gaps, on the noise floor
    the gap already carries. An emitter does not stop moving because its
    burst ended, so a two-burst scene with a 10 ms gap resumes 10 ms further
    along the pass — not where it left off. Skip the gaps and
    `doppler_rate` would quietly mean "ppm per second of transmission",
    which is not a geometry anything flies.

______________________________________________________________________

## Ranged, like `freq` and `snr`

A pass is a distribution, not a number. Both ppm fields take the same
`LO:HI` form as `--freq`/`--snr`/`--level`, drawn uniformly on each repeat
instance:

```sh
# Eight bursts, eight geometries — a Monte Carlo over the pass.
wfmgen --type qpsk --fs 1e6 --sps 4 --snr 10 --count 4096 --off 2000 \
       --repeats 8 --doppler 2:9 --doppler-rate 0.1:0.5 \
       --carrier-hz 1.5e9 --file-type sigmf -o sweep
ls sweep.sigmf-meta
```

The draw is a hash of `(seed, repeat index, segment, source, field)`, so
`--record` stores the **span** and `--from-file` replays the identical
sequence of draws. What each instance actually flew is recorded separately,
per annotation, in the SigMF sidecar:

```json title="one annotation from sweep.sigmf-meta"
{
  "core:sample_start": 6096,
  "core:sample_count": 4096,
  "wfmgen:doppler_ppm": 2.1624832257080335,
  "wfmgen:doppler_rate_ppm_s": 0.4429440692793868,
  "wfmgen:carrier_hz": 1500000000,
  "wfmgen:doppler_lifetime": "per_instance"
}
```

Those keys appear only when a channel was actually built, so a scene without
Doppler writes exactly the sidecar it always did.

______________________________________________________________________

## The lifetime is a choice

`--repeats 8` plays one segment eight times. Does the emitter restart its
pass on each burst, or fly straight through?

- **`per_instance`** (default) — the channel dies with each instance and the
    geometry restarts. This is the **repeated-trial** shape: eight
    independent looks at a target, which is what composes with a ranged
    `--doppler` re-drawn per instance.
- **`persist`** — one continuous pass carries across the segment's gaps and
    repeat instances. This is the only lifetime under which `--doppler-rate`
    accumulates over a multi-burst scene: burst 8 is further along than burst
    1 by the full elapsed stream time.

```sh
# One pass, eight bursts of it — the offset has ramped by the last one.
wfmgen --type bpsk --fs 1e6 --sps 8 --count 4096 --off 4000 --repeats 8 \
       --doppler 5 --doppler-rate 200 --carrier-hz 2e9 \
       --doppler-lifetime persist -o onepass.cf32
ls -l onepass.cf32
```

A persisting channel is keyed by **(segment, source) position**, which is the
only source identity a scene file carries. So it persists across a segment's
repeat instances and their gaps, and two different segments each get their
own pass — sharing one pass between segments would need a declared source id
the format does not have.

______________________________________________________________________

## In JSON and in Python

Both fields are ordinary source keys, scalar or `[lo, hi]`:

```json title="pass.json"
{
  "version": 1,
  "segments": [
    {
      "type": "qpsk", "fs": 1e6, "sps": 4, "snr": 12,
      "num_samples": 20000,
      "doppler": [2, 9],
      "doppler_rate": 0.4,
      "carrier_hz": 2.4e9,
      "doppler_lifetime": "persist"
    }
  ]
}
```

```sh
wfmgen --from-file pass.json -o from_json.cf32
ls -l from_json.cf32
```

and the same names are `Segment`/`Synth` keyword arguments, with a ranged
field spelled as a tuple:

```python
from doppler.wfm.compose import Composer, Segment

scene = Composer(
    [
        Segment(
            "qpsk",
            fs=1e6,
            sps=4,
            snr=12.0,
            num_samples=20000,
            doppler=(2.0, 9.0),
            doppler_rate=0.4,
            carrier_hz=2.4e9,
            doppler_lifetime="persist",
        )
    ]
)
x = scene.compose()
print(x.shape, x.dtype)
```

______________________________________________________________________

## `Plan` refuses a Doppler source

[`Plan`](plan.md) caches each source's clean **on-time** once, in isolation,
and then re-weights it. A Doppler channel does not fit in that box, for two
reasons that were measured against `compose()` rather than assumed:

- **A burst depends on what came before it.** The channel is a stateful
    resampler and it runs through the gaps, so a trailing gap carries the
    burst's ring-out and a leading `delay_samples` advances the geometry
    before the burst starts. A cached on-time has nowhere to keep that
    history. Measured on a clean one-source scene: `off_samples = 256`
    diverges from `compose()` across all 256 gap samples, and
    `delay_samples = 256` diverges over the *burst*, from sample 257 on.
- **The noise sits on the wrong side.** `compose()` puts the AWGN inside the
    synth, so the channel resamples it too; the cache holds a clean render
    and adds noise outside the channel. Measured on a bundled single noisy
    source with no gaps at all: max |err| 1.73 on a unit-power signal.

`persist` would be refused even without those, and for its own reason: the
cache builds each source **independently and concurrently**, and
"bit-identical to the serial build" is a documented property of it — a
channel carrying across segments makes segment *i* depend on segments
0…*i*−1 having been rendered, in order.

So `prepare()` refuses, rather than caching a render that differs from
`compose()` in a way nothing downstream can see:

```python
from doppler.wfm import prepare
from doppler.wfm.compose import Composer, Segment

kw = dict(fs=1e6, sps=4, num_samples=4096, carrier_hz=2.2e9)

try:
    prepare(Composer([Segment("bpsk", doppler=5.0, **kw)]).to_json())
except ValueError as exc:
    print("refused:", "doppler" in str(exc))

# ...but the keys alone cost nothing: zero doppler AND zero doppler_rate
# builds no channel, so a declared lifetime describes nothing.
plan = prepare(
    Composer([Segment("bpsk", doppler_lifetime="persist", **kw)]).to_json()
)
print(len(plan))
```

`compose()` and `stream()` honour both lifetimes in full — only the sweep
cache is restricted. Teaching the cache to carry a channel's history is the
follow-up, [doppler#1109](https://github.com/doppler-dsp/doppler/issues/1109);
until then, sweep a Doppler scene by composing it per point.

______________________________________________________________________

## See also

- [Scenes](scenes.md) — ranged values, `repeats`, `--record`/`--from-file`.
- [Prepare Once, Sweep Many (`Plan`)](plan.md) — what the cache is for.
- [Concepts](concepts.md) — where `Segment` and `Composer` sit.
