/**
 * @file dp_event_log_core.h
 * @brief A run's events as SigMF annotations: appended live, finalized at close.
 *
 * A pool of receivers produces *transitions* — seeded, tracking, degraded,
 * lost, released, a gap in the input stream.  Each one is a fact about a span
 * of the SAMPLE stream, which is the only index the DSP below has
 * (docs/design/async-dsss-receiver.md §2.2): a record carries a stream
 * position, never a time.  SigMF says the same thing in a standard vocabulary
 * — an annotation is `core:sample_start` plus `core:sample_count` — so the
 * events of a run ARE a SigMF `annotations` array, with the receiver's own
 * fields under a `doppler:` namespace.
 *
 * @section evlog_two_files One shape cannot do both jobs
 * A `.sigmf-meta` is ONE JSON document: `global`, `captures`, `annotations`,
 * closed braces and all.  A run that emits events for hours cannot keep
 * rewriting it, and a run that is killed has written nothing.  So this object
 * keeps the two jobs apart:
 *
 *   - **During the run** each event is appended to a flat file as one JSON
 *     object on one line (JSON Lines), flushed immediately.  That file is
 *     tail-able while the run is live, and a crash costs at most the event
 *     being written, never the ones before it.
 *   - **At finalize** the lines are collected into the `annotations` array of
 *     a proper `.sigmf-meta` sidecar — through the writer's existing SigMF
 *     emitter (wfm_sigmf_meta_json_ex(), wfm_writer/wfm_writer_core.h), never
 *     a second one.  `global` and `captures` therefore come out byte-for-byte
 *     the way every other doppler sidecar spells them, including the
 *     omit-when-unknown rules that document says at length.
 *
 * Finalize reads the flat file rather than a memory copy, so the sidecar for
 * a run that died can be written afterwards by
 * dp_event_log_write_meta() with no live object at all.
 *
 * @section evlog_fields The fields of one event
 * The span and the label are the annotation.  Everything the holder knows
 * about the emitter — its id, its state, its C/N0, its Doppler — is staged
 * first with dp_event_log_field() / dp_event_log_field_str() and consumed by
 * the next dp_event_log_append(), which renders each one as `doppler:<name>`
 * and clears the table.  Staging rather than a parameter list is what keeps
 * this object ignorant of any particular receiver's record: the fields are
 * whatever the holder has, and the table is fixed-size, so nothing allocates
 * per event (§11.5 — the pool runs for hours).
 *
 * @section evlog_freq Frequency edges are omitted, never guessed
 * `core:freq_lower_edge` / `core:freq_upper_edge` are ABSOLUTE frequencies, so
 * they need the channel's centre — which a BLUE header carries and a NATS
 * frame does not.  An event states its offset from that centre and the width
 * it occupies; the absolute edges are emitted only when the centre is known
 * (dp_event_log_open()'s @p fc), and the offset and width are recorded as
 * `doppler:freq_hz` / `doppler:bandwidth_hz` either way, so nothing the caller
 * knew is lost when the centre is not known.
 *
 */

#ifndef DP_EVENT_LOG_CORE_H
#define DP_EVENT_LOG_CORE_H

#include <stddef.h>
#include <stdint.h>

#include "clib_common.h" /* DP_OK, DP_ERR_INVALID, DP_ERR_SEND */
#include "wfm_writer/wfm_writer_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Fields stageable for one event.  Staging a 17th fails rather than drops. */
#define DP_EVENT_LOG_MAX_FIELDS 16
/** Maximum staged field name length, including the NUL terminator. */
#define DP_EVENT_LOG_NAME_MAX 32
/** Maximum staged string VALUE length, including the NUL terminator. */
#define DP_EVENT_LOG_STR_MAX 64

/** Opaque event log; see dp_event_log_open(). */
typedef struct dp_event_log dp_event_log_t;

/**
 * @brief jm's spelling of ::dp_event_log_t.
 *
 * The same one-line bridge dp_tlm_core.h and dp_tlm_capture_core.h carry, for
 * the same reason: jm derives an object's state struct as
 * `<component>_state_t` with no override (just-makeit#797), and a log is an
 * opaque handle whose C name follows this library's own convention.  The
 * alias costs nothing at runtime and goes away when jm#797 lands
 * `state_type`.
 */
typedef dp_event_log_t dp_event_log_state_t;

/**
 * @brief Opens (truncating) the flat event file for a run.
 *
 * Truncates, like dp_tlm_capture_open(): a log names one run, and appending
 * to a previous run's file would merge two runs into one sidecar with no way
 * for a reader to tell.  To finalize a file this process did not write — the
 * log of a run that crashed — use dp_event_log_write_meta(), which needs no
 * log object.
 *
 * @param path Flat event file, truncated if it exists.  One JSON object per
 *             line, flushed per event, so it can be tailed live.
 * @param fc   Channel centre frequency (Hz), or ::WFM_FC_NONE (0.0) when the
 *             input does not state one — a NATS stream, typically.  It
 *             decides two things together, which is why it is one argument
 *             and not two: whether an event can carry absolute
 *             `core:freq_*_edge` keys, and what `captures[0]` reports as
 *             `core:frequency`.
 * @return New log, or NULL on a NULL/empty @p path, an unopenable @p path, or
 *         allocation failure.
 *
 * @code
 * >>> import json, os, tempfile
 * >>> from doppler.telemetry import EventLog
 * >>> d = tempfile.mkdtemp()
 * >>> log = EventLog(os.path.join(d, "run.events"), fc=2.4e9)
 * >>> log.field("emitter", 3)
 * >>> log.field_str("state", "tracking")
 * >>> log.append(48000, "seeded", bandwidth_hz=4.0e6)
 * >>> log.finalize(os.path.join(d, "run.sigmf-meta"), fs=10e6)
 * >>> m = json.load(open(os.path.join(d, "run.sigmf-meta")))
 * >>> m["annotations"][0]["core:sample_start"]
 * 48000
 * >>> m["annotations"][0]["doppler:state"]
 * 'tracking'
 * >>> log.close()
 *
 * @endcode
 */
dp_event_log_t *dp_event_log_open (const char *path, double fc);

/**
 * @brief Closes the flat file, keeping the object readable.
 *
 * Idempotent — a second call is ::DP_OK and does nothing.  Separate from the
 * destructor because a close can FAIL (the last buffered bytes meeting a full
 * disk) and a caller is entitled to hear about it; the destructor cannot
 * return.
 *
 * @param log The log.  NULL is ::DP_ERR_INVALID.
 * @return ::DP_OK, or ::DP_ERR_SEND if a write or the close itself failed at
 *         any point during the run (the error is sticky: an append that could
 *         not reach the disk is reported here even if later ones succeeded).
 */
int dp_event_log_close (dp_event_log_t *log);

/**
 * @brief Closes if still open, then frees.  NULL is a no-op.
 *
 * Returns what dp_event_log_close() would have: a caller must not learn more
 * from asking than from letting the object fall out of scope, and this is the
 * same one condition reported twice rather than two verdicts free to drift.
 *
 * @param log The log, or NULL.
 * @return ::DP_OK, or ::DP_ERR_SEND if any event failed to reach the disk.
 *         NULL is ::DP_OK — freeing nothing cannot fail.
 */
int dp_event_log_destroy (dp_event_log_t *log);

/**
 * @brief Stages a numeric field for the next event.
 *
 * Rendered as `doppler:<name>` in the annotation, then cleared.  Integral
 * values print as integers (`3`, not `3.0`), which is what a reader expects
 * of an emitter id.
 *
 * @param log   The log.
 * @param name  Field name, without the namespace — `"emitter"`, not
 *              `"doppler:emitter"`.  Up to 31 bytes.
 * @param value The value.  A non-finite value is refused rather than written:
 *              JSON has no NaN, and a sidecar that cannot be parsed is worse
 *              than a missing field.
 * @return ::DP_OK, or ::DP_ERR_INVALID on NULL, an over-long or empty name, a
 *         non-finite value, or a full staging table (16 fields).
 *
 * @code
 * >>> import os, tempfile
 * >>> from doppler.telemetry import EventLog
 * >>> d = tempfile.mkdtemp()
 * >>> log = EventLog(os.path.join(d, "run.events"))
 * >>> log.field("cn0_db_hz", 47.5)
 * >>> log.field("emitter", 3)          # integral: renders as 3
 * >>> log.append(1024, "tracking")
 * >>> log.count
 * 1
 * >>> log.close()
 *
 * @endcode
 */
int dp_event_log_field (dp_event_log_t *log, const char *name, double value);

/**
 * @brief Stages a string field for the next event.
 *
 * The string face of dp_event_log_field(), for the fields that are names
 * rather than numbers — a state, a reason, a code.
 *
 * @param log   The log.
 * @param name  Field name, without the namespace.
 * @param value The value; copied, up to 63 bytes.
 * @return ::DP_OK, or ::DP_ERR_INVALID on NULL, an over-long or empty name,
 *         an over-long value, or a full staging table.
 *
 * @code
 * >>> import json, os, tempfile
 * >>> from doppler.telemetry import EventLog
 * >>> d = tempfile.mkdtemp()
 * >>> log = EventLog(os.path.join(d, "run.events"))
 * >>> log.field_str("state", "tracking")
 * >>> log.append(1024, "seeded")
 * >>> log.close()
 * >>> line = open(os.path.join(d, "run.events")).readline()
 * >>> json.loads(line)["doppler:state"]
 * 'tracking'
 *
 * @endcode
 */
int dp_event_log_field_str (dp_event_log_t *log, const char *name,
                            const char *value);

/**
 * @brief Appends one event and consumes the staged fields.
 *
 * Writes one JSON object on one line and flushes it, so a reader tailing the
 * file sees the event as it happens and a crash cannot cost an earlier one.
 * The staged fields are cleared whether or not the write succeeded — an event
 * that failed to reach the disk must not leak its fields into the next one.
 *
 * @param log           The log.
 * @param sample_start  Stream position of the event → `core:sample_start`.
 * @param label         `core:label` — `"seeded"`, `"tracking"`, `"lost"`,
 *                      `"gap"`, whatever the holder calls it.  It comes
 *                      before the span, and has no default, because an
 *                      unlabelled transition is a position with nothing said
 *                      about it: the label is the event.
 * @param sample_count  Span in samples → `core:sample_count`.  0 means an
 *                      INSTANT and the key is omitted, which is the honest
 *                      spelling: a transition happens at a sample, and a
 *                      written `0` would claim a measured span of nothing.
 * @param freq_hz       Offset from the channel centre (Hz), positive above.
 * @param bandwidth_hz  Occupied width (Hz).  <= 0.0 means "no band stated",
 *                      and then neither the edges nor `doppler:freq_hz`
 *                      appear: an event like a stream gap has no frequency,
 *                      and a 0 Hz offset written for it would read as an
 *                      on-centre emitter.
 * @return ::DP_OK, ::DP_ERR_INVALID on NULL or a closed log, or ::DP_ERR_SEND
 *         if the line could not be written.
 *
 * @code
 * >>> import json, os, tempfile
 * >>> from doppler.telemetry import EventLog
 * >>> d = tempfile.mkdtemp()
 * >>> log = EventLog(os.path.join(d, "run.events"))
 * >>> log.append(48000, "seeded")            # an instant
 * >>> log.append(96000, "gap", sample_count=1024)   # a span
 * >>> log.close()
 * >>> rows = [json.loads(x) for x in
 * ...         open(os.path.join(d, "run.events"))]
 * >>> "core:sample_count" in rows[0], rows[1]["core:sample_count"]
 * (False, 1024)
 *
 * @endcode
 */
int dp_event_log_append (dp_event_log_t *log, uint64_t sample_start,
                         const char *label, uint64_t sample_count,
                         double freq_hz, double bandwidth_hz);

/** @brief Events appended so far (successfully written). */
size_t dp_event_log_count (const dp_event_log_t *log);

/**
 * @brief Writes the `.sigmf-meta` sidecar for this log's events.
 *
 * Flushes the flat file and renders it through dp_event_log_write_meta(),
 * with this log's own path and @p fc.  The log stays open and usable
 * afterwards: a long run can emit a sidecar per hour and keep going.
 *
 * @param log         The log.
 * @param meta_path   Sidecar to write, conventionally `<base>.sigmf-meta`.
 * @param sample_type Dataset wire type (wavegen order) → `core:datatype`.
 * @param endian      0 little, 1 big.
 * @param fs          Sample rate (Hz), or 0.0 to leave `core:sample_rate`
 *                    unstated.  The dataset and the telemetry file come from
 *                    dp_event_log_set_dataset() / _set_telemetry().
 * @param t0_unix_sec Capture start in UNIX seconds, or ::WFM_TIMECODE_UNSET
 *                    (0.0) → `captures[0]."core:datetime"`, omitted when
 *                    unset.
 * @return ::DP_OK, ::DP_ERR_INVALID on NULL arguments, ::DP_ERR_SEND on a
 *         read/write failure, or ::DP_ERR_MEMORY.
 *
 * @code
 * >>> import json, os, tempfile
 * >>> from doppler.telemetry import EventLog
 * >>> d = tempfile.mkdtemp()
 * >>> log = EventLog(os.path.join(d, "run.events"), fc=2.4e9)
 * >>> log.append(48000, "seeded", bandwidth_hz=4.0e6)
 * >>> meta = os.path.join(d, "run.sigmf-meta")
 * >>> log.finalize(meta, fs=1.0e7)
 * >>> log.close()
 * >>> json.load(open(meta))["captures"][0]["core:frequency"]
 * 2400000000
 *
 * @endcode
 */
int dp_event_log_finalize (dp_event_log_t *log, const char *meta_path,
                           int sample_type, int endian, double fs,
                           double t0_unix_sec);

/**
 * @brief Names the sample file these events index.
 *
 * A property of the RUN, not of a sidecar, which is why it is set once here
 * rather than passed to every dp_event_log_finalize() — a long run writes a
 * sidecar an hour and the dataset does not change between them.
 *
 * Unset (the default) means there is nothing on disk to point at — a live
 * NATS stream that nobody recorded — and the sidecar then says
 * `core:metadata_only`, which is SigMF's own word for it rather than an
 * absent key a reader has to interpret.
 *
 * @param log  The log.
 * @param name Dataset basename, copied.  NULL or empty restores "none".
 * @return ::DP_OK, or ::DP_ERR_INVALID on a NULL log.
 *
 * @code
 * >>> import json, os, tempfile
 * >>> from doppler.telemetry import EventLog
 * >>> d = tempfile.mkdtemp()
 * >>> log = EventLog(os.path.join(d, "run.events"))
 * >>> log.set_dataset("capture.sigmf-data")
 * >>> log.append(0, "seeded")
 * >>> meta = os.path.join(d, "run.sigmf-meta")
 * >>> log.finalize(meta)
 * >>> log.close()
 * >>> json.load(open(meta))["global"]["core:dataset"]
 * 'capture.sigmf-data'
 *
 * @endcode
 */
int dp_event_log_set_dataset (dp_event_log_t *log, const char *name);

/**
 * @brief Names the `dp_tlm` record file written for the same run.
 *
 * Carried with its record dtype under a `doppler:telemetry` global, so one
 * sidecar indexes all three products of a run — the dataset, the events, the
 * telemetry — each in the format that suits its rate: annotations for
 * transitions at a handful a minute, a flat record file for a time series at
 * thousands a second (§8.1).
 *
 * @param log  The log.
 * @param path Telemetry record file, copied.  NULL or empty restores "none".
 * @return ::DP_OK, or ::DP_ERR_INVALID on a NULL log.
 *
 * @code
 * >>> import json, os, tempfile
 * >>> from doppler.telemetry import EventLog
 * >>> d = tempfile.mkdtemp()
 * >>> log = EventLog(os.path.join(d, "run.events"))
 * >>> log.set_telemetry("run.tlm")
 * >>> log.append(0, "seeded")
 * >>> meta = os.path.join(d, "run.sigmf-meta")
 * >>> log.finalize(meta)
 * >>> log.close()
 * >>> json.load(open(meta))["global"]["doppler:telemetry"]["path"]
 * 'run.tlm'
 *
 * @endcode
 */
int dp_event_log_set_telemetry (dp_event_log_t *log, const char *path);

/**
 * @brief Renders any flat event file into a `.sigmf-meta` sidecar.
 *
 * The finalize step with no live log, which is what makes the flat file worth
 * having: the sidecar for a run that was killed is written afterwards, from
 * the file on disk, by a different process if need be.
 * dp_event_log_finalize() is this function with the log's own path and centre
 * frequency.
 *
 * A line the parser rejects is SKIPPED, not fatal: a killed run leaves a
 * truncated last line, and refusing to describe the hours before it would
 * lose the whole run to its final millisecond.
 *
 * @param log_path    Flat event file to read.
 * @param meta_path   Sidecar to write.
 * @param sample_type Dataset wire type (wavegen order) → `core:datatype`.
 * @param endian      0 little, 1 big.
 * @param fs          Sample rate (Hz), 0.0 leaves it unstated.
 * @param fc          Channel centre (Hz), 0.0 leaves `core:frequency`
 *                    unstated.  The edges inside the annotations were decided
 *                    when they were appended and are copied through as they
 *                    stand.
 * @param t0_unix_sec Capture start in UNIX seconds, or 0.0.
 * @param dataset     Dataset basename, or NULL for `core:metadata_only`.
 * @param telemetry   `dp_tlm` record file for the same run, or NULL.
 * @return ::DP_OK, ::DP_ERR_INVALID on NULL arguments, ::DP_ERR_SEND if the
 *         log could not be read or the sidecar written, or ::DP_ERR_MEMORY.
 */
int dp_event_log_write_meta (const char *log_path, const char *meta_path,
                             int sample_type, int endian, double fs, double fc,
                             double t0_unix_sec, const char *dataset,
                             const char *telemetry);

#ifdef __cplusplus
}
#endif

#endif /* DP_EVENT_LOG_CORE_H */
