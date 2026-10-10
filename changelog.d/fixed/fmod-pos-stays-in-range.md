- **A code loop no longer reads one chip past its code at a spacing just off
    a phase-grid point.** The positive fold every periodic phase in the
    library goes through could return exactly its period, outside its
    documented `[0, m)`, and the DLL's replica tap indexed the code with it
    (#2110).
