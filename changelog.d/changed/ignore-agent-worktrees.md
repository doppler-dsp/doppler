- **`.claude/worktrees/` is ignored**, so `make lint` (and `make train`,
    which runs it in the current checkout) no longer lints agent worktrees
    nested under the main checkout as if they were the tree.
