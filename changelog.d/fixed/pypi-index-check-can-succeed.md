- **The release's PyPI-index wait could only ever time out, and took the
    container images and the GitHub Release down with it — twice.** The
    `publish-container` job waited for the new version to be resolvable before
    building images, using

    ```sh
    if pip index versions doppler-dsp 2>/dev/null | grep -q "$VERSION"; then
    ```

    That job has **no `setup-python`**. A bare `pip` was never guaranteed to
    exist on the runner, and `2>/dev/null` turned its absence into "not
    indexed yet" — forever. So the loop had no reachable success path: v0.43.0
    timed out at 5 minutes and v0.43.1 at 15, both while the version was live
    on PyPI the entire time, and in both releases `Create GitHub Release` was
    skipped because it needs this job.

    Widening the budget from 5 to 15 minutes was treating the symptom, and
    v0.43.1 is the measurement that says so: the same failure, 900 seconds
    later. **A check that cannot succeed is not a slow check.**

    It now reads the **simple index** — the thing pip actually reads — with
    `curl`, which is always present, and matches `doppler_dsp-<version>[-.]`
    so that `0.43.1` cannot match `0.43.10` (a wheel is
    `doppler_dsp-<v>-…whl`, an sdist `doppler_dsp-<v>.tar.gz`). On timeout it
    now prints the versions the index *does* list, so the next failure says
    what it saw rather than only that it gave up.

    Verified against the live index in both directions, which is the property
    the old check never had: a real version is found, a fabricated one is not.
