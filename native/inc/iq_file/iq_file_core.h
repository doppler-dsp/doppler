/**
 * @file iq_file_core.h
 * @brief IqFile component API.
 *
 * Lifecycle: create -> [step / steps / reset]* -> destroy
 *
 * Example:
 * @code
 * iq_file_state_t *obj = iq_file_create(NULL);
 * float complex y = iq_file_step(obj, 0.0f + 0.0f * I);
 * iq_file_destroy(obj);
 * @endcode
 */
#ifndef IQ_FILE_CORE_H
#define IQ_FILE_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief IqFile state.
 *
 * Allocate with iq_file_create().
 */
typedef struct {
    int fd;
    size_t position;
    size_t nsamples;
    int sample_type;
    int endian;
} iq_file_state_t;

/**
 * @brief Create a iq_file instance.
 *
 * @param filepath  filepath (required).
 * @param sample_type  Enum index; 0=cf32…4=ci8.
 * @param endian  Enum index; 0=le…1=be.
 * @return Heap-allocated state, or NULL on allocation failure.
 * @note Caller must call iq_file_destroy() when done.
 */
iq_file_state_t *iq_file_create(const char * filepath, int sample_type, int endian);

/**
 * @brief Destroy a iq_file instance and release all memory.
 * @param state  May be NULL.
 */
void iq_file_destroy(iq_file_state_t *state);

/**
 * @brief Reset IqFile to its post-create state.
 * @param state  Must be non-NULL.
 */
void iq_file_reset(iq_file_state_t *state);





/**
 * @brief Get current fd.
 * @param state  Must be non-NULL.
 */
int iq_file_get_fd(const iq_file_state_t *state);

/**
 * @brief Set fd.
 * @param state  Must be non-NULL.
 * @param val    New value.
 */
void iq_file_set_fd(iq_file_state_t *state, int val);

/**
 * @brief Get current position.
 * @param state  Must be non-NULL.
 */
size_t iq_file_get_position(const iq_file_state_t *state);

/**
 * @brief Set position.
 * @param state  Must be non-NULL.
 * @param val    New value.
 */
void iq_file_set_position(iq_file_state_t *state, size_t val);

/**
 * @brief Get current nsamples.
 * @param state  Must be non-NULL.
 */
size_t iq_file_get_nsamples(const iq_file_state_t *state);

/**
 * @brief Set nsamples.
 * @param state  Must be non-NULL.
 * @param val    New value.
 */
void iq_file_set_nsamples(iq_file_state_t *state, size_t val);

/**
 * @brief Get current sample_type.
 * @param state  Must be non-NULL.
 */
int iq_file_get_sample_type(const iq_file_state_t *state);

/**
 * @brief Set sample_type.
 * @param state  Must be non-NULL.
 * @param val    New value.
 */
void iq_file_set_sample_type(iq_file_state_t *state, int val);

/**
 * @brief Get current endian.
 * @param state  Must be non-NULL.
 */
int iq_file_get_endian(const iq_file_state_t *state);

/**
 * @brief Set endian.
 * @param state  Must be non-NULL.
 * @param val    New value.
 */
void iq_file_set_endian(iq_file_state_t *state, int val);



float complex iq_file_read(iq_file_state_t *state, size_t n, float complex *out);
float complex iq_file_close(iq_file_state_t *state);
#ifdef __cplusplus
}
#endif

#endif /* IQ_FILE_CORE_H */
