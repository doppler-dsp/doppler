/**
 * @file f32_buffer_core.h
 * @brief The component header jm's binding includes.
 *
 * Under `header_only = "true"` a component's C lives entirely in its header
 * and jm scaffolds no `_core.c`. This is that header, and it is the file the
 * AUTHOR owns -- jm creates it once and never rewrites it, the same contract
 * `_core.c` has for an ordinary component.
 *
 * For the ring there is nothing to write: everything the binding calls is
 * either the ring itself or one of the siblings this proposal asks for, so
 * this is a single include. It exists because the generated fragment says
 * `#include "f32_buffer/f32_buffer_core.h"` and a probe without it cannot
 * compile -- which is how the first version of the probe came to reference
 * files that are not in the tree.
 */
#ifndef F32_BUFFER_CORE_H
#define F32_BUFFER_CORE_H

#include "proposed-siblings.h"

#endif /* F32_BUFFER_CORE_H */
