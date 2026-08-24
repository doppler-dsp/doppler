# Python Interrupt API

Stopping a run. `doppler.interrupt` is the one flag every blocking wait in
doppler consults, and the scoped guard that arms it.

The flag is **process-wide**, so `Interrupt` is a handle to a facility
rather than an instance of one: two guards observe the same flag. What a
guard scopes is the *arming* — which signals it installed and the latency
it overrode — so both can be undone exactly, by the code that did them.

```python
import numpy as np
from doppler.interrupt import Interrupt

it = Interrupt(np.array([], dtype=np.int32))   # a handle, arms nothing
it.interrupt()
assert it.interrupted()
it.resume()
```

Construction is what arms, and it clears the flag as it does — a stale one
would refuse the first wait inside the very block that just armed:

```python
import signal

import numpy as np

from doppler.interrupt import Interrupt

with Interrupt(np.array([signal.SIGINT], dtype=np.int32)) as it:
    while not it.interrupted():
        ...  # a receive, a ring wait, a generate loop
        # Ctrl+C is what ends this loop in a real run. The request is made
        # in-process here because the guard CHAINS to the interpreter's own
        # SIGINT handler, so a genuine signal would raise KeyboardInterrupt
        # out of this page rather than let it finish.
        it.interrupt()
    assert it.interrupted()

# The flag is process-wide and outlives the guard, so a page that sets it
# puts it back. Inside your own program that is exactly what you do NOT
# want -- see below.
Interrupt(np.array([], dtype=np.int32)).resume()
```

Leaving the block restores every handler the guard displaced. It does
**not** clear the flag: a caller that was interrupted still needs to see
that it was, after the block that noticed has exited.

See [Ending a wait](../design/io-termination.md) for the contract this
serves on all three transports, and why the primitive is an object rather
than the free functions it used to be.

::: doppler.interrupt.Interrupt

## Related pages

<!-- related-pages:start -->

**Design** — [Ending a capture — spooling an endless stream to disk while reading it back](../design/end-of-capture.md), [Ending a wait — one contract for network, memory and disk](../design/io-termination.md)

<!-- related-pages:end -->
