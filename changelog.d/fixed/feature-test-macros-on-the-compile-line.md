- **A feature-test macro in a public header does not work, and two of ours
    were there ([#986](https://github.com/doppler-dsp/doppler/issues/986)).**
    glibc's `features.h` latches the feature set on its **first** inclusion,
    so `#define _GNU_SOURCE` inside a header is a no-op for every translation
    unit that reached libc before including it — which is most of them.
    `buffer/buffer.h` carried one at its own line 51 and it was inert in
    exactly that way, measured rather than reasoned: after
    `#include <stdio.h>` then `#include "buffer/buffer.h"`, `__USE_GNU` is
    **not defined**.

    It never bit in a normal build for a reason that makes it worse, not
    better. doppler compiles as `-std=gnu99` (`CMAKE_C_EXTENSIONS` defaults
    ON), so `_DEFAULT_SOURCE` already declared the `syscall()` and
    `ftruncate()` that `buffer.h` calls. The line therefore looked like what
    made the file work while contributing nothing. Under a strict `-std=c99`
    the same header compiles or fails **on include order alone**, and
    `native/inc` is installed wholesale — a downstream picks its own order and
    its own dialect.

    This had already cost a revert. The phase-6 telemetry work on the
    follow-mode reader was backed out because adding an include *above* that
    line stopped `syscall` resolving; it read as "telemetry doesn't work
    here" rather than as an include-order trap, because the header appeared
    to have handled the problem.

    The macros are now on the compile line, where include order cannot defeat
    them, and on the **exported target** (`PUBLIC`), so a downstream that
    links doppler inherits the requirement instead of having to discover it.
    The 18 per-file `#define _GNU_SOURCE` lines in tests are gone with them —
    one definition site, and they had begun emitting redefinition warnings.

    `scripts/check_installed_headers.py` gains the second question, because
    it already walks exactly the right file set: no installed header may
    define a feature-test macro. Registration-free and allowlist-free. **It
    found a second instance on its first run** — `dp_isotime.h` raising
    `_POSIX_C_SOURCE`, whose comment also claimed doppler "builds as strict
    c99", which it does not.
