- **The benchmark-fixture gate's child pytest no longer walks the system
    temp directory.** Bounded only by the repo's config, its collection
    climbed every parent of its `tmp_path`. On Windows that walk stats each
    sibling it lists, so another xdist worker's `mkdtemp` directory,
    removed mid-walk, failed the test. The child now stops the walk at its
    own directory (`--confcutdir`), and a test holds that bound (#2718).
