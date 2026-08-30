- **A header example called a function that does not exist.**
    `burst_despreader_core.h` showed `burst_despreader_step(obj, x)` — there is
    no scalar step, because the object despreads a *block*: one prompt per
    spread symbol, so a single input chip is not a symbol. The `@brief`
    lifecycle line named it too. Invisible to the arity gate by construction,
    which compares calls against functions *declared in the same header*;
    found by compiling every `@code` block under `native/inc`, which is
    [#1082](https://github.com/doppler-dsp/doppler/issues/1082)'s other half.
