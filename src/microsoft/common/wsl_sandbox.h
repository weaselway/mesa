/*
 * Copyright © Weaselway contributors
 * SPDX-License-Identifier: MIT
 */

#ifndef WSL_SANDBOX_H
#define WSL_SANDBOX_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __linux__

/* Lets D3D12 run in a Flatpak sandbox, which has neither /dev/dxg nor WSL's
 * files in /usr/lib/wsl. Call it before libd3d12.so or libdxcore.so is loaded. Does nothing
 * unless Mesa runs from a GL extension with a wsl/ directory next to its
 * lib/ directory.
 */
void wsl_sandbox_init(void);

#else

static inline void wsl_sandbox_init(void) {}

#endif

#ifdef __cplusplus
}
#endif

#endif
