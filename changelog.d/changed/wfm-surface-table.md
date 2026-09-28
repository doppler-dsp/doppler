- **wfmgen's field flags come from one generated table.** The 28 flags that
    set a source or segment field are rows of `wfm/wfm_surface.h`, generated
    from `just-makeit.toml`, rather than hand-written rows in `wfmgen.c`.
    Behaviour is unchanged: the flag-matrix golden is byte-identical (#853).
