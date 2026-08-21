- **`make nats-down` removed the JetStream store while the broker was still
    writing to it**, and failed the cleanup step of jobs whose tests had
    passed:

    ```text
    rm: cannot remove '.../streams/DP_WORK_ep752264292/msgs': Directory not empty
    ```

    `kill` returns when the signal is *queued*, not when the process is gone.
    Measured against a real nats-server: it stayed alive for roughly nine
    hundred further `kill -0` polls after `kill` returned, and a JetStream
    shutdown spends that window flushing stream state to disk. The `rm -rf`
    ran straight into it — emptying a stream's `msgs/` directory and then
    failing its `rmdir` on a file the broker had re-created in between.

    The recipe now stops every writer, **waits for it to be gone** (SIGTERM,
    then SIGKILL after 10s), and only then removes the store; the container
    comes down before the store too, so a future bind mount cannot quietly
    reintroduce this. The removal itself can no longer fail the target — it
    runs under `if: always()`, so a surviving temp directory is worth a
    warning and is not worth turning a red test run into a red cleanup.

    `scripts/start-nats.sh` lost the same race in the other direction, where
    `set -e` made a "Directory not empty" abort the *start*. It now retries
    and then insists on an empty store, saying which command to run.

    Gated by `src/doppler/tests/test_nats_teardown.py`, which drives the real
    recipe against a stand-in that keeps writing through SIGTERM. Deleting
    the wait loop turns it red.
