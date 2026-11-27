- **A Debug build aborts when the Dll's output bound is wrong.** An emission
    `dp_dll_steps` refuses while `max_out` is at least
    `dp_dll_steps_max_out()` and the code rate has stayed within twice nominal
    now asserts. That case can only be a wrong bound: a capacity below the
    bound is the caller's, and a rate past the headroom is outside what the
    bound assumes. Release builds are unchanged
    ([dll_core.h](native/inc/doppler/dll/dll_core.h), #3282).
