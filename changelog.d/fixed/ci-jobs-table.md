- **`docs/dev/ci.md` lists every CI job, and a gate keeps it that way.** The
    "What runs where" table named a hand-picked third of `ci.yml`'s jobs.
    It now names all of them, and `make ci-aggregator-check` fails when a job
    is missing, named twice, or does not exist (#2599).
