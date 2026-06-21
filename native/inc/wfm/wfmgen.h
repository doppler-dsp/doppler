/*
 * wfmgen.h — the wfmgen composer CLI, exposed as a plain callable.
 *
 * `doppler_wfmgen()` is the entire body of the `wfmgen` command-line tool with
 * the process `main()` stripped off: it takes an argv vector, runs the same
 * parse → compose → write/stream pipeline, and returns the shell exit code. The
 * standalone `wfmgen` binary is a one-line `main` shim over it.
 *
 * It is archived into libdoppler so a downstream that links libdoppler.a — or
 * loads libdoppler.so — can drive the full generator without shelling out. The
 * zmq sink is statically linked, so there is no runtime libzmq dependency. And
 * because it is the exact same code path, `doppler_wfmgen(argc, argv)` is
 * byte-identical to running `wfmgen …`.
 */
#ifndef DOPPLER_WFM_WFMGEN_H
#define DOPPLER_WFM_WFMGEN_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Run the wfmgen composer CLI in-process (argv in, exit code out).
 *
 * Parses @p argv exactly as the `wfmgen` binary does (`--type`, `--count`,
 * `--from-file`, `--output`, `--record`, the container/wire/endian flags, the
 * `zmq://` sink, `--realtime` pacing, …), composes the waveform, and writes it
 * to the chosen destination (a file, stdout, or a ZMQ PUB endpoint). Output is
 * byte-identical to invoking the CLI with the same arguments — it is the same
 * code path, not a reimplementation.
 *
 * Process-global only in the ways the CLI is: it may write to @c stdout /
 * @c stderr and create the @c --output / @c --record files. It installs no
 * signal handlers, registers no `atexit` hooks, and keeps no mutable global
 * state, so it is safe to call repeatedly within one process. Not reentrant
 * across threads (it shares @c stdout).
 *
 * @param argc Argument count, including @c argv`[0]` (the program name).
 * @param argv Argument vector; @c argv`[0]` is used only in diagnostics/usage.
 * @return 0 on success; a non-zero shell exit code on a usage or I/O error
 *         (mirrors the CLI: 1 = runtime/I/O failure, 2 = bad arguments).
 *
 * @code
 * // Generate a 4096-sample QPSK capture to a file, in-process.
 * char *av[] = { "wfmgen", "--type", "qpsk", "--count", "4096",
 *                "--output", "out.cf32", NULL };
 * int rc = doppler_wfmgen(7, av);   // rc == 0; out.cf32 written
 * @endcode
 */
int doppler_wfmgen (int argc, char *argv[]);

#ifdef __cplusplus
}
#endif

#endif /* DOPPLER_WFM_WFMGEN_H */
