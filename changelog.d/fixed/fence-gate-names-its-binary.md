- **The shell fence gate said which `wfmgen` it ran.** It executes the real
    binary, so its verdict belongs to one build — and on an unbuilt tree every
    fence died as `exit 1` with an empty stderr, reading as "this documented
    flag is wrong". `which` was not enough: the console-script shim resolves
    while the binary it `execv`s is absent, so the gate probes `wfmgen --help`
    and names the resolved path on every failure.
