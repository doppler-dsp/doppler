/**
 * @file frame_core.h
 * @brief A frame's bit layout, held as an object so Python can describe one.
 *
 * This is the RECEIVE half of the frame story. `wfm_frame_t`
 * (`wfm/wfm_frame.h`) is what a generator builds a frame from and what
 * `wfm_frame_crc_ok()` scores a received one against, and until now only C
 * could hold one — so `ber`'s frame meter, which exists precisely to turn CRC
 * outcomes into an exact error-rate interval, had no way to be fed from the
 * language most captures are analysed in.
 *
 * ## It owns NO layout
 *
 * Every decision — where the CRC sits, that it covers the payload alone and
 * nothing else, that a repeated preamble repeats the SAME bits — stays in
 * `wfm_frame.c`. This object is lifecycle and delegation: it copies the
 * caller's literal arrays so the descriptor outlives the call that made it,
 * materialises the frame once, and hands everything else to
 * `wfm_frame_layout()` / `wfm_frame_bits()` / `wfm_frame_crc_ok()`. Re-deriving
 * any of it here would rebuild exactly the TX/RX drift the descriptor was
 * introduced to stop.
 *
 * ## Two lengths per field, and they are not the same length
 *
 * A field is either a literal array or a handful of numbers a receiver can
 * REGENERATE. So each of the three carries both: @p preamble is the literal
 * and @p preamble_len is its extent, while @p preamble_nbits is how many bits
 * a *generated* kind should emit. `wfm_seq_t` already names this apart —
 * @p reg_bits is a register width, @p len is an output length — and conflating
 * them is the mistake that documentation exists to prevent.
 *
 * ## The frame is materialised at CREATE
 *
 * `frame_create()` builds the bits immediately and returns NULL if the
 * descriptor cannot produce them (a literal kind with no array, a PN with no
 * register width, an empty geometry). A descriptor that cannot be materialised
 * is not a frame, and finding that out at construction is what lets the
 * binding raise something better than a failure three calls later.
 *
 * @code
 * // Barker-13 sync over a 16-bit literal payload, with a CRC-16 trailer.
 * static const uint8_t sync[13]  = {1,1,1,1,1,0,0,1,1,0,1,0,1};
 * static const uint8_t pay[16]   = {0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1};
 * frame_state_t *f = frame_create(
 *     0, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   // no preamble
 *     0, sync, 13, 0, 0, 0, 0, 0, 0, 0, 0, 0,     // literal sync
 *     0, pay, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0,      // literal payload
 *     1);                                          // crc16
 * uint8_t *b = malloc(frame_bits_max_out(f, 1));
 * size_t   n = frame_bits(f, 1, b, f->nbits);      // 13 + 16 + 16 == 45
 * frame_crc_ok(f, b, n);                           // 1 — it is its own truth
 * free(b);
 * frame_destroy(f);
 * @endcode
 *
 * @see docs/design/rx-test.md section 7
 */
#ifndef FRAME_CORE_H
#define FRAME_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#include "pn/pn_core.h"
#include "gold/gold_core.h"
#include "wfm/wfm_frame.h" /* the descriptor and its layout — the one SSOT */
#include "conv/conv_core.h"
#include "rs/rs_core.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Frame state.
 *
 * Allocate with frame_create().
 */
typedef struct {
    /** The DESCRIPTION — fields and stages — which is what everything here
        delegates on. `wfm_frame_t` is one configuration of it, so the
        thirty-odd-argument constructor and the field-by-field builder produce
        the same kind of thing and share every method below. Its `bits`
        pointers address the owned copies, never the caller's arrays: a Python
        buffer is released the moment the call that supplied it returns. */
    wfm_frame_desc_t d;
    /** The general layout, derived at build. */
    wfm_frame_desc_layout_t dl;
    /** What the last frame_deframe() found, summed across the stages it
        reversed. Read-backs rather than a returned record: the call hands
        back BITS, and jm binds one return value. `rx_checked == 0` means
        the description carries no reversible stage — which is not the same
        fact as a failed check, and an FER conflating them would score every
        unprotected frame as an error. */
    int rx_checked;
    int rx_units;
    int rx_ok;
    int rx_symbols;
    /** The four-field configuration, kept only when the object was built that
        way — it is what `layout()`'s NAMED view reports. A description built
        field by field has no preamble/sync/payload/crc to name, and
        `layout()` says so by reporting a zero `total_bits` rather than
        inventing offsets for fields that do not exist. */
    wfm_frame_t f;
    /** Computed at create for the configured path; zero otherwise. */
    wfm_frame_layout_t l;
    /** Non-zero once the configured path filled `f` and `l`. */
    int named;
    /** Owned copies of every literal field; NULL for a generated kind. */
    uint8_t *own[WFM_FRAME_MAX_FIELDS];
    /** One materialised frame, built at create (configured) or at `build()`
        (described) — which is also the proof the description CAN be
        materialised. `bits()` repeats this rather than regenerating, so every
        repeat is bit-identical by construction and a PN field cannot advance
        its register between them. */
    uint8_t *one;
/*<<property_struct_fields>>*/
  size_t nbits;
} frame_state_t;

/**
 * @brief Create a frame instance.
 *
 * Each of the three fields takes the same twelve arguments: a kind, a literal
 * array with its length, a generated output length, and the PN/Gold generator
 * parameters. Only the ones the kind uses are read.
 *
 * @param preamble_kind  Enum index; 0=literal…3=dotted.
 * @param preamble  Input uint8_t array (length passed as preamble_len).
 * @param preamble_len  Literal preamble length in bits.
 * @param preamble_nbits  Output bits for a GENERATED preamble kind (default: 0).
 * @param preamble_reps  Repetitions of the preamble; 0 = no preamble (default: 0).
 * @param preamble_poly  PN feedback polynomial; 0 selects the maximal-length one (default: 0).
 * @param preamble_seed  PN seed; 0 selects 1, since an all-zero register is a fixed point (default: 0).
 * @param preamble_reg_bits  PN/Gold register width, 1..64 (default: 0).
 * @param preamble_lfsr  Enum index; 0=galois…1=fibonacci.
 * @param preamble_taps_a  Gold: first register's taps (default: 0).
 * @param preamble_seed_a  Gold: first register's seed (default: 0).
 * @param preamble_taps_b  Gold: second register's taps (default: 0).
 * @param preamble_seed_b  Gold: second register's seed (default: 0).
 * @param sync_kind  Enum index; 0=literal…3=dotted.
 * @param sync  Input uint8_t array (length passed as sync_len).
 * @param sync_len  Literal sync-word length in bits.
 * @param sync_nbits  Output bits for a GENERATED sync kind (default: 0).
 * @param sync_poly  PN feedback polynomial; 0 selects the maximal-length one (default: 0).
 * @param sync_seed  PN seed; 0 selects 1 (default: 0).
 * @param sync_reg_bits  PN/Gold register width, 1..64 (default: 0).
 * @param sync_lfsr  Enum index; 0=galois…1=fibonacci.
 * @param sync_taps_a  Gold: first register's taps (default: 0).
 * @param sync_seed_a  Gold: first register's seed (default: 0).
 * @param sync_taps_b  Gold: second register's taps (default: 0).
 * @param sync_seed_b  Gold: second register's seed (default: 0).
 * @param payload_kind  Enum index; 0=literal…3=dotted.
 * @param payload  Input uint8_t array (length passed as payload_len).
 * @param payload_len  Literal payload length in bits.
 * @param payload_nbits  Output bits for a GENERATED payload kind (default: 0).
 * @param payload_poly  PN feedback polynomial; 0 selects the maximal-length one (default: 0).
 * @param payload_seed  PN seed; 0 selects 1 (default: 0).
 * @param payload_reg_bits  PN/Gold register width, 1..64 (default: 0).
 * @param payload_lfsr  Enum index; 0=galois…1=fibonacci.
 * @param payload_taps_a  Gold: first register's taps (default: 0).
 * @param payload_seed_a  Gold: first register's seed (default: 0).
 * @param payload_taps_b  Gold: second register's taps (default: 0).
 * @param payload_seed_b  Gold: second register's seed (default: 0).
 * @param crc  Enum index; 0=none…1=crc16.
 * @return Heap-allocated state, or NULL if the geometry is empty or a field
 *         cannot be built (a literal with no array, a PN with no register
 *         width) — the descriptor is refused rather than half-honoured.
 * @note Caller must call frame_destroy() when done.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.wfm import Frame
 * >>> empty = np.empty(0, np.uint8)                    # an absent field
 * >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)   # Barker-13
 * >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
 * >>> f = Frame(empty, sync, payload, crc="crc16")
 * >>> f.nbits                                          # 13 + 16 + 16
 * 45
 * >>> f.layout().payload_off
 * 13
 * >>> f.crc_ok(f.bits())        # its own bits are its own truth
 * 1
 *
 * A payload a receiver can REGENERATE, rather than one it must be handed:
 *
 * >>> g = Frame(empty, sync, empty, payload_kind="pn",
 * ...           payload_nbits=1024, payload_reg_bits=10, crc="crc16")
 * >>> g.nbits
 * 1053
 * @endcode
 */
frame_state_t *frame_create(int preamble_kind, const uint8_t *preamble, size_t preamble_len, size_t preamble_nbits, size_t preamble_reps, uint64_t preamble_poly, uint64_t preamble_seed, uint32_t preamble_reg_bits, int preamble_lfsr, uint64_t preamble_taps_a, uint64_t preamble_seed_a, uint64_t preamble_taps_b, uint64_t preamble_seed_b, int sync_kind, const uint8_t *sync, size_t sync_len, size_t sync_nbits, uint64_t sync_poly, uint64_t sync_seed, uint32_t sync_reg_bits, int sync_lfsr, uint64_t sync_taps_a, uint64_t sync_seed_a, uint64_t sync_taps_b, uint64_t sync_seed_b, int payload_kind, const uint8_t *payload, size_t payload_len, size_t payload_nbits, uint64_t payload_poly, uint64_t payload_seed, uint32_t payload_reg_bits, int payload_lfsr, uint64_t payload_taps_a, uint64_t payload_seed_a, uint64_t payload_taps_b, uint64_t payload_seed_b, int crc);

/**
 * @brief Destroy a frame instance and release all memory.
 * @param state  May be NULL.
 */
void frame_destroy(frame_state_t *state);

/**
 * @brief Bits @ref frame_bits will write for @p n frames — `n * nbits`.
 *
 * @param state  The frame.
 * @param n      Frame repetitions.
 */
size_t frame_bits_max_out(frame_state_t *state, size_t n);

/**
 * @brief Materialise @p n consecutive frames, one bit per byte.
 *
 * @p n counts FRAMES, not bits: a descriptor describes one frame, and a
 * capture holds many. Repeating here rather than making the caller tile it is
 * what matches the generator, whose framed source cycles the same frame to
 * fill whatever length was asked for — so a stream compared against this
 * lines up with the one that was transmitted.
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
 * >>> empty = np.empty(0, np.uint8)
 * >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
 * >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
 * >>> d = FrameDesc(empty, sync, payload, crc="crc16")
 * >>> d.build()
 * >>> len(d.bits())        # one frame: 13 + 16 + 16
 * 45
 * >>> len(d.bits(2))       # n counts FRAMES, tiled the way a capture is
 * 90
 *
 * @endcode
 */
size_t frame_bits(frame_state_t *state, size_t n, uint8_t *out, size_t max_out);

/**
 * @brief Where each field lands, in bits from the start of the frame.
 *
 * The offsets a receiver needs to slice a capture, computed by the same code
 * the generator laid the frame out with.
 *
 * @param state  The frame.
 * @return Where each named field lands.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.wfm import Frame
 * >>> empty = np.empty(0, np.uint8)
 * >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
 * >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
 * >>> lay = Frame(empty, sync, payload, crc="crc16").layout()
 * >>> lay.sync_off, lay.payload_off, lay.crc_off
 * (0, 13, 29)
 * >>> lay.total_bits
 * 45
 *
 * This is the NAMED view, so it reports the four fields a `Frame` is built
 * from. A description assembled with `add_field` reports zeros here and is
 * read with `field_off()` / `field_bits()` instead.
 *
 * @endcode
 */
wfm_frame_layout_t frame_layout(frame_state_t *state);

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
 * @param rx_bits_len  How many; must be at least @ref frame_state_t::nbits.
 * @return 1 pass, 0 fail, -1 if the frame carries no CRC or @p rx_bits is
 *         shorter than one frame.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.wfm import FrameDesc
 * >>> empty = np.empty(0, np.uint8)
 * >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
 * >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
 * >>> d = FrameDesc(empty, sync, payload, crc="crc16")
 * >>> d.build()
 * >>> d.crc_ok(d.bits())           # its own bits are its own truth
 * 1
 * >>> rx = np.asarray(d.bits()).copy()
 * >>> rx[d.field_off(2)] ^= 1      # flip one payload bit
 * >>> d.crc_ok(rx)
 * 0
 *
 * @endcode
 */
int frame_crc_ok(frame_state_t *state, const uint8_t *rx_bits, size_t rx_bits_len);

/**
 * @brief The same frame, DEFERRED — a description a caller can extend.
 *
 * Every argument @ref frame_create takes, and the flavor is what it does with
 * them: this one stops before materialising, so the four fields are a
 * STARTING POINT rather than a finished frame. Append with
 * @ref frame_add_field and @ref frame_add_stage, then @ref frame_build.
 * Pass empty arrays for all three to begin from nothing.
 *
 * That is what makes a frame doppler has never heard of describable — a
 * CCSDS CADU among them — without a constructor argument per field of a fixed
 * list. The thirty-odd arguments both constructors take exist because a field
 * count baked into a prototype forces every field's every parameter into it;
 * appending is how a fifth field is added without a signature change.
 *
 * It is also what makes the CCSDS coding reachable from Python at all.
 * `ccsds_tm` has no binding and is not getting one, so a caller meets the
 * outer code, the randomiser and the inner code by DESCRIBING a CADU rather
 * than through a CCSDS entry point added here.
 *
 * An empty description is legal here and refused by @ref frame_create, and
 * the difference is where completeness can be judged: that constructor's
 * description is complete when it returns, and this one is not complete until
 * @ref frame_build is called.
 *
 * @ref frame_layout's NAMED view reports nothing for a description, on
 * purpose — it would go stale the moment a fifth field is appended, and a
 * stale offset is worse than an absent one. Read a description through
 * @ref frame_field_off and its siblings.
 *
 * @return An unbuilt description, or NULL on allocation failure or a field
 *         that cannot be copied.
 */
frame_state_t *frame_create_desc(int preamble_kind, const uint8_t *preamble, size_t preamble_len, size_t preamble_nbits, size_t preamble_reps, uint64_t preamble_poly, uint64_t preamble_seed, uint32_t preamble_reg_bits, int preamble_lfsr, uint64_t preamble_taps_a, uint64_t preamble_seed_a, uint64_t preamble_taps_b, uint64_t preamble_seed_b, int sync_kind, const uint8_t *sync, size_t sync_len, size_t sync_nbits, uint64_t sync_poly, uint64_t sync_seed, uint32_t sync_reg_bits, int sync_lfsr, uint64_t sync_taps_a, uint64_t sync_seed_a, uint64_t sync_taps_b, uint64_t sync_seed_b, int payload_kind, const uint8_t *payload, size_t payload_len, size_t payload_nbits, uint64_t payload_poly, uint64_t payload_seed, uint32_t payload_reg_bits, int payload_lfsr, uint64_t payload_taps_a, uint64_t payload_seed_a, uint64_t payload_taps_b, uint64_t payload_seed_b, int crc);

/**
 * @brief Append one field to a description.
 *
 * Either the caller supplies the bits (@p lit, or a generated kind) or a
 * stage derives them (@p derived_by non-zero). Both are fields, because both
 * are on the wire.
 *
 * @param state        A frame from @ref frame_create_desc.
 * @param lit          Literal bits, copied here so the description outlives
 *                     the call; may be NULL.
 * @param lit_len      Length of @p lit in bits.
 * @param kind         @ref wfm_seq_kind_t index; 0=literal…3=dotted.
 * @param gen_len      Output bits for a GENERATED kind.
 * @param reps         Repetitions of the field, verbatim; 0 means one.
 * @param poly         PN feedback polynomial; 0 selects the maximal-length.
 * @param seed         PN seed; 0 selects 1.
 * @param reg_bits     PN/Gold register width.
 * @param lfsr         0=galois, 1=fibonacci.
 * @param taps_a       Gold: first register's taps.
 * @param seed_a       Gold: first register's seed.
 * @param taps_b       Gold: second register's taps.
 * @param seed_b       Gold: second register's seed.
 * @param derived_by   0 when the caller supplies this field; otherwise the
 *                     index of the producing stage, PLUS ONE.
 * @param derived_bits Length of a derived field, in bits.
 * @return The new field's index, or -1 if the description is full, already
 *         built, or the literal could not be copied.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.wfm import FrameDesc, ccsds_asm_bits
 * >>> empty = np.empty(0, np.uint8)
 * >>> asm = ccsds_asm_bits()
 * >>> octets = np.array([(i * 29 + 5) & 0xFF for i in range(223)],
 * ...                   np.uint8)
 * >>> data = np.unpackbits(octets).astype(np.uint8)
 * >>> d = FrameDesc(empty, empty, empty)   # begin from nothing
 * >>> d.add_field(asm)                     # the attached sync marker
 * 0
 * >>> d.add_field(data)                    # the transfer frame
 * 1
 *
 * A field the CALLER does not supply is still a field, because it is still
 * on the wire -- `derived_by` names the stage that fills it, PLUS ONE:
 *
 * >>> d.add_field(empty, derived_by=1, derived_bits=32 * 8)
 * 2
 *
 * @endcode
 */
int frame_add_field(frame_state_t *state, const uint8_t *lit, size_t lit_len,
                    int kind, size_t gen_len, size_t reps, uint64_t poly,
                    uint64_t seed, uint32_t reg_bits, int lfsr,
                    uint64_t taps_a, uint64_t seed_a, uint64_t taps_b,
                    uint64_t seed_b, uint32_t derived_by,
                    size_t derived_bits);

/**
 * @brief Append one stage, and the span of fields it covers.
 *
 * @p n_fields is the load-bearing part and 0 means the stage does not run.
 * A stage that inherited "everything before me" instead of declaring its
 * cover is the representation that cannot express a CCSDS CADU — see
 * `wfm/wfm_frame.h`.
 *
 * @param state        A frame from @ref frame_create_desc.
 * @param kind         @ref wfm_stage_kind_t index; 0=crc16…4=interleave.
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
 *         built.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.wfm import FrameDesc, ccsds_asm_bits
 * >>> empty = np.empty(0, np.uint8)
 * >>> asm = ccsds_asm_bits()
 * >>> octets = np.array([(i * 29 + 5) & 0xFF for i in range(223)],
 * ...                   np.uint8)
 * >>> data = np.unpackbits(octets).astype(np.uint8)
 * >>> d = FrameDesc(empty, empty, empty)
 * >>> _ = d.add_field(asm), d.add_field(data)
 * >>> _ = d.add_field(empty, derived_by=1, derived_bits=32 * 8)
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
int frame_add_stage(frame_state_t *state, int kind, uint32_t first_field,
                    uint32_t n_fields, uint32_t depth, uint32_t emit_num,
                    uint32_t emit_den, uint32_t unit_bits);

/**
 * @brief Lay out and materialise a described frame.
 *
 * The point at which a description is checked, which for @ref frame_create
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
 * The inner encoder starts from the all-zero register on every build: a
 * description describes ONE frame. A stream of CADUs sharing one register is
 * a transmitter's job and lives in `ccsds_tm_frame_encode`.
 *
 * @param state  A frame from @ref frame_create_desc.
 * @return 0 on success, -1 if the description is empty, unbuildable, names a
 *         stage with no kernel here, or was already built.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.wfm import FrameDesc
 * >>> empty = np.empty(0, np.uint8)
 * >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
 * >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
 * >>> d = FrameDesc(empty, sync, payload, crc="crc16")
 * >>> d.build()
 * >>> d.nbits                     # 13 + 16 + 16, laid out by build()
 * 45
 *
 * A description that cannot produce bits is not a frame, and is refused
 * rather than half-built:
 *
 * >>> FrameDesc(empty, empty, empty).build()
 * Traceback (most recent call last):
 *     ...
 * ValueError: build failed (rc=-1)
 *
 * @endcode
 */
int frame_build(frame_state_t *state);

/**
 * @brief What @ref frame_check found, summed across the stages it reversed.
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
 * The receive mirror of @ref frame_bits, reading the same description — so a
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
 * >>> empty = np.empty(0, np.uint8)
 * >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
 * >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
 * >>> d = FrameDesc(empty, sync, payload, crc="crc16")
 * >>> d.build()
 * >>> r = d.check(d.bits(1))
 * >>> r.passed, r.ok, r.units
 * (1, 1, 1)
 *
 * Flip a bit the CRC covers and the verdict turns over:
 *
 * >>> rx = np.asarray(d.bits(1)).copy()
 * >>> rx[d.field_off(2)] ^= 1
 * >>> d.check(rx).passed
 * 0
 *
 * Carrying no check is NOT passing one -- both are reported, separately:
 *
 * >>> n = FrameDesc(empty, sync, payload, crc="none")
 * >>> n.build()
 * >>> c = n.check(n.bits(1))
 * >>> c.passed, c.checked
 * (0, 0)
 *
 * @endcode
 */
frame_check_t frame_check(frame_state_t *state, const uint8_t *rx_bits, size_t rx_bits_len);

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
 * The payload is then a slice, at @ref frame_field_off of the payload
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
 * @param max_out      Capacity of @p out; see frame_deframe_max_out().
 * @return Bits written — the frame's length — or 0 if the description is
 *         empty or either buffer is too small.
 * @code
 * >>> import numpy as np
 * >>> from doppler.wfm import Frame
 * >>> empty = np.zeros(0, dtype=np.uint8)
 * >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], dtype=np.uint8)
 * >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], dtype=np.uint8)
 * >>> f = Frame(empty, sync, payload, crc="crc16")
 * >>> rx = np.asarray(f.bits())          # a clean capture of its own frame
 * >>> got = np.asarray(f.deframe(rx))
 * >>> f.rx_ok, f.rx_units, f.rx_checked  # one CRC, and it passed
 * (1, 1, 1)
 * >>> off = f.layout().payload_off       # the payload is a SLICE
 * >>> bool(np.array_equal(got[off:off + 16], payload))
 * True
 * >>> rx[off] ^= 1                       # one bit flipped in flight
 * >>> _ = f.deframe(rx)
 * >>> f.rx_ok, f.rx_units                # the check notices
 * (0, 1)
 *
 * @endcode
 */
size_t frame_deframe(frame_state_t *state, const uint8_t *rx_bits, size_t rx_bits_len, uint8_t *out, size_t max_out);

/**
 * @brief Max bits frame_deframe() writes: the frame's own length.
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
size_t frame_deframe_max_out(frame_state_t *state, size_t rx_bits_len);


/**
 * @brief Fields in the description.
 *
 * @param state  The frame.
 * @return How many fields the description carries.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.wfm import FrameDesc
 * >>> empty = np.empty(0, np.uint8)
 * >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
 * >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
 * >>> d = FrameDesc(empty, sync, payload, crc="crc16")
 * >>> d.n_fields()          # the four named fields, absent ones included
 * 4
 *
 * @endcode
 */
size_t frame_n_fields(frame_state_t *state);

/**
 * @brief Stages in the description.
 *
 * @param state  The frame.
 * @return How many stages the description carries.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.wfm import FrameDesc
 * >>> empty = np.empty(0, np.uint8)
 * >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
 * >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
 * >>> d = FrameDesc(empty, sync, payload, crc="crc16")
 * >>> d.build()
 * >>> d.n_stages()         # the CRC is a stage like any other
 * 1
 *
 * @endcode
 */
size_t frame_n_stages(frame_state_t *state);

/**
 * @brief Bit offset of field @p i, or 0 if there is no such field.
 * @param state  The frame.
 * @param i      Field index.
 * @return Bits from the start of the frame.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.wfm import FrameDesc
 * >>> empty = np.empty(0, np.uint8)
 * >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
 * >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
 * >>> d = FrameDesc(empty, sync, payload, crc="crc16")
 * >>> d.build()
 * >>> d.field_off(1), d.field_off(2), d.field_off(3)
 * (0, 13, 29)
 *
 * Field 0 is the absent preamble: an empty field still HAS an index, so the
 * indices a caller passed to `add_field` keep meaning what they meant.
 *
 * >>> d.field_off(0), d.field_bits(0)
 * (0, 0)
 *
 * @endcode
 */
size_t frame_field_off(frame_state_t *state, size_t i);

/**
 * @brief Bits in field @p i, or 0 if there is no such field.
 * @param state  The frame.
 * @param i      Field index.
 * @return The field's length in bits.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.wfm import FrameDesc
 * >>> empty = np.empty(0, np.uint8)
 * >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
 * >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
 * >>> d = FrameDesc(empty, sync, payload, crc="crc16")
 * >>> d.build()
 * >>> d.field_bits(1), d.field_bits(2), d.field_bits(3)
 * (13, 16, 16)
 *
 * @endcode
 */
size_t frame_field_bits(frame_state_t *state, size_t i);

/**
 * @brief First CADU bit stage @p i covers; 0 for a stage that did not run.
 * @param state  The frame.
 * @param i      Stage index.
 * @return Bits from the start of the frame.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.wfm import FrameDesc
 * >>> empty = np.empty(0, np.uint8)
 * >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
 * >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
 * >>> d = FrameDesc(empty, sync, payload, crc="crc16")
 * >>> d.build()
 * >>> d.stage_first(0)     # the CRC starts at the payload, not at bit 0
 * 13
 *
 * @endcode
 */
size_t frame_stage_first(frame_state_t *state, size_t i);

/**
 * @brief Bits stage @p i covers; 0 for a stage that did not run.
 * @param state  The frame.
 * @param i      Stage index.
 * @return The covered span, in bits.
 *
 * @code
 * >>> import numpy as np
 * >>> from doppler.wfm import FrameDesc
 * >>> empty = np.empty(0, np.uint8)
 * >>> sync = np.array([1,1,1,1,1,0,0,1,1,0,1,0,1], np.uint8)
 * >>> payload = np.array([0,1,1,0,1,0,0,1,1,1,0,0,0,1,0,1], np.uint8)
 * >>> d = FrameDesc(empty, sync, payload, crc="crc16")
 * >>> d.build()
 * >>> d.stage_bits(0)      # payload+CRC: what crc16 covered
 * 32
 *
 * @endcode
 */
size_t frame_stage_bits(frame_state_t *state, size_t i);

#ifdef __cplusplus
}
#endif

#endif /* FRAME_CORE_H */
