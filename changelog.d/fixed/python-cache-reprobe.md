- **A build tree follows its interpreter.** `make pyext` and the downstream
    example configure with `-U '_Python3_*'`, so CMake probes the interpreter
    again each time. A venv rebuilt on another Python keeps its path, and a
    configured tree went on building extensions for the old version, which
    the new one could not import.
