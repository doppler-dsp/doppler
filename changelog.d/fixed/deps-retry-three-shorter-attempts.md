- **The CI dependency install retries three times, not twice**, inside the
    same step ceiling. `DEPS_DEADLINE x DEPS_TRIES` moves from 600x2 to
    420x3 (~21.5 min against the 25-minute ceiling `deps-budget-check`
    enforces).

    Measured, not guessed. With the runners' azure mirror answering `Ign` and
    the archive.ubuntu.com fallback trickling, the timings say the retry is
    the thing that works and the long deadline is not:

    | job                   | install-deps                                  | outcome |
    | --------------------- | --------------------------------------------- | ------- |
    | Build on ubuntu-24.04 | 831 s = 600 (attempt 1 killed) + 10 + **221** | pass    |
    | Python 3.13           | 119 s                                         | pass    |
    | coverage, Python 3.11 | 1210 s = 600 + 10 + 600, both exhausted       | fail    |

    A healthy attempt is 120-220 s, so the back half of a 600 s deadline is
    spent stalled: it buys nothing a retry would not buy sooner. Three 420 s
    attempts give ~2x headroom over a healthy install and three rolls of the
    mirror lottery instead of two.

    This is a mitigation and is labelled as one — [#885](https://github.com/doppler-dsp/doppler/issues/885)
    carries the fix, which is to stop apt-installing the 112 MB of toolchain
    the runner image already ships.
