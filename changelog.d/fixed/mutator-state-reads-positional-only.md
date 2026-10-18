- **The mutator-state gate probes positional-only methods.** Stubs that
    mark parameters positional-only with `/` (just-makeit 0.108.0) read
    as taking no parameters, so 40 methods went unprobed. The gate now
    reads them as the positional parameters they are (#2428).
