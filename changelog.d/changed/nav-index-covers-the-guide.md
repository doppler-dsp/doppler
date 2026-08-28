- **`check_nav_index` covers `docs/guide/` too.** It gated `design`, `dev` and
    `gallery` only, so the section with the most pages had no backstop. The
    guide nests, so containment is now the index.md *nearest* a page, and a
    subsection's own index answers to its parent — which is how it found
    `wfm-io/index.md` missing from `docs/guide/index.md` on its first run.
