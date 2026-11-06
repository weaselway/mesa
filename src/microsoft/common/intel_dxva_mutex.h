/*
 * Copyright © Weaselway contributors
 * SPDX-License-Identifier: MIT
 */

#ifndef INTEL_DXVA_MUTEX_H
#define INTEL_DXVA_MUTEX_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __linux__

/* Makes the mutexes that Intel's WSL video driver creates from now on
 * recursive. Call it after the D3D12 device exists and before creating a
 * video decoder, encoder or processor. Does nothing without that driver.
 */
void intel_dxva_mutex_fix(void);

#else

static inline void intel_dxva_mutex_fix(void) {}

#endif

#ifdef __cplusplus
}
#endif

#endif
