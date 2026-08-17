- **The assertion ratchet compares against the merge base, not `origin/main`'s
    tip.** It asks whether *this branch* removed assertions, and the only honest
    baseline for that is where the branch started. Against the tip, a branch that
    is merely **behind** fails for assertions someone else *added* — naming a file
    it never touched, under the message "a test file LOST assertions".

    Measured twice on one branch: `test_mpsk_receiver_core.c` at −13 while `main`
    was 19 commits ahead, then `test_mpsk_core.c` at −10 after a soft-demapping
    commit landed 30 ahead. Neither file had been edited on the branch, and both
    "fixes" were a rebase — which is the tell: a gate whose verdict changes when
    you rebase is measuring the gap to `main`, and `git` already reports that.

    The cost is not the false alarm, it is the habit: a gate that cries wolf for a
    reason unrelated to the diff trains the reader to rebase-and-ignore, which is
    exactly how a real lost assertion would slip past. Its own docstring says a
    suite can go green while covering less; a ratchet can go red while nothing was
    lost, and that is the same defect from the other side.

    Proven both ways in a worktree pinned behind `main`: 51 assertions in the tree
    against 61 at the tip (**false FAIL**) and 51 at the merge base (**correctly
    quiet**), and removing three assertions there still goes red. Falls back to
    the given ref when no merge base exists, so an uncomputable baseline is not
    silently skipped. The messages now name which baseline was used.
