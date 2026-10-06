- **just-makeit pin 0.98.1 → 0.98.2.** The release carries the fix for
    `jm adopt` exiting 1 on a documented struct member called `name` (gh-1932,
    filed from doppler's fragment migration, #1446). `jm apply` and
    `jm upgrade` changed nothing in this tree: zero codegen drift.
