/*
 * Copyright © Weaselway contributors
 * SPDX-License-Identifier: MIT
 */

#ifndef WSL_DXGDRM_H
#define WSL_DXGDRM_H

#ifdef __cplusplus
extern "C" {
#endif

/* Open the dxgdrm render node if the module is loaded and its version is at
 * least 1.min_minor. Returns -1 when there is none, which is not an error.
 */
int wsl_open_dxgdrm_node(int min_minor);

#ifdef __cplusplus
}
#endif

#endif
