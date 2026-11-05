/*
 * Copyright © Weaselway contributors
 * SPDX-License-Identifier: MIT
 */

#ifndef D3DKMT_PRIORITY_H
#define D3DKMT_PRIORITY_H

#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif

/* GPU scheduling priority of the dxgkrnl contexts that D3D12 creates on WSL.
 *
 * D3D12 command queue priorities have no effect on WSL, but the kernel
 * context priority does (D3DKMTSetContextSchedulingPriority, -7..7 without
 * privileges). Raising our own context does nothing measurable, lowering
 * everybody else does. So every context starts at MEDIUM (-3), and only
 * clients that ask for HIGH priority (compositors) stay at 0.
 */
#define D3DKMT_PRIORITY_LOW    -7
#define D3DKMT_PRIORITY_MEDIUM -3
#define D3DKMT_PRIORITY_HIGH    0
#define D3DKMT_PRIORITY_NONE    INT_MIN

#ifdef __linux__

/* Hooks the context creation of libd3d12core.so. Call it before the first
 * D3D12 device is created. Does nothing when libd3d12core.so is missing.
 */
void d3dkmt_priority_init(void);

/* Requests a priority for all contexts of the process that were not created
 * with a pending priority. The process uses the highest priority requested so
 * far, and MEDIUM until the first request.
 */
void d3dkmt_priority_request(int priority);

/* Contexts created by the calling thread get this priority and ignore
 * process requests. Pass D3DKMT_PRIORITY_NONE to reset it.
 */
void d3dkmt_priority_set_pending(int priority);

#else

static inline void d3dkmt_priority_init(void) {}
static inline void d3dkmt_priority_request(int priority) {}
static inline void d3dkmt_priority_set_pending(int priority) {}

#endif

#ifdef __cplusplus
}
#endif

#endif
