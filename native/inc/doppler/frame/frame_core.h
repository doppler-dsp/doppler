/**
 * @file frame_core.h
 * @brief A frame's bit layout, held as an object so Python can describe one.
 *
 * This is the RECEIVE half of the frame story. A frame description
 * (`wfm_frame_desc_t`, `wfm/wfm_frame.h`) is what a generator builds a frame
 * from and what `dp_wfm_frame_desc_crc_ok()` scores a received one against,
 * and until now only C could hold one — so `ber`'s frame meter, which exists
 * precisely to turn CRC outcomes into an exact error-rate interval, had no
 * way to be fed from the language most captures are analysed in.
 *
 * ## It owns NO layout
 *
 * Every decision — where the CRC sits, that it covers the payload alone and
 * nothing else, that a repeated preamble repeats the SAME bits — stays in
 * `wfm_frame.c`. This object is lifecycle and delegation: it copies the
 * caller's literal arrays so the description outlives the call that made it,
 * describes them with `dp_wfm_frame_fixed()`, materialises the frame once,
 * and hands everything else to `dp_wfm_frame_desc_layout()` /
 * `dp_wfm_frame_assemble()` / `dp_wfm_frame_desc_crc_ok()`. Re-deriving any
 * of it here would rebuild exactly the TX/RX drift the description was
 * introduced to stop.
 *
 * ## It takes BITS
 *
 * Each field is an unpacked bit array, one bit per byte, and nothing else:
 * no kind, no generator parameters, no repetition count. Every other form
 * reaches it through a helper that returns bits -- `field_bits()` for the
 * Field text form (`pn:1023:10`, `0x1ACFFC1D`, `*4`), `cvt`'s `hex_to_bin`
 * and `bytes_to_bin` for hex and packed octets -- so this object has one
 * constructor shape and no dispatch (docs/design/frame-description.md §F.3).
 * An element that is not 0 or 1 is REFUSED rather than masked: a byte of 101
 * is what a digit string becomes when it is passed where bits belong, and
 * masking would make that mistake a valid-looking field.
 *
 * ## The frame is materialised at CREATE
 *
 * `dp_frame_create()` builds the bits immediately and returns NULL if they
 * cannot be built (an element that is not a bit, an empty geometry). A frame
 * that cannot be materialised is not a frame, and finding that out at
 * construction is what lets the binding raise something better than a
 * failure three calls later.
 *
 * @code
 * // Barker-13 sync over a 16-bit literal payload, with a CRC-16 trailer.
 * static const uint8_t sync[13]  = {1,1,1,1,1,0,0,1,1,0,1,0,1};
 * static const uint8_t pay[16]   = {0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1};
 * dp_frame_state_t *f = dp_frame_create(NULL, 0,     // no preamble
 *                                       sync, 13,
 *                                       pay, 16,
 *                                       1);           // crc16
 * uint8_t *b = malloc(dp_frame_bits_max_out(f, 1));
 * size_t   n = dp_frame_bits(f, 1, b, f->nbits);     // 13 + 16 + 16 == 45
 * dp_frame_crc_ok(f, b, n);                           // 1 -- its own truth
 * free(b);
 * dp_frame_destroy(f);
 * @endcode
 *
 * @see docs/design/rx-test.md section 7
 */
#ifndef DP_FRAME_CORE_H
#define DP_FRAME_CORE_H

#include "doppler/clib_common.h"
#include "doppler/jm_perf.h"
#include "doppler/pn/pn_core.h"
#include "doppler/gold/gold_core.h"
#include "doppler/wfm/wfm_frame.h" /* the descriptor and its layout — the one SSOT */
#include "doppler/conv/conv_core.h"
#include "doppler/rs/rs_core.h"
#include "doppler/cvt/cvt_core.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Frame state.
 *
 * Allocate with dp_frame_create().
 */
typedef struct {
    /** The DESCRIPTION — fields and stages — which is what everything here
        delegates on. The constructor describes the common frame through
        `dp_wfm_frame_fixed()` and the builder appends field by field, so the
        two produce the same kind of thing and share every method below. Its `bits`
        pointers address the owned copies, never the caller's arrays: a Python
        buffer is released the moment the call that supplied it returns. */
    wfm_frame_desc_t d;
    /** The general layout, derived at build. */
    wfm_frame_desc_layout_t dl;
    /** What the last dp_frame_deframe() found, summed across the stages it
        reversed. Read-backs rather than a returned record: the call hands
        back BITS, and jm binds one return value. `rx_checked == 0` means
        the description carries no reversible stage — which is not the same
        fact as a failed check, and an FER conflating them would score every
        unprotected frame as an error. */
    int rx_checked;
    int rx_units;
    int rx_ok;
    int rx_symbols;
    /** Owned copies of every literal field; NULL for a generated kind. */
    uint8_t *own[WFM_FRAME_MAX_FIELDS];
    /** One materialised frame, built at create (configured) or at `build()`
        (described) — which is also the proof the description CAN be
        materialised. `bits()` repeats this rather than regenerating, so every
        repeat is bit-identical by construction and a PN field cannot advance
        its register between them. */
    uint8_t *one;
    /** Non-zero once the description is fixed: set by @ref dp_frame_create
        and by a successful @ref dp_frame_build. It is NOT `one != NULL`,
        because a description with a data field is built, laid out and
        checkable, yet has no single frame to hold -- its data field has no
        bits of its own, so `one` stays NULL and @ref dp_frame_bits writes
        none. */
    int built;
/*<<property_struct_fields>>*/
  size_t nbits;
} dp_frame_state_t;

/**
 * @brief Create a frame instance.
 *
 * Three fields as bits, each optional, and the CRC. An omitted field is
 * absent, which is `wfm_seq_t`'s own spelling of absence (a zero length).
 *
 * @param preamble      Preamble bits, one per element, each 0 or 1; may be
 *                      empty. A repeated preamble is repeated in its bits.
 * @param preamble_len  Its length in bits.
 * @param sync          Sync-word bits; may be empty.
 * @param sync_len      Its length in bits.
 * @param payload       Payload bits; may be empty.
 * @param payload_len   Its length in bits.
 * @param crc           Enum index; 0=none, 1=crc16 over the payload.
 * @return Heap-allocated state, or NULL if the geometry is empty or an
 *         element is not a bit -- the frame is refused rather than
 *         half-honoured.
 * @note Caller must call dp_frame_destroy() when done.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.wfm import Frame
 * >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)   # Barker-13
 * >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
 * >>> f = Frame(sync=sync, payload=payload, crc="crc16")
 * >>> f.nbits                                          # 13 + 16 + 16
 * 45
 * >>> f.field_off(f.field_index("payload"))
 * 13
 * >>> f.crc_ok(f.bits())        # its own bits are its own truth
 * 1
 *
 * @endcode
 */
dp_frame_state_t *dp_frame_create(const uint8_t *preamble, size_t preamble_len, const uint8_t *sync, size_t sync_len, const uint8_t *payload, size_t payload_len, int crc);

/**
 * @brief Destroy a frame instance and release all memory.
 * @param state  May be NULL.
 */
void dp_frame_destroy(dp_frame_state_t *state);

/**
 * @brief Bits @ref dp_frame_bits will write for @p n frames — `n * nbits`.
 *
 * @param state  The frame.
 * @param n      Frame repetitions.
 */
size_t dp_frame_bits_max_out(dp_frame_state_t *state, size_t n);

/**
 * @brief Materialise @p n consecutive frames, one bit per byte.
 *
 * @p n counts FRAMES, not bits: a descriptor describes one frame, and a
 * capture holds many. It is the truth for a transmitter that sends the same
 * frame n times -- a data source of n copies of the payload, one chunk a
 * frame -- so a stream compared against this lines up with the one that was
 * transmitted.
 *
 * @param state    The frame.
 * @param n        Frame repetitions.
 * @param out      Output, one bit per byte.
 * @param max_out  Capacity of @p out; the write is truncated to whole frames
 *                 that fit rather than overrunning.
 * @return Bits written.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.wfm import FrameDesc
 * >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
 * >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
 * >>> d = FrameDesc(sync=sync, payload=payload, crc="crc16")
 * >>> d.build()
 * >>> len(d.bits())        # one frame: 13 + 16 + 16
 * 45
 * >>> len(d.bits(2))       # n counts FRAMES, tiled the way a capture is
 * 90
 *
 * @endcode
 */
size_t dp_frame_bits(dp_frame_state_t *state, size_t n, uint8_t *out, size_t max_out);

/**
 * @brief Check one received frame's CRC.
 *
 * **This is what makes a truth-free frame error rate possible.** It needs no
 * payload truth at all, so it works on a real capture, and unlike a
 * self-referenced EVM or a blind M2M4 it still catches a false lock — a
 * rotated constellation fails the check rather than looking clean.
 *
 * @param state        The frame the bits are laid out by.
 * @param rx_bits      Received bits, one per byte.
 * @param rx_bits_len  How many; must be at least @ref dp_frame_state_t::nbits.
 * @return 1 pass, 0 fail, -1 if the frame carries no CRC or @p rx_bits is
 *         shorter than one frame.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.wfm import FrameDesc
 * >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
 * >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
 * >>> d = FrameDesc(sync=sync, payload=payload, crc="crc16")
 * >>> d.build()
 * >>> d.crc_ok(d.bits())           # its own bits are its own truth
 * 1
 * >>> rx = np.asarray(d.bits()).copy()
 * >>> rx[d.field_off(d.field_index("payload"))] ^= 1   # one payload bit
 * >>> d.crc_ok(rx)
 * 0
 *
 * @endcode
 */
int dp_frame_crc_ok(dp_frame_state_t *state, const uint8_t *rx_bits, size_t rx_bits_len);

/**
 * @brief The same frame, DEFERRED — a description a caller can extend.
 *
 * Every argument @ref dp_frame_create takes, and the flavor is what it does
 * with them: this one stops before materialising, so the fields are a
 * STARTING POINT rather than a finished frame. Append with
 * @ref dp_frame_add_field, @ref dp_frame_add_derived and
 * @ref dp_frame_add_stage_over, then @ref dp_frame_build. Omit all three
 * fields to begin from nothing.
 *
 * That is what makes a frame doppler has never heard of describable — a
 * CCSDS CADU among them — without a constructor argument per field of a
 * fixed list: appending is how a fifth field is added without a signature
 * change.
 *
 * It is also what makes the CCSDS coding reachable from Python at all.
 * `ccsds_tm` has no binding and is not getting one, so a caller meets the
 * outer code, the randomiser and the inner code by DESCRIBING a CADU rather
 * than through a CCSDS entry point added here.
 *
 * An empty description is legal here and refused by @ref dp_frame_create, and
 * the difference is where completeness can be judged: that constructor's
 * description is complete when it returns, and this one is not complete until
 * @ref dp_frame_build is called.
 *
 * Read either one through @ref dp_frame_field_index and the indexed
 * accessors beside it, @ref dp_frame_field_off and its siblings.
 *
 * @return An unbuilt description, or NULL if an element is not a bit.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.wfm import FrameDesc, STAGE_CRC16
 * >>> d = FrameDesc()                             # begin from nothing
 * >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)  # Barker-13
 * >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
 * >>> d.add_field("sync", sync)                   # returns its index
 * 0
 * >>> d.add_field("payload", payload)
 * 1
 * >>> d.add_derived("crc", 16)                    # a stage will fill it
 * 2
 * >>> d.add_stage_over(STAGE_CRC16, "payload", "crc")
 * 0
 * >>> d.build()
 * >>> d.nbits                                     # 13 + 16 + 16
 * 45
 * >>> d.crc_ok(d.bits())        # its own bits are its own truth
 * 1
 *
 * @endcode
 */
dp_frame_state_t *dp_frame_create_desc(const uint8_t *preamble, size_t preamble_len, const uint8_t *sync, size_t sync_len, const uint8_t *payload, size_t payload_len, int crc);

/**
 * @brief Append one named field to a description. Returns its index; -1 in
 * C, `ValueError` from Python.
 *
 * The field is bits and nothing else, copied here so the description
 * outlives the call. A field a STAGE fills is appended with
 * @ref dp_frame_add_derived instead, because the caller has no bits for it.
 *
 * @param state     A frame from @ref dp_frame_create_desc.
 * @param name      The field's name, or NULL/"" for anonymous; a name
 *                  another field carries is refused.
 * @param bits      The bits, one per element, each 0 or 1.
 * @param bits_len  How many; 0 is refused (an empty field is no field).
 * @return The new field's index, or -1 if the description is full or
 *         already built, the name is taken, or an element is not a bit.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.wfm import FrameDesc
 * >>> from doppler.ccsds import asm_bits
 * >>> octets = np.array([(i * 29 + 5) & 0xFF for i in range(223)],
 * ...                   np.uint8)
 * >>> d = FrameDesc()                      # begin from nothing
 * >>> d.add_field("asm", asm_bits())       # the attached sync marker
 * 0
 * >>> d.add_field("data", np.unpackbits(octets))   # the transfer frame
 * 1
 * >>> d.field_index("data")
 * 1
 *
 * @endcode
 */
int dp_frame_add_field(dp_frame_state_t *state, const char *name, const uint8_t *bits, size_t bits_len);

/**
 * @brief Append one stage, and the span of fields it covers.
 *
 * @p n_fields is the load-bearing part and 0 means the stage does not run.
 * A stage that inherited "everything before me" instead of declaring its
 * cover is the representation that cannot express a CCSDS CADU — see
 * `wfm/wfm_frame.h`.
 *
 * @param state        A frame from @ref dp_frame_create_desc.
 * @param kind         stage kind: a @ref wfm_stage_kind_t value
 *                     (0=crc16…4=interleave), or a caller's own from
 *                     `WFM_STAGE_USER` (0x1000) up, whose kernel then
 *                     has to reach the assembler through its ops table.
 * @param first_field  First field covered.
 * @param n_fields     Fields covered; 0 = the stage does not run.
 * @param depth        Interleaving depth, for an outer code.
 * @param emit_num     Expansion numerator for a stage that emits a NEW
 *                     stream; 0 when the stage stays inside the frame.
 * @param emit_den     Expansion denominator.
 * @param unit_bits    INTERLEAVE only: bits per interleaved unit; 0 reads
 *                     as 1. Match it to the outer code's symbol — permuting
 *                     octets is what spreads a burst across the codewords of
 *                     a code over GF(256), and permuting bits inside one
 *                     spreads a burst within a symbol that is already wrong.
 * @return The new stage's index, or -1 if the description is full or already
 *         built. The Python binding raises `ValueError` rather than
 *         handing back the -1.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.wfm import FrameDesc
 * >>> from doppler.ccsds import asm_bits
 * >>> octets = np.array([(i * 29 + 5) & 0xFF for i in range(223)],
 * ...                   np.uint8)
 * >>> d = FrameDesc()
 * >>> _ = d.add_field("asm", asm_bits())
 * >>> _ = d.add_field("data", np.unpackbits(octets))
 * >>> _ = d.add_derived("parity", 32 * 8)   # the outer code fills it
 * >>> d.add_stage(1, first_field=1, n_fields=2, depth=1)   # RS(255,223)
 * 0
 * >>> d.add_stage(2, first_field=1, n_fields=2)            # randomiser
 * 1
 *
 * Both start at field 1, so both skip the marker -- the cover is DECLARED,
 * which is the whole reason a CADU is describable here:
 *
 * >>> d.build()
 * >>> d.stage_first(0), d.stage_bits(0)
 * (32, 2040)
 *
 * @endcode
 */
int dp_frame_add_stage(dp_frame_state_t *state, int kind, uint32_t first_field,
                    uint32_t n_fields, uint32_t depth, uint32_t emit_num,
                    uint32_t emit_den, uint32_t unit_bits);

/**
 * @brief Lay out and materialise a described frame.
 *
 * The point at which a description is checked, which for @ref dp_frame_create
 * happens inside the constructor: a description that cannot produce its own
 * bits is not a frame. It is separate here only because the description
 * arrives over several calls and there is no earlier moment at which it is
 * complete.
 *
 * The CRC, the outer code, the randomiser and the inner code are all
 * runnable: `ccsds_tm` has no Python binding and is not getting one, so this
 * object is where a caller meets them. A stage naming a kernel nothing here
 * carries is refused rather than skipped, because a stage that quietly did
 * not run produces a frame that still assembles and syncs to nothing.
 *
 * A description with a data field (@ref dp_frame_add_data) builds too: the
 * field has no bits until a source draws them, so the description is laid out
 * from its lengths and its stages are proved runnable over a chunk that is
 * thrown away. It then has no single frame, so @ref dp_frame_bits writes
 * none, while @ref dp_frame_deframe and @ref dp_frame_check work as for any
 * other -- a receiver needs the data field's length and nothing else.
 *
 * The inner encoder starts from the all-zero register on every build: a
 * description describes ONE frame. A stream of CADUs sharing one register is
 * a transmitter's job and lives in `dp_ccsds_tm_frame_encode`.
 *
 * @param state  A frame from @ref dp_frame_create_desc.
 * @return 0 on success, -1 if the description is empty, unbuildable, names a
 *         stage with no kernel here, or was already built. The Python
 *         binding raises `ValueError` and returns nothing.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.wfm import FrameDesc
 * >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
 * >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
 * >>> d = FrameDesc(sync=sync, payload=payload, crc="crc16")
 * >>> d.build()
 * >>> d.nbits                     # 13 + 16 + 16, laid out by build()
 * 45
 *
 * A description that cannot produce bits is not a frame, and is refused
 * rather than half-built:
 *
 * >>> FrameDesc().build()
 * Traceback (most recent call last):
 *     ...
 * ValueError: cannot build: the description is empty, unbuildable, ...
 *
 * @endcode
 */
int dp_frame_build(dp_frame_state_t *state);

/**
 * @brief Index of the field called @p name, or -1.
 *
 * The one lookup that resolves a name, so every index-taking entry point
 * keeps working unchanged and a rename can only be wrong once. An unnamed
 * field is ANONYMOUS rather than named `""`, so the empty name matches
 * nothing — including a field that has no name.
 *
 * @param state  the frame.
 * @param name   the field name.
 * @return the index, or -1 on NULL or a name no field carries. This is the
 *         one verb whose -1 survives into Python: a name that matches
 *         nothing is an ANSWER, not a refusal, so there is nothing to
 *         raise about.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.wfm import FrameDesc
 * >>> d = FrameDesc()
 * >>> d.add_field("sync", np.array([1,0,1,0,1,0,1,1,1,1,0,0], np.uint8))
 * 0
 * >>> d.field_index("sync")
 * 0
 * >>> d.field_index("absent")
 * -1
 *
 * @endcode
 */
int dp_frame_field_index(dp_frame_state_t *state, const char *name);

/**
 * @brief Give an already-appended field a name, or clear it with `""`.
 *
 * @param state  the frame.
 * @param index  the field to name.
 * @param name   the new name; truncated at `WFM_FRAME_NAME_MAX - 1`.
 * @return 0, or -1 on NULL, an out-of-range @p index, a name another field
 *         already carries, or once the frame is built. It is a command
 *         rather than a query, so the Python binding raises `ValueError`
 *         on the -1 and returns nothing on the 0.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.wfm import FrameDesc
 * >>> d = FrameDesc()
 * >>> d.add_field("", np.array([1, 0, 1, 0], np.uint8))   # anonymous
 * 0
 * >>> d.name_field(0, "payload")
 * >>> d.field_index("payload")
 * 0
 *
 * @endcode
 */
int dp_frame_name_field(dp_frame_state_t *state, uint32_t index, const char *name);

/**
 * @brief Append a named field a stage will fill. Returns its index; -1 in C,
 * `ValueError` from Python.
 *
 * A field with a declared length and no source: a CRC trailer, a block of
 * check symbols. Its producer is wired by @ref dp_frame_add_stage_over rather
 * than named here, because no stage exists yet when the field it derives is
 * appended — fields are ordered by POSITION and stages by APPLICATION.
 *
 * @param state  the frame.
 * @param name   the field's name, or NULL for anonymous.
 * @param bits   its length, which its stage decides and the caller states.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.wfm import FrameDesc
 * >>> d = FrameDesc()
 * >>> d.add_field("payload", np.array([1, 0, 1, 0], np.uint8))
 * 0
 * >>> d.add_derived("crc", 16)          # a stage will fill it
 * 1
 *
 * @endcode
 */
int dp_frame_add_derived(dp_frame_state_t *state, const char *name, size_t bits);

/**
 * @brief Append a named DATA field: @p len bits a data source fills, one
 * chunk per frame. Returns its index; -1 in C, `ValueError` from Python.
 *
 * The object spelling of the Field text `data:LEN`, and the same field:
 * a @ref WFM_SEQ_DATA sequence of length @p len, which is exactly what
 * `dp_wfm_field_parse("data:LEN")` produces for a scene's or the CLI's
 * frame. The description knows the field's length and never its bits:
 * a transmitter draws them from its data source at each frame (a source's
 * `data=`), and a CRC or outer code covering the field covers that frame's
 * chunk. It is a method rather than Field text in @ref dp_frame_add_field
 * because an object takes bits, and a data field has none
 * (rx-frame-description.md, D5).
 *
 * A frame draws from one data source, so a description carries at most
 * one data field; a second is refused where geometry is judged, by the
 * layout, as it is for every other face.
 *
 * @param state  A frame from @ref dp_frame_create_desc.
 * @param name   The field's name, or NULL/"" for anonymous; a name another
 *               field carries is refused.
 * @param len    Bits per frame, `LEN` in `data:LEN`: from 1 to
 *               @ref WFM_FIELD_MAX_BITS, the bound the Field grammar puts
 *               on the text form.
 * @return The new field's index, or -1 if @p len is out of range, the
 *         description is full or already built, or the name is taken.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.wfm import FrameDesc, STAGE_CRC16, Segment, Composer
 * >>> d = FrameDesc()
 * >>> d.add_field("sync", np.array([1, 1, 1, 0, 0, 1, 0], np.uint8))
 * 0
 * >>> d.add_data("payload", 8)          # 8 bits of the data source a frame
 * 1
 * >>> d.add_derived("crc", 16)
 * 2
 * >>> d.add_stage_over(STAGE_CRC16, "payload", "crc")
 * 0
 * >>> seg = Segment(type="bits", sps=1, modulation="bpsk", frame=d,
 * ...               data=np.unpackbits(np.array([0xA5, 0x3C], np.uint8)))
 * >>> len(Composer([seg]).compose())    # two frames of 7 + 8 + 16 bits
 * 62
 *
 * @endcode
 */
int dp_frame_add_data(dp_frame_state_t *state, const char *name, size_t len);

/**
 * @brief Append a stage covering `[first .. last]` by name.
 *
 * The cover is the load-bearing part of the representation and this is the
 * form that reads. It wires a derived field's producer for you, which
 * applies the invariant the layout already enforces rather than adding one.
 *
 * @param state      the frame.
 * @param kind       a stage kind — `doppler.wfm.STAGE_CRC16` and its
 *                   siblings, or a caller's own from `STAGE_USER` up.
 * @param first      name of the first field covered.
 * @param last       name of the last field covered; may equal @p first.
 * @param depth      RS / interleave depth; 0 when unused.
 * @param unit_bits  interleave unit; 0 reads as 1.
 * @return the new stage's index, or -1 on NULL, a full description, a name
 *         neither field carries, @p last before @p first, or once built.
 *         The Python binding raises `ValueError` rather than handing back
 *         the -1.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.wfm import FrameDesc
 * >>> d = FrameDesc()
 * >>> d.add_field("payload", np.array([0, 1, 1, 0, 1, 0, 0, 1], np.uint8))
 * 0
 * >>> d.add_derived("crc", 16)
 * 1
 * >>> d.add_stage_over(0, "payload", "crc")   # 0 = crc16
 * 0
 * >>> d.build()
 * >>> d.crc_ok(d.bits())                # its own bits are its own truth
 * 1
 *
 * @endcode
 */
int dp_frame_add_stage_over(dp_frame_state_t *state, int kind, const char *first,
                         const char *last, uint32_t depth,
                         uint32_t unit_bits);


/**
 * @brief What @ref dp_frame_check found, summed across the stages it reversed.
 *
 * One record rather than one per stage, because a caller doing frame
 * accounting wants a verdict and a cost. @p units and @p ok count CHECKS —
 * one for a CRC, one per codeword for an interleaved outer code — so
 * `ok == units` is the verdict and @p symbols is what it cost to get there.
 *
 * @p corrected and @p symbols are the honest measure of how hard a link is
 * running: `ok == units` with a rising @p symbols is margin being spent, and
 * it is spent before it is lost. A CRC cannot report that at all, which is
 * why an outer code is a strictly better detector and not merely a stronger
 * one.
 */
typedef struct {
    int      passed;    /**< every check good: 1 yes, 0 no. `passed`, not
                             `pass`: the obvious name is a Python keyword  */
    uint32_t stages;    /**< stages in the description                     */
    uint32_t checked;   /**< how many were reversed HERE (see below)       */
    uint32_t units;     /**< checks performed across them                  */
    uint32_t ok;        /**< how many came out good                        */
    uint32_t corrected; /**< how many needed and received repair           */
    uint32_t symbols;   /**< symbol errors repaired                        */
} frame_check_t;

/**
 * @brief Undo the description's stages over a received frame, and report.
 *
 * The receive mirror of @ref dp_frame_bits, reading the same description — so a
 * transmitter and a receiver holding the same `Frame` cannot disagree about
 * which stage covered what.
 *
 * **This is the truth-free frame error rate on a coded link.** It needs the
 * description and the received bits and no payload truth at all, so it works
 * on a real capture, and unlike a self-referenced EVM it still catches a
 * false lock.
 *
 * @p checked is smaller than @p stages when the description names a stage the
 * receiver does not reverse here — the inner code is the case, since it is
 * undone before frame synchronisation and a frame checker never sees channel
 * symbols. Such a stage is reported as not checked, never as passed.
 *
 * @param state        The frame the bits are laid out by.
 * @param rx_bits      Received bits, one per byte. Copied, not modified.
 * @param rx_bits_len  How many; must be at least one frame.
 * @return The outcome. @p passed is 0 and @p checked is 0 when the description
 *         carries no reversible stage at all — "carries no check" is not "the
 *         check passed", and an FER conflating them would score every
 *         unprotected frame as perfect.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.wfm import FrameDesc
 * >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
 * >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
 * >>> d = FrameDesc(sync=sync, payload=payload, crc="crc16")
 * >>> d.build()
 * >>> r = d.check(d.bits(1))
 * >>> r.passed, r.ok, r.units
 * (1, 1, 1)
 *
 * Flip a bit the CRC covers and the verdict turns over:
 *
 * >>> rx = np.asarray(d.bits(1)).copy()
 * >>> rx[d.field_off(d.field_index("payload"))] ^= 1
 * >>> d.check(rx).passed
 * 0
 *
 * Carrying no check is NOT passing one -- both are reported, separately:
 *
 * >>> n = FrameDesc(sync=sync, payload=payload, crc="none")
 * >>> n.build()
 * >>> c = n.check(n.bits(1))
 * >>> c.passed, c.checked
 * (0, 0)
 *
 * @endcode
 */
frame_check_t dp_frame_check(dp_frame_state_t *state, const uint8_t *rx_bits, size_t rx_bits_len);

/**
 * @brief Undo this description's stages over a received frame — DEFRAME it.
 *
 * The receive counterpart of building one, and the layer a receiver stops
 * short of: `DsssBurstReceiver` and friends hand back hard and soft
 * decisions for a frame's symbols and make no claim about what they mean,
 * because knowing that needs a description — this one (doppler#1022).
 *
 * Returns the frame with every reversible stage undone, in place order:
 * a randomiser XORed back, an outer code's repairs APPLIED, a CRC checked.
 * The payload is then a slice, at @ref dp_frame_field_off of the payload
 * field — which is the caller's arithmetic because a description does not
 * privilege one field over another.
 *
 * The verdict comes back as read-backs (`ok`, `units`, `checked`,
 * `symbols`), not as a return value, since the return is the bits. Read
 * them exactly as @ref frame_check_t's, including the distinction that
 * matters most: `checked == 0` says the description carries no reversible
 * stage at all, which is a different fact from a check that failed.
 *
 * A stage with no `undo` kernel — a convolutional inner code, which a
 * receiver cannot even frame-sync through — is reported as not checked
 * rather than as passed.
 *
 * @param state        The frame.
 * @param rx_bits      Received bits, `frame_bits` of them; treated as a
 *                     capture and never modified.
 * @param rx_bits_len  How many were supplied.
 * @param out          Receives the corrected frame.
 * @param max_out      Capacity of @p out; see dp_frame_deframe_max_out().
 * @return Bits written — the frame's length — or 0 if the description is
 *         empty or either buffer is too small.
 * @code
 * >>> import numpy as np
 * >>> from doppler.wfm import Frame
 * >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], dtype=np.uint8)
 * >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], dtype=np.uint8)
 * >>> f = Frame(sync=sync, payload=payload, crc="crc16")
 * >>> rx = np.asarray(f.bits())          # a clean capture of its own frame
 * >>> got = np.asarray(f.deframe(rx))
 * >>> f.rx_ok, f.rx_units, f.rx_checked  # one CRC, and it passed
 * (1, 1, 1)
 * >>> off = f.field_off(f.field_index("payload"))   # a SLICE
 * >>> bool(np.array_equal(got[off:off + 16], payload))
 * True
 * >>> rx[off] ^= 1                       # one bit flipped in flight
 * >>> _ = f.deframe(rx)
 * >>> f.rx_ok, f.rx_units                # the check notices
 * (0, 1)
 *
 * @endcode
 */
size_t dp_frame_deframe(dp_frame_state_t *state, const uint8_t *rx_bits, size_t rx_bits_len, uint8_t *out, size_t max_out);

/**
 * @brief Max bits dp_frame_deframe() writes: the frame's own length.
 *
 * Size a `deframe()` buffer with this. The bound is the DESCRIPTION's, not
 * the input's: a frame is as long as its fields say, so how many bits were
 * received does not change how many come back.
 *
 * @param state        The frame.
 * @param rx_bits_len  How many bits are on offer. Ignored, for the reason
 *                     above; it is in the signature because the binding's
 *                     capacity call passes the input's length.
 * @return The frame's length in bits, or 0 for an empty description.
 */
size_t dp_frame_deframe_max_out(dp_frame_state_t *state, size_t rx_bits_len);


/**
 * @brief Fields in the description.
 *
 * @param state  The frame.
 * @return How many fields the description carries.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.wfm import FrameDesc
 * >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
 * >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
 * >>> d = FrameDesc(sync=sync, payload=payload, crc="crc16")
 * >>> d.n_fields()          # sync, payload, crc -- no preamble was given
 * 3
 *
 * @endcode
 */
size_t dp_frame_n_fields(dp_frame_state_t *state);

/**
 * @brief Stages in the description.
 *
 * @param state  The frame.
 * @return How many stages the description carries.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.wfm import FrameDesc
 * >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
 * >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
 * >>> d = FrameDesc(sync=sync, payload=payload, crc="crc16")
 * >>> d.build()
 * >>> d.n_stages()         # the CRC is a stage like any other
 * 1
 *
 * @endcode
 */
size_t dp_frame_n_stages(dp_frame_state_t *state);

/**
 * @brief Bit offset of field @p i, or 0 if there is no such field.
 * @param state  The frame.
 * @param i      Field index.
 * @return Bits from the start of the frame.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.wfm import FrameDesc
 * >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
 * >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
 * >>> d = FrameDesc(sync=sync, payload=payload, crc="crc16")
 * >>> d.build()
 * >>> d.field_off(0), d.field_off(1), d.field_off(2)
 * (0, 13, 29)
 *
 * An absent field has no index: no preamble was given, so field 0 is the
 * sync word. Ask for a field by name rather than by position, and an index
 * past the end is 0.
 *
 * >>> d.field_off(d.field_index("crc")), d.field_off(7)
 * (29, 0)
 *
 * @endcode
 */
size_t dp_frame_field_off(dp_frame_state_t *state, size_t i);

/**
 * @brief Bits in field @p i, or 0 if there is no such field.
 * @param state  The frame.
 * @param i      Field index.
 * @return The field's length in bits.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.wfm import FrameDesc
 * >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
 * >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
 * >>> d = FrameDesc(sync=sync, payload=payload, crc="crc16")
 * >>> d.build()
 * >>> d.field_bits(0), d.field_bits(1), d.field_bits(2)
 * (13, 16, 16)
 *
 * @endcode
 */
size_t dp_frame_field_bits(dp_frame_state_t *state, size_t i);

/**
 * @brief First CADU bit stage @p i covers; 0 for a stage that did not run.
 * @param state  The frame.
 * @param i      Stage index.
 * @return Bits from the start of the frame.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.wfm import FrameDesc
 * >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
 * >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
 * >>> d = FrameDesc(sync=sync, payload=payload, crc="crc16")
 * >>> d.build()
 * >>> d.stage_first(0)     # the CRC starts at the payload, not at bit 0
 * 13
 *
 * @endcode
 */
size_t dp_frame_stage_first(dp_frame_state_t *state, size_t i);

/**
 * @brief Bits stage @p i covers; 0 for a stage that did not run.
 * @param state  The frame.
 * @param i      Stage index.
 * @return The covered span, in bits.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.wfm import FrameDesc
 * >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
 * >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
 * >>> d = FrameDesc(sync=sync, payload=payload, crc="crc16")
 * >>> d.build()
 * >>> d.stage_bits(0)      # payload+CRC: what crc16 covered
 * 32
 *
 * @endcode
 */
size_t dp_frame_stage_bits(dp_frame_state_t *state, size_t i);

#ifdef __cplusplus
}
#endif

#endif /* FRAME_CORE_H */
